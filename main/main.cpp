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

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cstdio>

#include "wifi_manager.h"
#include "camera_budget.h"
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

    // --- 5. 聊天引擎启动 ---
    auto* engine = ChatEngine::GetInstance();
    if (!engine->Initialize()) {
        ESP_LOGE(TAG, "ChatEngine init failed");
        esp_restart();
    }

    // --- 5.5. Optional module initialization ---
    /* 旧的 CameraDriver::Initialize 会在已经装好的 I2C0 上再配一次脚，
     * 背光用的 XL9555 会跟着失效。这一单只走 esp_camera_init。 */
    {
        const esp_err_t cam = camera_budget_init();
        ESP_LOGI(TAG, "esp_camera_init -> %s", esp_err_to_name(cam));
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

    /* 必须真发一次。失败没关系，要的是 TLS 内容缓冲有没有被分配。 */
    engine->SendMessage("ping");
    vTaskDelay(pdMS_TO_TICKS(2000));
    log_mem("after_chat");

    // --- 7. 主循环委托给 LVGL Task ---
    // LVGL timer task 每 5ms 触发一次 lv_timer_handler()
    // chat_ui 在 LVGL task 中刷新
    // 用户输入 → ChatEngine::SendMessage() → HTTP → SSE → UI 回调
    ESP_LOGI(TAG, "RoboMind-S3 started successfully");

    ppm_bridge_start();

    // 运行 LVGL task 循环 (阻塞，永不返回)
    DisplayDriver::GetInstance()->RunLvglLoop();
}
