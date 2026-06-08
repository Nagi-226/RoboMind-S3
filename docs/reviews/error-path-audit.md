# RX-PRE-01: 错误传播链审计报告

> 📅 审计日期: 2026-06-07 | 审计人: Reasonix
> 📋 范围: 全模块 (7 个 .cpp)，逐函数追踪 esp_err_t 返回值检查 + 资源释放

---

## 方法论

对每个模块的每个公开/内部函数，检查四维度：
- **C1**: 每个 `esp_err_t` 返回值是否被检查？
- **C2**: 每个 `return false` 是否有对应 `ESP_LOGE`（调用方可追溯根因）？
- **C3**: 失败分支是否正确释放已分配资源（I2C/SPI/Task/Buffer/Semaphore）？
- **C4**: 模式一致性：新模块 vs 老模块，同一个模式的错误处理是否对齐？

---

## 1. camera_driver (新模块)

| 函数 | 行号 | 维度 | 状态 | 详情 |
|------|------|------|------|------|
| `Initialize()` | 50-91 | C1 | ✅ | `i2c_param_config`, `i2c_driver_install`, `ReadReg`×2 全部检查 |
| | 81 | C3 | ✅ | CHIP_ID 验证失败后调用 `ReleaseI2c()` 释放 I2C 驱动 |
| | — | C2 | ✅ | 每条错误路径均有 `ESP_LOGE` |
| `WriteReg()` | 93-110 | C1 | ✅ | `i2c_master_write_to_device` 返回值检查 |
| | 95 | C3 | ✅ | `i2c_installed_` 前置守卫 |
| | — | C2 | ✅ | 参数错误 + I2C 错误均有 `ESP_LOGE` |
| `ReadReg()` | 112-136 | C1 | ✅ | `i2c_master_write_read_device` 返回值检查 |
| | 113 | C3 | ✅ | `val==nullptr` 前置检查 |
| | — | C2 | ✅ | 三种失败路径均有 `ESP_LOGE` |
| `ReleaseI2c()` | 143-153 | C1 | ✅ | `i2c_driver_delete` 返回值检查（仅 log，不传播 — 合理的析构语义） |
| | — | C2 | ✅ | 失败时 `ESP_LOGE` |

**评分**: ⭐⭐⭐⭐⭐ (5/5) — 新模块标杆

---

## 2. sd_card (新模块)

| 函数 | 行号 | 维度 | 状态 | 详情 |
|------|------|------|------|------|
| `Mount()` | 73-143 | C1 | ✅ | `spi_bus_initialize`, `esp_vfs_fat_sdspi/sdmmc_mount` 全部检查 |
| | 116-123 | C3 | ✅ | SPI 初始化成功但 mount 失败 → `spi_bus_free` 回滚 |
| | 105 | C3 | ⚠️ | `s_host` 为全局静态；SPI init 成功后若 mount 失败，`s_host` 保持初始化但 `s_card=nullptr`。后续 `Unmount` 会尝试 `spi_bus_free(s_host.slot)` —— 正确 |
| | — | C2 | ✅ | 所有错误路径均有 `ESP_LOGE` |
| `Unmount()` | 145-163 | C1 | ✅ | `esp_vfs_fat_sdcard_unmount`, `spi_bus_free` 返回值检查 |
| | — | C2 | ✅ | 失败时 `ESP_LOGE`（不传播，合理的析构语义） |
| `SaveFile()` | 175-201 | C1 | ✅ | `fopen`, `fwrite`, `fclose` 全部检查 |
| | 191 | C3 | ✅ | `fwrite` 失败后调用 `fclose` 释放 FILE handle |
| | 197 | C1 | ⚠️ 🟡 | `fclose` 非零返回值被检查并 `return false`，但此时数据已完整写入——文件内容应与返回值无关。过于严格可能导致误报 |
| `ReadFile()` | 203-249 | C1 | ✅ | `fopen`, `fseek`, `ftell`, `fread` 全部检查 |
| | 220-240 | C3 | ✅ | 每条早期返回都调用 `fclose` |
| `ListDir()` | 251-278 | C1 | ✅ | `opendir`, `closedir` 返回值检查 |
| | — | C2 | ✅ | 错误路径 `ESP_LOGE` |
| `GetFreeSpaceMB()` | 280-293 | C1 | ✅ | `statvfs` 返回值检查 |
| `ResolvePath()` | 295-303 | — | ✅ | 纯字符串操作，无 I/O |

**评分**: ⭐⭐⭐⭐⭐ (5/5) — 新模块标杆

---

## 3. system_monitor (新模块)

| 函数 | 行号 | 维度 | 状态 | 详情 |
|------|------|------|------|------|
| `Snapshot()` | 60-82 | C1 | ✅ | `esp_wifi_sta_get_ap_info` 返回值检查 |
| | 78-79 | C2 | ✅ | AP info 失败时 `ESP_LOGE` 但不中断——合理（监控应容错） |
| | — | C3 | N/A | 无资源分配 |
| `FormatHealth()` | 84-114 | — | ✅ | 纯字符串格式化，无 I/O |

**评分**: ⭐⭐⭐⭐⭐ (5/5) — 新模块标杆

---

## 4. audio_io (新模块 — 最复杂)

| 函数 | 行号 | 维度 | 状态 | 详情 |
|------|------|------|------|------|
| `Initialize()` | 103-155 | C1 | ✅ | `i2s_new_channel`, `i2s_channel_init_std_mode`, `i2s_channel_enable`, `xTaskCreate` 全部检查 |
| | 120 | C3 | ✅ | 每个失败点均调用 `ReleaseAudioResources()` |
| | 105 | C3 | 🟡 | 幂等守卫 `if (s_rx_channel && s_capture_task) return true;` — 检测全局 `s_capture_task` 而非 class member。单例下 OK，但若 `s_capture_task` 在其他路径被意外置空（如 task 自身 exit），重入会重复初始化 |
| `StartRecording()` | 157-180 | C1 | ✅ | `i2s_channel_read` 返回值检查 |
| | 159 | C2 | ✅ | 前置守卫 `s_rx_channel` `s_capture_task` + `ESP_LOGE` |
| | 174 | C3 | 🟡 | `xTaskNotifyGive` 通知 capture task；但若上一轮通知未被消费（race），此调用可能堆积通知值。`ulTaskNotifyTake(pdTRUE, ...)` 会清零，问题不大 |
| `StopRecording()` | 182-192 | C2 | ✅ | 设置 state + 唤醒 capture task |
| | — | C3 | 🟡 | 未检查 `xTaskNotifyGive` 返回值 |
| `PlayPcm()` | 194-243 | C1 | ✅ | `i2s_channel_write` 返回值检查 + 重试循环 |
| | 209 | C3 | 🔴 | 若调用时正在录音：`StopRecording()` → `InitTxChannel()` 失败 → state = kError → 不恢复录音状态。场景丢失 |
| | 236 | C3 | 🟡 | 播放成功后 state = kIdle。若播放前在录音，录音未恢复 |
| `InitTxChannel()` | 258-303 | C1 | ✅ | 所有 `i2s_*` 调用返回值检查 |
| | 275 | C3 | ✅ | 旧 TX channel 先 disable → delete 再重建 |
| | 288 | C3 | ✅ | `i2s_channel_init_std_mode` 失败后 `i2s_del_channel` + null |
| `AllocateCaptureBuffers()` | 305-321 | C1 | ✅ | `heap_caps_malloc` 返回值检查 × `kCaptureBufferCount` 次 |
| | 312 | C3 | ✅ | 任意 buffer 分配失败 → return false（已分配的不释放——**调用方 `ReleaseAudioResources` 负责**。但 `AllocateCaptureBuffers` 本身不释放已分配 buffer！）→ 🔴 见下 |
| | 312 | C3 | 🔴 | **部分分配残留**：若 buffer[0] 成功、buffer[1] 失败，buffer[0] 未释放即 return false。虽然 Initialize 的调用方会调 `ReleaseAudioResources` 清理，但 `AllocateCaptureBuffers` 作为独立函数违反自清理原则 |
| `ReleaseAudioResources()` | 323-371 | C1 | ✅ | 所有 `i2s_*` 调用返回值检查 |
| | — | C3 | ✅ | 依次清理 task → RX → TX → buffers |

**评分**: ⭐⭐⭐⭐ (4/5) — 复杂但扎实，2 个资源泄漏风险 + 1 个状态恢复问题

---

## 5. display_driver (老模块)

| 函数 | 行号 | 维度 | 状态 | 详情 |
|------|------|------|------|------|
| `Initialize()` | 85-168 | C1 | 🔴 H1 | `spi_device_transmit` 返回值（在 `LcdWriteCommand/Data/Data16` 内）全部丢弃 |
| | 142 | C1 | 🔴 H2 | `lv_disp_drv_register` 返回 `lv_disp_t*`，未检查 nullptr |
| | 159-163 | C1 | 🔴 H3 | `esp_timer_create` + `esp_timer_start_periodic` 返回值未检查 |
| | 131-135 | C3 | ✅ | `heap_caps_malloc` 返回值检查，失败 return false |
| | — | C2 | ✅ | SPI / LCD 失败路径有 `ESP_LOGE` |
| `InitSpiBus()` | 172-205 | C1 | ✅ | `spi_bus_initialize`, `spi_bus_add_device` 返回值检查 |
| `InitLcdController()` | 231-316 | C1 | 🔴 H1 | 始终 `return true` → 无故障传播 |
| `RunLvglLoop()` | 321-327 | C1 | 🔴 H4 | 死循环无 watchdog 喂狗 |
| `~DisplayDriver()` | 81-83 | C3 | 🔴 | 仅释放 LVGL buffers。未停止 `esp_timer` (tick timer 悬空回调!)、未 `spi_bus_free`、未 `lv_deinit` |

**评分**: ⭐⭐ (2/5) — 4 个已知 HIGH 问题未修复

---

## 6. wifi_manager (老模块)

| 函数 | 行号 | 维度 | 状态 | 详情 |
|------|------|------|------|------|
| `Connect()` | 37-140 | C1 | ✅ | `esp_netif_init`, `esp_event_loop_create`, `esp_wifi_init`, handler register ×2, `esp_wifi_set_mode/set_config/start` 全部检查 |
| | 72 | C1 | 🟠 M3 | `esp_netif_create_default_wifi_sta` 返回 `esp_netif_t*` 未检查 nullptr |
| | 54 | C3 | 🟠 M1 | `kMaxRetry` 硬编码 10，忽略 Kconfig `ROBOMIND_WIFI_MAX_RETRY` (默认 5) |
| `Disconnect()` | 142-160 | C3 | ✅ | 依次 unregister handler → delete event group |
| `HandleWifiEvent()` | 185-212 | C2 | ✅ | 断连 + 重试超限有 `ESP_LOGW` |
| `~WifiManager()` | 33-35 | C3 | ✅ | 调用 `Disconnect()` |

**评分**: ⭐⭐⭐⭐ (4/5) — 2 个已知 MEDIUM 问题未修复

---

## 7. chat_engine (老模块)

| 函数 | 行号 | 维度 | 状态 | 详情 |
|------|------|------|------|------|
| `Initialize()` | 62-102 | C1 | ✅ | `xSemaphoreCreateBinary`, `xTaskCreatePinnedToCore` 返回值检查 |
| `DoChatRequest()` | 282-342 | C1 | ✅ | `esp_http_client_perform` 返回值检查 |
| `ParseSSELine()` | 196-233 | C3 | ✅ | `cJSON_Parse` 后调用 `cJSON_Delete` |
| `~ChatEngine()` | header:67 | C3 | 🔴 | `= default` — 未删除 `s_chat_task` (FreeRTOS task)、未删除 `s_request_sem` (semaphore)。两者为全局静态，进程生命周期内不释放。实际运行中不会造成泄漏（静态变量在程序退出时才析构，而 ESP32 通常不复用），但违反 RAII 原则 |
| `TrimHistory()` | 153-158 | C3 | ✅ | 迭代器安全删除 |

**评分**: ⭐⭐⭐ (3/5) — 析构函数为默认（运行时无害但规范缺失）

---

## 新旧模块对比

| 维度 | camera_driver | sd_card | system_monitor | audio_io | display_driver | wifi_manager | chat_engine |
|------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| C1: esp_err_t 全覆盖 | ✅ | ✅ | ✅ | ✅ | 🔴 (4) | 🟠 (1) | ✅ |
| C2: 每条 false 有 ESP_LOGE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| C3: 失败路径资源释放 | ✅ | ✅ | N/A | 🟡 (3) | 🔴 (1) | 🟠 (1) | 🟡 (1) |
| C4: 模式一致性 | 标杆 | 标杆 | 标杆 | 接近 | 需修复 | 需修复 | 需修复 |

**结论**: 新模块 (camera/sd/system_monitor) 质量显著高于老模块。audio_io 作为最复杂的新模块有 3 个边缘问题但核心路径正确。

---

## 问题汇总（按严重等级）

| # | 等级 | 模块 | 函数 | 行号 | 问题 | 建议 |
|---|------|------|------|------|------|------|
| E1 | 🔴 | display_driver | InitLcdController | 231 | `spi_device_transmit` 返回值 5+ 次全部丢弃 | 每处加 `ESP_ERROR_CHECK` 或 `ret` 检查 + return false |
| E2 | 🔴 | display_driver | Initialize | 142 | `lv_disp_drv_register` 返回值未检查 nullptr | 加 nullptr 检查 + return false |
| E3 | 🔴 | display_driver | Initialize | 159 | `esp_timer_create` + `start_periodic` 返回值未检查 | 加 ret 检查 + return false |
| E4 | 🔴 | display_driver | ~DisplayDriver | 81 | 未停止 esp_timer / 未 spi_bus_free / 未 lv_deinit | 析构中加 esp_timer_stop/delete + spi_bus_free |
| E5 | 🔴 | audio_io | AllocateCaptureBuffers | 312 | buffer[0] 成功后 buffer[1] 失败 → buffer[0] 泄漏（依赖调用方清理） | 函数内 free 已分配的 buffer 或改为 RAII |
| E6 | 🟠 | wifi_manager | Connect | 72 | `esp_netif_create_default_wifi_sta` nullptr 未检查 | 加 nullptr 检查 + return false |
| E7 | 🟠 | wifi_manager | Connect | 28 | `kMaxRetry=10` 硬编码，Kconfig 被忽略 | 改用 `CONFIG_ROBOMIND_WIFI_MAX_RETRY` |
| E8 | 🟠 | display_driver | RunLvglLoop | 325 | 死循环无 watchdog 喂狗 | 加 `esp_task_wdt_reset()` |
| E9 | 🟡 | audio_io | PlayPcm | 209 | TX init 失败后丢失录音状态（kRecording→kError） | 保存/恢复 previous_state |
| E10 | 🟡 | audio_io | PlayPcm | 236 | 播放成功后 state=kIdle，录音未恢复 | 同上 |
| E11 | 🟡 | chat_engine | ~ChatEngine | header:67 | `= default` 不清理 FreeRTOS task/semaphore | 运行时无害（ESP32 不复用），标注即可 |
| E12 | 🟡 | sd_card | SaveFile | 197 | `fclose` 非零返回 → 报告写入失败 | 改为仅 log 警告不 return false |
