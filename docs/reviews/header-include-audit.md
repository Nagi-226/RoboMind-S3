# 头文件包含依赖审计报告

> **审计日期**: 2026-06-08
> **审计任务**: CW-PRE-03 (P2)
> **审计人**: CodeWhale
> **范围**: `main/` 下全部 8 个 `.h` + 9 个 `.cpp` 文件
> **规则**: 标注未使用的 `#include` 和缺失的隐式依赖，不直接修改代码

---

## 摘要

- **审计文件**: 17 个 (8 头文件 + 9 实现文件)
- **发现未使用 #include**: 14 处
- **发现缺失隐式依赖**: 2 处
- **高优先级 (会导致编译脆弱)**: 0 处
- **中优先级 (维护性隐患)**: 2 处

---

## 逐文件审计

### 1. audio_io.h — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<cstddef>` | `size_t` | ✅ |
| `<cstdint>` | `int16_t`, `uint32_t` | ✅ |
| `<functional>` | `std::function` | ✅ |
| `<string>` | `std::string` | ✅ |

---

### 2. audio_io.cpp — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"audio_io.h"` | 自身头文件 | ✅ |
| `<algorithm>` | `std::max`, `std::min` | ✅ |
| `<cstring>` | `std::memset` (line 352) | ✅ |
| `<utility>` | `std::move` (line 234) | ✅ |
| `"driver/gpio.h"` | `gpio_num_t` | ✅ |
| `"driver/i2s_std.h"` | `i2s_chan_handle_t`, `i2s_channel_*`, `I2S_*` | ✅ |
| `"esp_err.h"` | `ESP_OK`, `esp_err_to_name` | ✅ |
| `"esp_heap_caps.h"` | `heap_caps_malloc`, `heap_caps_free`, `MALLOC_CAP_SPIRAM` | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` | ✅ |
| `"freertos/FreeRTOS.h"` | `xTaskCreate`, `vTaskDelete`, `ulTaskNotifyTake`, `portMAX_DELAY`, `pdTRUE`, `pdPASS`, `pdMS_TO_TICKS` | ✅ |
| `"freertos/task.h"` | `TaskHandle_t` | ✅ |
| `"sdkconfig.h"` | `CONFIG_ROBOMIND_*` | ✅ |

---

### 3. camera_driver.h — ⚠️ 1 处未使用

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<cstddef>` | `size_t` | ✅ |
| `<cstdint>` | `uint8_t`, `uint16_t` | ✅ |
| `<functional>` | `std::function` | ✅ |
| **`<vector>`** | **无** | **❌ 未使用** |

> `camera_driver.h` 不声明任何 `std::vector` 成员或参数。该头文件是纯 I2C SCCB 接口，不需要 vector。

---

### 4. camera_driver.cpp — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"camera_driver.h"` | 自身头文件 | ✅ |
| `<utility>` | `std::move` (line 131) | ✅ |
| `"driver/gpio.h"` | `gpio_num_t`, `GPIO_PULLUP_ENABLE` | ✅ |
| `"driver/i2c.h"` | `i2c_config_t`, `i2c_master_*`, `I2C_MODE_MASTER` | ✅ |
| `"esp_err.h"` | `ESP_OK`, `esp_err_to_name` | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGE` | ✅ |
| `"freertos/FreeRTOS.h"` | `pdMS_TO_TICKS`, `TickType_t` | ✅ |
| `"sdkconfig.h"` | `CONFIG_ROBOMIND_CAMERA_*` | ✅ |

---

### 5. chat_engine.h — ⚠️ 2 处未使用

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<cstddef>` | `size_t` | ✅ |
| **`<cstdint>`** | **无** | **❌ 未使用** |
| `<deque>` | `std::deque` | ✅ |
| `<functional>` | `std::function` | ✅ |
| `<string>` | `std::string` | ✅ |
| **`<vector>`** | **无** | **❌ 未使用** |
| `"esp_err.h"` | `esp_err_t` | ✅ |
| `"esp_http_client.h"` | `esp_http_client_event_t` | ✅ |

> 聊天引擎头文件中 `max_history_` 声明为 `int` 而非 `int32_t`，`history_` 使用 `std::deque` 而非 `std::vector`。两个 include 均为冗余。

---

### 6. chat_engine.cpp — ⚠️ 3 处未使用

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"chat_engine.h"` | 自身头文件 | ✅ |
| `"cJSON.h"` | `cJSON_*` | ✅ |
| **`<cstdio>`** | **无** | **❌ 未使用** |
| **`<cstring>`** | **无** | **❌ 未使用** |
| **`"esp_check.h"`** | **无** | **❌ 未使用** |
| `"esp_crt_bundle.h"` | `esp_crt_bundle_attach` | ✅ |
| `"esp_http_client.h"` | `esp_http_client_*` (继承自 .h) | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` | ✅ |
| `"freertos/FreeRTOS.h"` | `xSemaphoreCreateBinary`, `xTaskCreatePinnedToCore`, `portMAX_DELAY`, `pdTRUE`, `pdPASS` | ✅ |
| `"freertos/semphr.h"` | `SemaphoreHandle_t`, `xSemaphoreGive`, `xSemaphoreTake` | ✅ |
| `"freertos/task.h"` | `TaskHandle_t` | ✅ |
| `"system_monitor.h"` | `SystemMonitor` | ✅ |

> `chat_engine.cpp` 中无 `printf`/`fprintf`/`snprintf` 调用（grep 确认），无 `strlen`/`strcmp`/`memset` 调用，无 `ESP_RETURN_*`/`ESP_GOTO_*` 宏使用。三个 include 均为冗余。

---

### 7. chat_ui.h — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<functional>` | `std::function` | ✅ |
| `<string>` | `std::string` | ✅ |
| `"lvgl.h"` | `lv_obj_t*`, `lv_event_t*` | ✅ |
| `"chat_engine.h"` | `ChatEngine::Status` | ✅ |

---

### 8. chat_ui.cpp — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"chat_ui.h"` | 自身头文件 | ✅ |
| `<cstdio>` | `std::snprintf` (line 57) | ✅ |
| `<cstring>` | `strlen` (line 210) | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGE` | ✅ |
| `"wifi_manager.h"` | `WifiManager::GetInstance()->GetRssi()` | ✅ |

---

### 9. display_driver.h — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<cstdint>` | `uint8_t` | ✅ |
| `"esp_timer.h"` | `esp_timer_handle_t` | ✅ |
| `"lvgl.h"` | `lv_disp_t*`, `lv_disp_draw_buf_t`, `lv_color_t*` | ✅ |

---

### 10. display_driver.cpp — ⚠️ 2 未使用 + 🔴 2 隐式依赖

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"display_driver.h"` | 自身头文件 | ✅ |
| **`<cstdio>`** | **无** | **❌ 未使用** |
| **`<cstring>`** | **无** | **❌ 未使用** |
| `"driver/gpio.h"` | `gpio_num_t`, `gpio_set_direction`, `gpio_set_level`, `GPIO_MODE_*` | ✅ |
| `"driver/i2c.h"` | `i2c_config_t`, `i2c_master_write_read_device`, `i2c_param_config`, `i2c_driver_install`, `i2c_driver_delete`, `I2C_MODE_MASTER` | ✅ |
| `"driver/ledc.h"` | `ledc_timer_config_t`, `ledc_channel_config_t`, `ledc_set_duty`, `ledc_update_duty`, `LEDC_*` | ✅ |
| `"driver/spi_master.h"` | `spi_device_handle_t`, `spi_bus_*`, `spi_device_*`, `spi_transaction_t` | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGE`, `ESP_LOGW` | ✅ |
| `"esp_rom_sys.h"` | `esp_rom_delay_us` (line 156) | ✅ |
| `"esp_timer.h"` | `esp_timer_create`, `esp_timer_start_periodic`, `esp_timer_stop`, `esp_timer_delete`, `esp_timer_create_args_t` | ✅ |
| `"esp_task_wdt.h"` | `esp_task_wdt_add`, `esp_task_wdt_reset` (lines 726, 730) | ✅ |
| `"freertos/FreeRTOS.h"` | `vTaskDelay`, `pdMS_TO_TICKS` | ✅ |
| `"freertos/task.h"` | `xTaskNotifyGive` (not used?), `TaskHandle_t` | ⚠️ |
| `"freertos/semphr.h"` | `SemaphoreHandle_t` | ⚠️ |

> `<cstdio>` 和 `<cstring>` 在 display_driver.cpp 中完全未使用（grep 确认无 `printf`/`snprintf`/`memset`/`strlen` 等调用）。

**🔴 隐式依赖（缺失的显式 include）：**

| 缺失 Include | 使用符号 | 当前传递来源 |
|-------------|---------|-------------|
| **`"esp_heap_caps.h"`** | `heap_caps_malloc()` (lines 345-346), `heap_caps_free()` (lines 291, 351-352) | `freertos/task.h` → ... → `esp_heap_caps.h` |
| **`"driver/spi_common.h"`** | `SPI_DMA_CH_AUTO` (line 426) | `driver/spi_master.h` → `spi_common.h` |

> `heap_caps_malloc` 和 `heap_caps_free` 在 LVGL 缓冲区分配中直接使用，应显式 include `esp_heap_caps.h`。
> `SPI_DMA_CH_AUTO` 定义在 `driver/spi_common.h`，当前通过 `driver/spi_master.h` 间接获取。

---

### 11. main.cpp — ⚠️ 3 处未使用

| Include | 使用符号 | 状态 |
|---------|---------|------|
| **`<cstdio>`** | **无** | **❌ 未使用** |
| **`<cstring>`** | **无** | **❌ 未使用** |
| `"esp_err.h"` | `ESP_OK`, `esp_err_to_name`, `ESP_ERR_NVS_*` | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` | ✅ |
| `"esp_system.h"` | `esp_restart` | ✅ |
| `"nvs.h"` | `nvs_open`, `nvs_get_u32`, `nvs_set_u32`, `nvs_commit`, `nvs_close` | ✅ |
| `"nvs_flash.h"` | `nvs_flash_init`, `nvs_flash_erase`, `ESP_ERROR_CHECK` | ✅ |
| `"freertos/FreeRTOS.h"` | `vTaskDelay`, `pdMS_TO_TICKS` | ✅ |
| **`"freertos/task.h"`** | **无** | **❌ 未使用** |
| `"sdkconfig.h"` | `CONFIG_ROBOMIND_*` | ✅ |
| `"wifi_manager.h"` | `WifiManager` | ✅ |
| `"display_driver.h"` | `DisplayDriver` | ✅ |
| `"chat_ui.h"` | `ChatUI` | ✅ |
| `"chat_engine.h"` | `ChatEngine` | ✅ |
| `"camera_driver.h"` | `CameraDriver` | ✅ |
| `"sd_card.h"` | `SdCard` | ✅ |
| `"audio_io.h"` | `AudioIO` | ✅ |
| `"system_monitor.h"` | `SystemMonitor` | ✅ |

> main.cpp 不用任何 C 标准库函数，不用 `TaskHandle_t`/`xTaskCreate`。三个 include 为冗余。

---

### 12. sd_card.h — ⚠️ 1 处未使用

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<cstddef>` | `size_t` | ✅ |
| `<cstdint>` | `uint64_t`, `uint8_t` | ✅ |
| **`<functional>`** | **无** | **❌ 未使用** |
| `<string>` | `std::string` | ✅ |
| `<vector>` | `std::vector` | ✅ |

> `sd_card.h` 不声明任何回调，`<functional>` 是冗余。

---

### 13. sd_card.cpp — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"sd_card.h"` | 自身头文件 | ✅ |
| `<cstdio>` | `FILE`, `fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell`, `SEEK_END`, `SEEK_SET`, `remove` | ✅ |
| `<cstring>` | `std::strcmp` | ✅ |
| `<dirent.h>` | `DIR`, `opendir`, `readdir`, `closedir`, `dirent` | ✅ |
| `<sys/statvfs.h>` | `statvfs` | ✅ |
| `"driver/gpio.h"` | `gpio_num_t` | ✅ |
| `"driver/sdmmc_host.h"` | `SDMMC_HOST_DEFAULT`, `sdmmc_slot_config_t`, `SDMMC_SLOT_CONFIG_DEFAULT` | ✅ |
| `"driver/sdspi_host.h"` | `SDSPI_HOST_DEFAULT`, `sdspi_device_config_t`, `SDSPI_DEVICE_CONFIG_DEFAULT`, `SDSPI_DEFAULT_DMA` | ✅ |
| `"driver/spi_common.h"` | `spi_bus_initialize`, `spi_bus_free` | ✅ |
| `"esp_err.h"` | `ESP_OK`, `esp_err_to_name`, `esp_err_t` | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGE` | ✅ |
| `"esp_vfs_fat.h"` | `esp_vfs_fat_sdmmc_mount_config_t`, `esp_vfs_fat_sdspi_mount`, `esp_vfs_fat_sdmmc_mount`, `esp_vfs_fat_sdcard_unmount` | ✅ |
| `"sdmmc_cmd.h"` | `sdmmc_card_print_info` | ✅ |
| `"sdkconfig.h"` | `CONFIG_ROBOMIND_SD_*` | ✅ |

---

### 14. system_monitor.h — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<cstddef>` | `size_t` | ✅ |
| `<cstdint>` | `uint32_t`, `int8_t` | ✅ |
| `<string>` | `std::string` | ✅ |

---

### 15. system_monitor.cpp — ⚠️ 1 处未使用

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"system_monitor.h"` | 自身头文件 | ✅ |
| `<cstdio>` | `std::snprintf` (line 85) | ✅ |
| `"audio_io.h"` | `AudioIO::State`, `AudioIO::GetInstance()` | ✅ |
| `"camera_driver.h"` | `CameraDriver::GetInstance()->IsInitialized()` | ✅ |
| `"chat_engine.h"` | `ChatEngine::GetInstance()->GetHistorySize()` | ✅ |
| `"esp_err.h"` | `ESP_OK`, `esp_err_to_name` | ✅ |
| `"esp_heap_caps.h"` | `esp_get_free_heap_size()`, `esp_get_minimum_free_heap_size()` | ✅ |
| `"esp_log.h"` | `ESP_LOGE` | ✅ |
| **`"esp_system.h"`** | **无** | **❌ 未使用** |
| `"esp_timer.h"` | `esp_timer_get_time` | ✅ |
| `"esp_wifi.h"` | `esp_wifi_sta_get_ap_info`, `wifi_ap_record_t` | ✅ |
| `"sd_card.h"` | `SdCard::GetInstance()->IsMounted()` | ✅ |
| `"wifi_manager.h"` | `WifiManager::GetInstance()->GetIpAddress()` | ✅ |

> `esp_get_free_heap_size()` 和 `esp_get_minimum_free_heap_size()` 来自 `esp_heap_caps.h`（已 include），`esp_system.h` 的 `esp_restart` 等函数未在此文件中使用。`esp_system.h` 为冗余。

---

### 16. wifi_manager.h — ✅ 无问题

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `<cstdint>` | `int8_t` | ✅ |
| `<functional>` | `std::function` | ✅ |
| `<string>` | `std::string` | ✅ |
| `"esp_event.h"` | `esp_event_base_t`, `int32_t` | ✅ |

---

### 17. wifi_manager.cpp — ⚠️ 1 处未使用

| Include | 使用符号 | 状态 |
|---------|---------|------|
| `"wifi_manager.h"` | 自身头文件 | ✅ |
| `<cstring>` | `strncpy` (line 105) | ✅ |
| `<utility>` | `std::move` (line 218) | ✅ |
| `"esp_err.h"` | `ESP_OK`, `esp_err_to_name`, `esp_err_t` | ✅ |
| `"esp_log.h"` | `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` | ✅ |
| **`"esp_mac.h"`** | **无** | **❌ 未使用** |
| `"esp_wifi.h"` | `esp_wifi_init`, `esp_wifi_*`, `wifi_config_t`, `WIFI_MODE_STA`, `wifi_event_sta_disconnected_t` | ✅ |
| `"esp_netif.h"` | `esp_netif_init`, `esp_netif_create_default_wifi_sta`, `esp_netif_t` | ✅ |
| `"freertos/FreeRTOS.h"` | `xEventGroupCreate`, `vEventGroupDelete`, `xEventGroupSetBits`, `xEventGroupWaitBits`, `EventGroupHandle_t`, `EventBits_t`, `pdFALSE`, `pdMS_TO_TICKS` | ✅ |
| `"freertos/event_groups.h"` | `EventGroupHandle_t` (补充) | ✅ |
| `"lwip/ip4_addr.h"` | `esp_ip4addr_ntoa` (line 270) | ✅ |

> `esp_mac.h` 提供 `esp_read_mac`/`esp_efuse_mac_get_default`，wifi_manager.cpp 未调用任何 MAC 相关函数。grep 确认无使用。

---

## 汇总表

### ❌ 未使用的 #include (14 处)

| # | 文件 | 行 | 未使用的 Include | 优先级 |
|---|------|-----|-----------------|--------|
| 1 | `camera_driver.h` | 8 | `#include <vector>` | 低 |
| 2 | `chat_engine.h` | 17 | `#include <cstdint>` | 低 |
| 3 | `chat_engine.h` | 22 | `#include <vector>` | 低 |
| 4 | `chat_engine.cpp` | 12 | `#include <cstdio>` | 低 |
| 5 | `chat_engine.cpp` | 13 | `#include <cstring>` | 低 |
| 6 | `chat_engine.cpp` | 15 | `#include "esp_check.h"` | 低 |
| 7 | `display_driver.cpp` | 16 | `#include <cstdio>` | 低 |
| 8 | `display_driver.cpp` | 17 | `#include <cstring>` | 低 |
| 9 | `main.cpp` | 15 | `#include <cstdio>` | 低 |
| 10 | `main.cpp` | 16 | `#include <cstring>` | 低 |
| 11 | `main.cpp` | 23 | `#include "freertos/task.h"` | 低 |
| 12 | `sd_card.h` | 8 | `#include <functional>` | 低 |
| 13 | `system_monitor.cpp` | 10 | `#include "esp_system.h"` | 低 |
| 14 | `wifi_manager.cpp` | 13 | `#include "esp_mac.h"` | 低 |

### 🔴 缺失的隐式依赖 (2 处)

| # | 文件 | 缺失 Include | 使用符号 | 优先级 |
|---|------|-------------|---------|--------|
| 1 | `display_driver.cpp` | `"esp_heap_caps.h"` | `heap_caps_malloc`, `heap_caps_free` | 中 |
| 2 | `display_driver.cpp` | `"driver/spi_common.h"` | `SPI_DMA_CH_AUTO` | 低 |

---

## 建议

1. **14 处未使用 include 可直接删除**，不影响编译。建议在 CW-PRE-01 清理时一并处理。
2. **`display_driver.cpp` 缺失 `esp_heap_caps.h`** — 虽然当前通过传递依赖编译通过，但 ESP-IDF 版本升级可能导致头文件重组、传递链断裂。建议显式添加。
3. **`display_driver.cpp` 缺失 `driver/spi_common.h`** — 优先级低，`spi_master.h` 稳定包含 `spi_common.h`，但显式 include 更规范。
4. `chat_engine.h` 中 `<cstdint>` 和 `<vector>` 的删除需要确认没有任何外部使用者依赖该头文件传递这些符号（项目内已确认无此依赖）。
