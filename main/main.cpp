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
#include <cstring>

#include "lvgl.h"

#include "wifi_manager.h"
#include "camera_budget.h"
#include "display_driver.h"
#include "chat_ui.h"
#include "chat_engine.h"
#include "sd_card.h"
#include "audio_io.h"
#include "system_monitor.h"
#include "ppm_producer.h"

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

struct frame_stats {
    unsigned min_v;
    unsigned max_v;
    unsigned distinct;
    unsigned long mean;
    unsigned long checksum;
};

frame_stats analyze(const uint8_t *buf, size_t len)
{
    unsigned hist[256];
    frame_stats s;
    size_t i;
    unsigned v;

    std::memset(hist, 0, sizeof(hist));
    s.min_v = 255u;
    s.max_v = 0u;
    s.distinct = 0u;
    s.checksum = 0u;

    for (i = 0; i < len; i++) {
        hist[buf[i]]++;
        if (buf[i] < s.min_v) { s.min_v = buf[i]; }
        if (buf[i] > s.max_v) { s.max_v = buf[i]; }
        s.checksum += buf[i];
    }
    for (v = 0; v < 256u; v++) {
        if (hist[v] != 0u) { s.distinct++; }
    }
    s.mean = (len == 0u) ? 0u : (s.checksum / static_cast<unsigned long>(len));
    if (len == 0u) { s.min_v = 0u; }
    return s;
}

static void grab_group(const char *name)
{
    for (int i = 0; i < 5; ++i) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == nullptr) {
            std::printf("[cam] %-7s f%d fb_get null\n", name, i);
            std::fflush(stdout);
            continue;
        }
        const frame_stats s = analyze(fb->buf, fb->len);
        std::printf("[cam] %-7s f%d len=%u min=%u max=%u distinct=%u mean=%lu sum=0x%08lx\n",
                    name, i,
                    static_cast<unsigned>(fb->len),
                    s.min_v, s.max_v, s.distinct, s.mean,
                    s.checksum);
        std::fflush(stdout);
        esp_camera_fb_return(fb);
    }
}

/* 组间隔必须不少于 1 秒，AE 才来得及动。这里留 12 秒，屏上给出遮挡提示。 */
static void pause_group(ChatUI *ui, const char *splash, int seconds)
{
    std::printf("[cam] pause %ds %s\n", seconds, splash);
    std::fflush(stdout);
    ui->ShowSplashScreen("R3h", splash);
    for (int i = 0; i < seconds; ++i) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static volatile uint32_t g_lv_fires = 0;

/* 取地址，不调用。否则链接器会把 PPM 的静态池裁掉。
 * U02：被锚定的对象从 ppm_bridge_start 换成 ppm_producer_start。
 * ⚠️ 这行不能删 —— 删了 .dram0.bss 会凭空少 46,911 B，那是假的。 */
static void *volatile g_ppm_keep = reinterpret_cast<void *>(&ppm_producer_start);

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

    /* 摄像头不退场。不调用 rg_release_camera。
     * PPM producer 由 CONFIG_ROBOMIND_PPM_PRODUCER_ENABLED 控制，
     * 开启时在 log_mem("after_chat") 之后启动（见上方 #if 块）。 */
    if (g_ppm_keep == nullptr) {
        std::printf("[ppm ] anchor missing\n");
        std::fflush(stdout);
    }

    const int64_t t0 = esp_timer_get_time();
    const bool sent = engine->SendMessage("ping");
    while (engine->IsBusy()) {
        if ((esp_timer_get_time() - t0) > 40000000) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    const int64_t elapsed_ms = (esp_timer_get_time() - t0) / 1000;
    std::printf("[chat] sent=%d n=1 avg=%lldms max=%lldms\n",
                sent ? 1 : 0,
                static_cast<long long>(elapsed_ms),
                static_cast<long long>(elapsed_ms));
    std::fflush(stdout);
    log_mem("after_chat");

#if CONFIG_ROBOMIND_PPM_PRODUCER_ENABLED
    /* 规格 §2.8.2：插入点固定在 after_chat 之后、RunLvglLoop() 之前。
     * 三条理由：
     *   ① 保住 [mem] 五行与 R3g 的可比性（两份都不含 producer）
     *   ② 此刻 largest 只剩 7,680 B，而栈要 6,144 B ⇒ 测的是最坏条件
     *   ③ 失败干净：xTaskCreate 返回 pdFAIL，不崩，打一行就定性
     * 默认开关为 n，本块不参与编译路径的行为（只包住调用点，不包住源文件）。 */
    ppm_producer_start();
#endif

    {
        sensor_t *s = esp_camera_sensor_get();
        if (s != nullptr) {
            std::printf("[sensor] aec=%d agc=%d gainceiling=%d quality=%d\n",
                        s->status.aec, s->status.agc,
                        s->status.gainceiling, s->status.quality);
        } else {
            std::printf("[sensor] null\n");
        }
        std::fflush(stdout);
    }

    grab_group("normal");
    pause_group(ui, "COVER LENS", 12);
    grab_group("covered");
    pause_group(ui, "UNCOVER LENS", 12);
    grab_group("normal2");

    // --- 7. 主循环委托给 LVGL Task ---
    // LVGL timer task 每 5ms 触发一次 lv_timer_handler()
    // chat_ui 在 LVGL task 中刷新
    // 用户输入 → ChatEngine::SendMessage() → HTTP → SSE → UI 回调
    ESP_LOGI(TAG, "RoboMind-S3 started successfully");

    /* 探针只证明 lv_timer_handler 还在走。
     * PPM producer 不在这里启动 —— 它在 after_chat 之后的 #if 块里。 */
    if (lv_timer_create(lv_probe_cb, 100, nullptr) == nullptr) {
        std::printf("[lvgl] timer create failed\n");
        std::fflush(stdout);
    }

    // 运行 LVGL task 循环 (阻塞，永不返回)
    DisplayDriver::GetInstance()->RunLvglLoop();
}
