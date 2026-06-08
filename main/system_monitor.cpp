#include "system_monitor.h"

#include <cstdio>

#include "audio_io.h"
#include "camera_driver.h"
#include "chat_engine.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sd_card.h"
#include "wifi_manager.h"

static const char* TAG = "sysmon";

namespace {
int64_t AppStartUs() {
    static const int64_t start_us = esp_timer_get_time();
    return start_us;
}

const char* AudioStateText(AudioIO::State state) {
    switch (state) {
    case AudioIO::State::kIdle:
        return "Idle";
    case AudioIO::State::kRecording:
        return "Recording";
    case AudioIO::State::kPlaying:
        return "Playing";
    case AudioIO::State::kError:
        return "Error";
    }
    return "Unknown";
}
}

SystemMonitor* SystemMonitor::GetInstance() {
    static SystemMonitor instance;
    return &instance;
}

SystemHealth SystemMonitor::Snapshot() {
    SystemHealth health = {};
    health.free_heap_bytes = esp_get_free_heap_size();
    health.min_free_heap_bytes = esp_get_minimum_free_heap_size();
    health.ip_address = WifiManager::GetInstance()->GetIpAddress();
    health.uptime_seconds = static_cast<uint32_t>((esp_timer_get_time() - AppStartUs()) /
                                                  1000000LL);
    health.camera_ok = CameraDriver::GetInstance()->IsInitialized();
    health.sd_ok = SdCard::GetInstance()->IsMounted();
    health.audio_ok = AudioIO::GetInstance()->GetState() != AudioIO::State::kError;
    health.chat_history_count = ChatEngine::GetInstance()->GetHistorySize();

    wifi_ap_record_t ap_info = {};
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        health.wifi_rssi_dbm = ap_info.rssi;
        health.wifi_ssid = reinterpret_cast<const char*>(ap_info.ssid);
    } else {
        ESP_LOGE(TAG, "WiFi AP info unavailable: %s", esp_err_to_name(ret));
        health.wifi_rssi_dbm = 0;
        health.wifi_ssid = "unknown";
    }

    return health;
}

std::string SystemMonitor::FormatHealth(const SystemHealth& h) {
    const uint32_t free_kb = h.free_heap_bytes / 1024;
    const uint32_t min_free_kb = h.min_free_heap_bytes / 1024;
    const uint32_t hours = h.uptime_seconds / 3600;
    const uint32_t minutes = (h.uptime_seconds % 3600) / 60;
    const uint32_t seconds = h.uptime_seconds % 60;
    const AudioIO::State audio_state = AudioIO::GetInstance()->GetState();

    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
                  "RoboMind-S3 System Health\n\n"
                  "Heap:   %lu KB free (min %lu KB)\n"
                  "WiFi:   %s (RSSI: %d dBm)\n"
                  "IP:     %s\n"
                  "Uptime: %luh %lum %lus\n"
                  "Camera: %s\n"
                  "SD:     %s\n"
                  "Audio:  %s\n"
                  "Chat:   %u messages in history",
                  static_cast<unsigned long>(free_kb),
                  static_cast<unsigned long>(min_free_kb),
                  h.wifi_ssid.empty() ? "unknown" : h.wifi_ssid.c_str(),
                  static_cast<int>(h.wifi_rssi_dbm),
                  h.ip_address.empty() ? "0.0.0.0" : h.ip_address.c_str(),
                  static_cast<unsigned long>(hours),
                  static_cast<unsigned long>(minutes),
                  static_cast<unsigned long>(seconds),
                  h.camera_ok ? "OK" : "Not Found",
                  h.sd_ok ? "Mounted" : "No Card",
                  AudioStateText(audio_state),
                  static_cast<unsigned>(h.chat_history_count));
    return std::string(buffer);
}
