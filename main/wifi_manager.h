/**
 * @file wifi_manager.h
 * @brief WiFi STA 连接管理器 (单例)
 *
 * 职责:
 *   - WiFi STA 模式初始化与连接
 *   - 自动重连 (无限重试，指数退避)
 *   - IP 地址获取
 *   - 连接状态事件回调
 */

#ifndef ROBOMIND_WIFI_MANAGER_H
#define ROBOMIND_WIFI_MANAGER_H

#include <cstdint>
#include <functional>
#include <string>

#include "esp_event.h"

enum class WifiState {
    kDisconnected,
    kConnecting,
    kConnected,
    kError,
};

class WifiManager {
public:
    static WifiManager* GetInstance();

    /// 连接 WiFi (会阻塞直到连接成功或超时)
    bool Connect();

    /// 断开 WiFi
    void Disconnect();

    /// 获取当前 IP 地址字符串
    std::string GetIpAddress() const;

    /// 获取当前连接状态
    WifiState GetState() const;

    /// 设置状态变化回调 (在 WiFi 事件上下文中调用，需线程安全)
    using StateCallback = std::function<void(WifiState state, const std::string& info)>;
    void SetStateCallback(StateCallback callback);

private:
    WifiManager() = default;
    ~WifiManager();
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    static void EventHandler(void* arg, esp_event_base_t base,
                             int32_t event_id, void* data);
    void HandleWifiEvent(int32_t event_id, void* data);
    void HandleIpEvent(int32_t event_id, void* data);

    std::string ip_address_;
    WifiState state_{WifiState::kDisconnected};
    StateCallback state_callback_;

    static constexpr int kConnectTimeoutMs = 30000;
};

#endif  // ROBOMIND_WIFI_MANAGER_H
