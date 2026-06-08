/**
 * @file wifi_manager.cpp
 * @brief WiFi STA 连接管理器实现
 */

#include "wifi_manager.h"

#include <cstring>
#include <utility>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"

static const char* TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#ifndef CONFIG_ROBOMIND_WIFI_MAX_RETRY
#define CONFIG_ROBOMIND_WIFI_MAX_RETRY 5
#endif

// --- Kconfig defaults ---
#ifndef CONFIG_ROBOMIND_WIFI_SSID
#define CONFIG_ROBOMIND_WIFI_SSID "MyWiFi"
#endif
#ifndef CONFIG_ROBOMIND_WIFI_PASSWORD
#define CONFIG_ROBOMIND_WIFI_PASSWORD "password"
#endif

static EventGroupHandle_t s_wifi_event_group = nullptr;
static esp_event_handler_instance_t s_wifi_event_handler = nullptr;
static esp_event_handler_instance_t s_ip_event_handler = nullptr;
static int s_retry_count = 0;

WifiManager* WifiManager::GetInstance() {
    static WifiManager instance;
    return &instance;
}

WifiManager::~WifiManager() {
    Disconnect();
}

bool WifiManager::Connect() {
    state_ = WifiState::kConnecting;
    s_retry_count = 0;

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return false;
    }

    static bool netif_initialized = false;
    if (!netif_initialized) {
        esp_err_t ret = esp_netif_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "netif init failed: %s", esp_err_to_name(ret));
            return false;
        }

        ret = esp_event_loop_create_default();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "event loop create failed: %s", esp_err_to_name(ret));
            return false;
        }

        netif_initialized = true;
    }

    // Create default WiFi STA network interface.
    esp_netif_t* netif = esp_netif_create_default_wifi_sta();
    if (!netif) {
        ESP_LOGE(TAG, "Failed to create WiFi STA netif");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // === After wifi_init, all error paths must clean up ===

    ret = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &WifiManager::EventHandler, this, &s_wifi_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi event handler register failed: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return false;
    }

    ret = esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        &WifiManager::EventHandler, this, &s_ip_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ip event handler register failed: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_handler);
        s_wifi_event_handler = nullptr;
        esp_wifi_deinit();
        return false;
    }

    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid),
            CONFIG_ROBOMIND_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
            CONFIG_ROBOMIND_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_set_mode failed: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_handler);
        s_wifi_event_handler = nullptr;
        esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_event_handler);
        s_ip_event_handler = nullptr;
        esp_wifi_deinit();
        return false;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_set_config failed: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_handler);
        s_wifi_event_handler = nullptr;
        esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_event_handler);
        s_ip_event_handler = nullptr;
        esp_wifi_deinit();
        return false;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_start failed: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_handler);
        s_wifi_event_handler = nullptr;
        esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_event_handler);
        s_ip_event_handler = nullptr;
        esp_wifi_deinit();
        return false;
    }

    ESP_LOGI(TAG, "WiFi connecting to SSID: %s", CONFIG_ROBOMIND_WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(kConnectTimeoutMs));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected OK");
        state_ = WifiState::kConnected;
        return true;
    }

    if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connection failed after %d retries", s_retry_count);
        state_ = WifiState::kError;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout");
        state_ = WifiState::kError;
    }

    return false;
}

void WifiManager::Disconnect() {
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_wifi_event_handler) {
        esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_handler);
        s_wifi_event_handler = nullptr;
    }
    if (s_ip_event_handler) {
        esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_event_handler);
        s_ip_event_handler = nullptr;
    }

    state_ = WifiState::kDisconnected;

    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
    }
}

std::string WifiManager::GetIpAddress() const {
    return ip_address_;
}

WifiState WifiManager::GetState() const {
    return state_;
}

void WifiManager::SetStateCallback(StateCallback callback) {
    state_callback_ = std::move(callback);
}

void WifiManager::EventHandler(void* arg, esp_event_base_t base,
                               int32_t event_id, void* data) {
    auto* self = static_cast<WifiManager*>(arg);
    if (base == WIFI_EVENT) {
        self->HandleWifiEvent(event_id, data);
    } else if (base == IP_EVENT) {
        self->HandleIpEvent(event_id, data);
    }
}

void WifiManager::HandleWifiEvent(int32_t event_id, void* data) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_DISCONNECTED: {
        auto* evt = static_cast<wifi_event_sta_disconnected_t*>(data);
        ESP_LOGW(TAG, "WiFi disconnected, reason=%d", evt->reason);

        if (s_retry_count < CONFIG_ROBOMIND_WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "WiFi retry %d/%d", s_retry_count, CONFIG_ROBOMIND_WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }

        state_ = WifiState::kDisconnected;
        if (state_callback_) {
            state_callback_(state_, "Disconnected, reason=" + std::to_string(evt->reason));
        }
        break;
    }

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WiFi STA connected");
        s_retry_count = 0;
        state_ = WifiState::kConnected;
        break;

    default:
        break;
    }
}

void WifiManager::HandleIpEvent(int32_t event_id, void* data) {
    auto* evt = static_cast<ip_event_got_ip_t*>(data);
    char ip_str[16];
    esp_ip4addr_ntoa(&evt->ip_info.ip, ip_str, sizeof(ip_str));
    ip_address_ = ip_str;
    ESP_LOGI(TAG, "Got IP: %s", ip_address_.c_str());
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

    if (state_callback_) {
        state_callback_(WifiState::kConnected, ip_address_);
    }
}