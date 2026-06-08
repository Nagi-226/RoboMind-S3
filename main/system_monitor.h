#pragma once
#ifndef ROBOMIND_SYSTEM_MONITOR_H
#define ROBOMIND_SYSTEM_MONITOR_H

#include <cstddef>
#include <cstdint>
#include <string>

struct SystemHealth {
    uint32_t free_heap_bytes;
    uint32_t min_free_heap_bytes;
    int8_t wifi_rssi_dbm;
    std::string wifi_ssid;
    std::string ip_address;
    uint32_t uptime_seconds;
    bool camera_ok;
    bool sd_ok;
    bool audio_ok;
    size_t chat_history_count;
};

class SystemMonitor {
public:
    static SystemMonitor* GetInstance();

    /// Capture a current system health snapshot.
    SystemHealth Snapshot();

    /// Format health as multi-line text for chat/local diagnostics.
    std::string FormatHealth(const SystemHealth& h);

private:
    SystemMonitor() = default;
    SystemMonitor(const SystemMonitor&) = delete;
    SystemMonitor& operator=(const SystemMonitor&) = delete;
};

#endif  // ROBOMIND_SYSTEM_MONITOR_H
