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
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#include "wifi_manager.h"
#include "display_driver.h"
#include "chat_ui.h"
#include "chat_engine.h"
#include "camera_driver.h"
#include "sd_card.h"
#include "audio_io.h"
#include "system_monitor.h"

static const char* TAG = "robo_main";

extern "C" void app_main(void)
{
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

    // --- 4. WiFi 连接 (屏幕亮后再联网) ---
    auto* wifi = WifiManager::GetInstance();
    bool wifi_ok = wifi->Connect();
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
#if CONFIG_ROBOMIND_CAMERA_PIN_SCL
    {
        auto* camera = CameraDriver::GetInstance();
        if (camera->Initialize()) {
            ESP_LOGI(TAG, "Camera driver initialized OK");
        } else {
            ESP_LOGW(TAG, "Camera init failed; expected if no OV5640 attached");
        }
    }
#endif

#if CONFIG_ROBOMIND_SD_PIN_CS
    {
        auto* sd = SdCard::GetInstance();
        if (sd->Mount()) {
            ESP_LOGI(TAG, "SD card mounted at /sdcard");
        } else {
            ESP_LOGW(TAG, "SD card mount failed; expected if no card inserted");
        }
    }
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

    // --- 7. 主循环委托给 LVGL Task ---
    // LVGL timer task 每 5ms 触发一次 lv_timer_handler()
    // chat_ui 在 LVGL task 中刷新
    // 用户输入 → ChatEngine::SendMessage() → HTTP → SSE → UI 回调
    ESP_LOGI(TAG, "RoboMind-S3 started successfully");

    // 运行 LVGL task 循环 (阻塞，永不返回)
    DisplayDriver::GetInstance()->RunLvglLoop();
}
