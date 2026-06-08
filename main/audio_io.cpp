/**
 * @file audio_io.cpp
 * @brief I2S audio capture/playback implementation.
 */

#include "audio_io.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#ifndef CONFIG_ROBOMIND_ENABLE_AUDIO
#define CONFIG_ROBOMIND_ENABLE_AUDIO 0
#endif

static const char* TAG = "audio_io";

namespace {
constexpr i2s_port_t kRxPort = I2S_NUM_0;
constexpr i2s_port_t kTxPort = I2S_NUM_1;
constexpr size_t kCaptureBufferCount = 2;
constexpr size_t kPlayChunkSamples = 512;
constexpr uint32_t kCaptureTaskStack = 4096;
constexpr UBaseType_t kCaptureTaskPriority = 1;

i2s_chan_handle_t s_rx_channel = nullptr;
i2s_chan_handle_t s_tx_channel = nullptr;
TaskHandle_t s_capture_task = nullptr;
int16_t* s_capture_buffers[kCaptureBufferCount] = {nullptr, nullptr};
size_t s_capture_buffer_samples = 0;
int s_tx_sample_rate = 0;
bool s_rx_enabled = false;
bool s_tx_enabled = false;

size_t CalculateBufferSamples(int sample_rate, int buffer_ms) {
    const int safe_ms = std::max(buffer_ms, 1);
    const int safe_rate = std::max(sample_rate, 1);
    const size_t samples = static_cast<size_t>(safe_rate) * safe_ms / 1000;
    return std::max<size_t>(samples, kPlayChunkSamples);
}

i2s_std_config_t MakeStdConfig(int sample_rate, gpio_num_t bck, gpio_num_t ws,
                               gpio_num_t dout, gpio_num_t din) {
    i2s_std_config_t config = {};
    config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    // I2S standard mode is the new-driver equivalent of I2S_COMM_FORMAT_STAND_I2S.
    config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                          I2S_SLOT_MODE_MONO);
    config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    config.gpio_cfg.bclk = bck;
    config.gpio_cfg.ws = ws;
    config.gpio_cfg.dout = dout;
    config.gpio_cfg.din = din;
    config.gpio_cfg.invert_flags.mclk_inv = false;
    config.gpio_cfg.invert_flags.bclk_inv = false;
    config.gpio_cfg.invert_flags.ws_inv = false;
    return config;
}
}

AudioIO* AudioIO::GetInstance() {
    static AudioIO instance;
    return &instance;
}

AudioIO::~AudioIO() {
    ReleaseAudioResources();
}

bool AudioIO::Initialize() {
#if CONFIG_ROBOMIND_ENABLE_AUDIO
    if (s_rx_channel && s_capture_task) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing I2S RX: BCK=%d WS=%d DIN=%d rate=%dHz buffer=%dms",
             CONFIG_ROBOMIND_AUDIO_I2S_BCK, CONFIG_ROBOMIND_AUDIO_I2S_WS,
             CONFIG_ROBOMIND_AUDIO_I2S_DIN, CONFIG_ROBOMIND_AUDIO_SAMPLE_RATE,
             CONFIG_ROBOMIND_AUDIO_BUFFER_MS);

    if (!AllocateCaptureBuffers()) {
        ReleaseAudioResources();
        return false;
    }

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(kRxPort, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&channel_config, nullptr, &s_rx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S RX channel create failed: %s", esp_err_to_name(ret));
        ReleaseAudioResources();
        return false;
    }

    i2s_std_config_t std_config = MakeStdConfig(
        CONFIG_ROBOMIND_AUDIO_SAMPLE_RATE,
        static_cast<gpio_num_t>(CONFIG_ROBOMIND_AUDIO_I2S_BCK),
        static_cast<gpio_num_t>(CONFIG_ROBOMIND_AUDIO_I2S_WS),
        I2S_GPIO_UNUSED,
        static_cast<gpio_num_t>(CONFIG_ROBOMIND_AUDIO_I2S_DIN));

    ret = i2s_channel_init_std_mode(s_rx_channel, &std_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S RX std init failed: %s", esp_err_to_name(ret));
        ReleaseAudioResources();
        return false;
    }

    ret = i2s_channel_enable(s_rx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S RX enable failed: %s", esp_err_to_name(ret));
        ReleaseAudioResources();
        return false;
    }
    s_rx_enabled = true;

    BaseType_t task_ret = xTaskCreate(&AudioIO::CaptureTask, "audio_cap",
                                      kCaptureTaskStack, this,
                                      kCaptureTaskPriority, &s_capture_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "audio_cap task create failed");
        ReleaseAudioResources();
        return false;
    }

    state_ = State::kIdle;
    ESP_LOGI(TAG, "I2S audio capture initialized");
    return true;
#else
    ESP_LOGW(TAG, "Audio disabled by CONFIG_ROBOMIND_ENABLE_AUDIO");
    return true;
#endif
}

bool AudioIO::StartRecording() {
#if CONFIG_ROBOMIND_ENABLE_AUDIO
    if (!s_rx_channel || !s_capture_task) {
        ESP_LOGE(TAG, "StartRecording failed: audio RX is not initialized");
        state_ = State::kError;
        return false;
    }

    size_t bytes_read = 0;
    do {
        bytes_read = 0;
        esp_err_t ret = i2s_channel_read(s_rx_channel, s_capture_buffers[0],
                                         s_capture_buffer_samples * sizeof(int16_t),
                                         &bytes_read, 0);
        if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "I2S RX drain failed: %s", esp_err_to_name(ret));
            state_ = State::kError;
            return false;
        }
    } while (bytes_read > 0);

    state_ = State::kRecording;
    xTaskNotifyGive(s_capture_task);
    return true;
#else
    ESP_LOGE(TAG, "StartRecording failed: audio is disabled");
    return false;
#endif
}

void AudioIO::StopRecording() {
#if CONFIG_ROBOMIND_ENABLE_AUDIO
    if (state_ == State::kRecording) {
        state_ = State::kIdle;
    }
    if (s_capture_task) {
        xTaskNotifyGive(s_capture_task);
    }
#else
    state_ = State::kIdle;
#endif
}

bool AudioIO::PlayPcm(const int16_t* samples, size_t num_samples, int sample_rate) {
#if CONFIG_ROBOMIND_ENABLE_AUDIO
    if (!samples || num_samples == 0 || sample_rate <= 0) {
        ESP_LOGE(TAG, "PlayPcm failed: invalid arguments");
        return false;
    }

    const State previous_state = state_;
    if (previous_state == State::kRecording) {
        StopRecording();
    }

    if (!InitTxChannel(sample_rate)) {
        state_ = State::kError;
        return false;
    }

    state_ = State::kPlaying;
    size_t offset = 0;
    while (offset < num_samples) {
        const size_t chunk_samples = std::min(kPlayChunkSamples, num_samples - offset);
        const uint8_t* write_ptr = reinterpret_cast<const uint8_t*>(samples + offset);
        size_t remaining_bytes = chunk_samples * sizeof(int16_t);

        while (remaining_bytes > 0) {
            size_t bytes_written = 0;
            esp_err_t ret = i2s_channel_write(s_tx_channel, write_ptr, remaining_bytes,
                                              &bytes_written, portMAX_DELAY);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S TX write failed: %s", esp_err_to_name(ret));
                state_ = State::kError;
                return false;
            }
            write_ptr += bytes_written;
            remaining_bytes -= bytes_written;
        }
        offset += chunk_samples;
    }

    state_ = State::kIdle;
    return true;
#else
    ESP_LOGE(TAG, "PlayPcm failed: audio is disabled");
    return false;
#endif
}

void AudioIO::SetRecordCallback(RecordCallback callback) {
    record_callback_ = std::move(callback);
}

void AudioIO::CaptureTask(void* arg) {
    auto* self = static_cast<AudioIO*>(arg);
    if (!self) {
        ESP_LOGE(TAG, "CaptureTask started with null AudioIO");
        vTaskDelete(nullptr);
        return;
    }
    self->CaptureLoop();
}

void AudioIO::CaptureLoop() {
    size_t buffer_index = 0;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (state_ == State::kRecording) {
            size_t bytes_read = 0;
            esp_err_t ret = i2s_channel_read(s_rx_channel, s_capture_buffers[buffer_index],
                                             s_capture_buffer_samples * sizeof(int16_t),
                                             &bytes_read, pdMS_TO_TICKS(CONFIG_ROBOMIND_AUDIO_BUFFER_MS));
            if (ret == ESP_ERR_TIMEOUT) {
                continue;
            }
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S RX read failed: %s", esp_err_to_name(ret));
                state_ = State::kError;
                break;
            }

            if (bytes_read > 0 && record_callback_) {
                record_callback_(s_capture_buffers[buffer_index], bytes_read / sizeof(int16_t),
                                 CONFIG_ROBOMIND_AUDIO_SAMPLE_RATE);
            }
            buffer_index = (buffer_index + 1) % kCaptureBufferCount;
        }
    }
}

bool AudioIO::InitTxChannel(int sample_rate) {
    if (s_tx_channel && s_tx_sample_rate == sample_rate) {
        return true;
    }

    if (s_tx_channel) {
        if (s_tx_enabled) {
            esp_err_t ret = i2s_channel_disable(s_tx_channel);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S TX disable before reconfig failed: %s", esp_err_to_name(ret));
                return false;
            }
            s_tx_enabled = false;
        }
        esp_err_t ret = i2s_del_channel(s_tx_channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S TX delete before reconfig failed: %s", esp_err_to_name(ret));
            return false;
        }
        s_tx_channel = nullptr;
        s_tx_sample_rate = 0;
    }

    ESP_LOGI(TAG, "Initializing I2S TX: BCK=%d WS=%d DOUT=%d rate=%dHz",
             CONFIG_ROBOMIND_AUDIO_I2S_BCK, CONFIG_ROBOMIND_AUDIO_I2S_WS,
             CONFIG_ROBOMIND_AUDIO_I2S_DOUT, sample_rate);

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(kTxPort, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&channel_config, &s_tx_channel, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S TX channel create failed: %s", esp_err_to_name(ret));
        s_tx_channel = nullptr;
        return false;
    }

    i2s_std_config_t std_config = MakeStdConfig(
        sample_rate,
        static_cast<gpio_num_t>(CONFIG_ROBOMIND_AUDIO_I2S_BCK),
        static_cast<gpio_num_t>(CONFIG_ROBOMIND_AUDIO_I2S_WS),
        static_cast<gpio_num_t>(CONFIG_ROBOMIND_AUDIO_I2S_DOUT),
        I2S_GPIO_UNUSED);

    ret = i2s_channel_init_std_mode(s_tx_channel, &std_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S TX std init failed: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_channel);
        s_tx_channel = nullptr;
        return false;
    }

    ret = i2s_channel_enable(s_tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S TX enable failed: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_channel);
        s_tx_channel = nullptr;
        return false;
    }

    s_tx_enabled = true;
    s_tx_sample_rate = sample_rate;
    return true;
}

bool AudioIO::AllocateCaptureBuffers() {
    s_capture_buffer_samples = CalculateBufferSamples(CONFIG_ROBOMIND_AUDIO_SAMPLE_RATE,
                                                       CONFIG_ROBOMIND_AUDIO_BUFFER_MS);
    const size_t buffer_bytes = s_capture_buffer_samples * sizeof(int16_t);
    for (size_t i = 0; i < kCaptureBufferCount; ++i) {
        if (!s_capture_buffers[i]) {
            s_capture_buffers[i] = static_cast<int16_t*>(heap_caps_malloc(buffer_bytes,
                                                                           MALLOC_CAP_SPIRAM));
        }
        if (!s_capture_buffers[i]) {
            ESP_LOGE(TAG, "capture buffer %u allocation failed (%u bytes)",
                     static_cast<unsigned>(i), static_cast<unsigned>(buffer_bytes));
            return false;
        }
        std::memset(s_capture_buffers[i], 0, buffer_bytes);
    }
    return true;
}

void AudioIO::ReleaseAudioResources() {
    if (s_capture_task) {
        vTaskDelete(s_capture_task);
        s_capture_task = nullptr;
    }

    if (s_rx_channel) {
        if (s_rx_enabled) {
            esp_err_t ret = i2s_channel_disable(s_rx_channel);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S RX disable failed: %s", esp_err_to_name(ret));
            }
            s_rx_enabled = false;
        }
        esp_err_t ret = i2s_del_channel(s_rx_channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S RX delete failed: %s", esp_err_to_name(ret));
        }
        s_rx_channel = nullptr;
    }

    if (s_tx_channel) {
        if (s_tx_enabled) {
            esp_err_t ret = i2s_channel_disable(s_tx_channel);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S TX disable failed: %s", esp_err_to_name(ret));
            }
            s_tx_enabled = false;
        }
        esp_err_t ret = i2s_del_channel(s_tx_channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S TX delete failed: %s", esp_err_to_name(ret));
        }
        s_tx_channel = nullptr;
        s_tx_sample_rate = 0;
    }

    for (auto*& buffer : s_capture_buffers) {
        if (buffer) {
            heap_caps_free(buffer);
            buffer = nullptr;
        }
    }
    s_capture_buffer_samples = 0;
    state_ = State::kIdle;
}