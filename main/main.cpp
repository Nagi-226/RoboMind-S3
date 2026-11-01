/**
 * @file main.cpp
 * @brief RoboMind-S3 主入口 — ESP32-S3 AI 聊天机器人
 *
 * 启动流程:
 *   1. NVS 初始化
 *   2. WiFi STA 连接 (Core 1)
 *   3. LVGL + 显示屏初始化 (Core 0)
 *   4. 聊天 UI 创建
 *   5. 聊天引擎启动
 *   6. FreeRTOS 双核任务调度
 */

#include "esp_camera.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cstdio>

#include "lvgl.h"

#include "wifi_manager.h"
#include "camera_budget.h"
#include "resource_gate.h"
#include "display_driver.h"
#include "chat_ui.h"
#include "chat_engine.h"
#include "sd_card.h"
#include "audio_io.h"
#include "system_monitor.h"
#include "ppm_bridge.h"

static const char* TAG = "robo_main";

static void log_mem(const char* tag)
{
    const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    std::printf("[mem] %s  free=%lu  largest=%lu  min_free=%lu  psram_free=%lu\n",
                tag,
                static_cast<unsigned long>(heap_caps_get_free_size(internal)),
                static_cast<unsigned long>(heap_caps_get_largest_free_block(internal)),
                static_cast<unsigned long>(heap_caps_get_minimum_free_size(internal)),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    std::fflush(stdout);
}

static const char* rg_state_name(rg_cam_state_t state)
{
    return state == RG_CAM_ON ? "CAM_ON" : "CAM_OFF";
}

static double mean_of(const uint8_t* p, size_t n)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += p[i];
    }
    if (n == 0) {
        return 0.0;
    }
    return static_cast<double>(sum) / static_cast<double>(n);
}

/* 重新拉起之后的前 5 帧只作废旧统计的证据，不拿灰度当门限。 */
static void warm_frames(int round)
{
    for (int i = 0; i < 5; ++i) {
        camera_fb_t* w = esp_camera_fb_get();
        if (w == nullptr) {
            std::printf("[warm] round %d frame %d get=null\n", round, i);
            std::fflush(stdout);
            continue;
        }
        const double mean = mean_of(w->buf, w->len);
        std::printf("[warm] round %d frame %d mean=%.1f\n", round, i, mean);
        std::fflush(stdout);
        esp_camera_fb_return(w);
    }
}

static volatile uint32_t g_lv_fires = 0;

/* 取地址，不调用。否则链接器会把 PPM 的静态池裁掉，堆基线就不是 R3f 那一档。 */
static void *volatile g_ppm_keep = reinterpret_cast<void *>(&ppm_bridge_start);

static void lv_probe_cb(lv_timer_t* t)
{
    (void)t;
    g_lv_fires += 1;
    if ((g_lv_fires % 10u) == 0u) {
        std::printf("[lvgl] timer_fires=%lu\n", static_cast<unsigned long>(g_lv_fires));
        std::fflush(stdout);
    }
}

extern "C" void app_main(void)
{
    esp_rom_printf("\n[CX-9] app_main start: USB-Serial-JTAG console probe\n");
    ESP_LOGI(TAG, "app_main start (CX-9 display debug)");

    // --- 1. NVS 初始化 ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupted, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init fatal: %s", esp_err_to_name(ret));
        esp_restart();
    }
    ESP_LOGI(TAG, "NVS initialized OK");

    // --- 2. 显示屏 + LVGL 初始化 (先亮屏!) ---
    if (!DisplayDriver::GetInstance()->Initialize()) {
        ESP_LOGE(TAG, "Display init failed, restarting...");
        esp_restart();
    }

    // --- 3. 聊天 UI 初始化 (先显示界面) ---
    auto* ui = ChatUI::GetInstance();
    if (!ui->Initialize()) {
        ESP_LOGE(TAG, "ChatUI init failed, restarting...");
        esp_restart();
    }
    ui->ShowSplashScreen("RoboMind-S3", "WiFi connecting...");
    lv_timer_handler();  // Flush the first LVGL frame before WiFi blocks.

    // --- 4. WiFi 连接 (屏幕亮后再联网) ---
    auto* wifi = WifiManager::GetInstance();
    log_mem("before_wifi");
    bool wifi_ok = wifi->Connect();
    log_mem("after_wifi");
    if (!wifi_ok) {
        ESP_LOGW(TAG, "WiFi connection failed, continuing offline");
        ui->ShowSplashScreen("RoboMind-S3", "WiFi offline - check SSID");
        // Reset failure counter on fresh boot
        nvs_handle_t handle;
        if (nvs_open("system", NVS_READWRITE, &handle) == ESP_OK) {
            uint32_t restart_count = 0;
            nvs_get_u32(handle, "wifi_fail_count", &restart_count);
            restart_count++;
            nvs_set_u32(handle, "wifi_fail_count", restart_count);
            nvs_commit(handle);
            nvs_close(handle);
        }
        // Don't restart — keep showing splash with error message
    } else {
        ESP_LOGI(TAG, "WiFi connected, IP: %s", wifi->GetIpAddress().c_str());
    }

    // --- 5.5. Optional module initialization ---
    /* 旧的 CameraDriver::Initialize 会在已经装好的 I2C0 上再配一次脚，
     * 背光用的 XL9555 会跟着失效。这一单只走 esp_camera_init。 */
    {
        const esp_err_t cam = camera_budget_init();
        ESP_LOGI(TAG, "esp_camera_init -> %s", esp_err_to_name(cam));
    }
    log_mem("after_camera_early");

    // --- 5. 聊天引擎启动 ---
    auto* engine = ChatEngine::GetInstance();
    if (!engine->Initialize()) {
        ESP_LOGE(TAG, "ChatEngine init failed");
        esp_restart();
    }
    log_mem("after_camera_init");

#if CONFIG_ROBOMIND_SD_PIN_CS
#if CONFIG_ROBOMIND_SD_MODE == 0 && \
    (CONFIG_ROBOMIND_SD_PIN_CS == CONFIG_ROBOMIND_DISPLAY_PIN_CS || \
     CONFIG_ROBOMIND_SD_PIN_MOSI == CONFIG_ROBOMIND_DISPLAY_PIN_MOSI || \
     CONFIG_ROBOMIND_SD_PIN_CLK == CONFIG_ROBOMIND_DISPLAY_PIN_CLK)
    ESP_LOGW(TAG, "Skipping SD auto-mount: SD SPI pins conflict with LCD during CX-9 display bring-up");
#else
    {
        auto* sd = SdCard::GetInstance();
        if (sd->Mount()) {
            ESP_LOGI(TAG, "SD card mounted at /sdcard");
        } else {
            ESP_LOGW(TAG, "SD card mount failed; expected if no card inserted");
        }
    }
#endif
#endif

#if CONFIG_ROBOMIND_ENABLE_AUDIO
    {
        auto* audio = AudioIO::GetInstance();
        if (audio->Initialize()) {
            ESP_LOGI(TAG, "Audio I/O initialized OK");
        } else {
            ESP_LOGW(TAG, "Audio init failed; check I2S pins");
        }
    }
#endif

    SystemMonitor::GetInstance()->Snapshot();

    ui->SetInputCallback([engine](const std::string& text) {
        engine->SendMessage(text);
    });

    engine->SetMessageCallback([](const std::string& role, const std::string& content) {
        ChatUI::GetInstance()->AppendMessage(role, content);
    });
    engine->SetStatusCallback([](ChatEngine::Status status, const std::string& info) {
        ChatUI::GetInstance()->SetStatus(status, info);
    });

    // --- 6. 更新状态 ---
    if (wifi_ok) {
        ui->ShowSplashScreen("RoboMind-S3", "AI Chatbot Ready");
    }

    /* 门控循环取代启动时那一次 ping。不调用 ppm_bridge_start：
     * 取帧循环还在跑的时候把摄像头退掉，是 use-after-free 的胚胎。 */
    if (g_ppm_keep == nullptr) {
        std::printf("[gate] ppm anchor missing\n");
        std::fflush(stdout);
    }
    std::printf("[gate] state=%s\n", rg_state_name(rg_camera_state()));
    std::fflush(stdout);
    log_mem("gate_cam_on");

    for (int round = 1; round <= 3; ++round) {
        const esp_err_t rel = rg_release_camera();
        std::printf("[gate] round %d release -> %s state=%s\n",
                    round, esp_err_to_name(rel), rg_state_name(rg_camera_state()));
        std::fflush(stdout);
        if (rel != ESP_OK) {
            break;
        }

        log_mem("gate_released");

        engine->SendMessage("ping");
        vTaskDelay(pdMS_TO_TICKS(2000));
        log_mem("gate_chat");

        const int64_t t0 = esp_timer_get_time();
        const esp_err_t acq = rg_acquire_camera();
        const double elapsed_s = static_cast<double>(esp_timer_get_time() - t0) / 1000000.0;
        std::printf("[gate] round %d acquire -> %s state=%s elapsed_s=%.3f\n",
                    round, esp_err_to_name(acq), rg_state_name(rg_camera_state()), elapsed_s);
        std::fflush(stdout);
        if (acq != ESP_OK) {
            break;
        }

        log_mem("gate_reacquired");
        warm_frames(round);
    }

    std::printf("[gate] state=%s\n", rg_state_name(rg_camera_state()));
    std::fflush(stdout);

    // --- 7. 主循环委托给 LVGL Task ---
    // LVGL timer task 每 5ms 触发一次 lv_timer_handler()
    // chat_ui 在 LVGL task 中刷新
    // 用户输入 → ChatEngine::SendMessage() → HTTP → SSE → UI 回调
    ESP_LOGI(TAG, "RoboMind-S3 started successfully");

    /* 探针只证明 lv_timer_handler 还在走。不启动 PPM 测量任务。 */
    if (lv_timer_create(lv_probe_cb, 100, nullptr) == nullptr) {
        std::printf("[lvgl] timer create failed\n");
        std::fflush(stdout);
    }

    // 运行 LVGL task 循环 (阻塞，永不返回)
    DisplayDriver::GetInstance()->RunLvglLoop();
}
