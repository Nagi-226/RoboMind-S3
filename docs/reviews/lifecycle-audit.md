# RX-PRE-03: 单例生命周期审计报告

> 📅 审计日期: 2026-06-07 | 审计人: Reasonix
> 📋 范围: 6 个 singleton 的构造/析构/资源生命周期

---

## 方法论

对每个 singleton 检查：
- **L1**: 构造函数是否正确初始化成员（不依赖外部状态）？
- **L2**: 析构函数是否正确释放所有堆/硬件资源？
- **L3**: 资源创建和释放是否配对（每个 `new/malloc/install/create/open` 有对应 free/delete/close）？
- **L4**: 析构顺序是否有隐式依赖（A 的析构用到 B 但 B 可能已析构）？

---

## Singleton 总览

| # | Singleton | 析构 | 资源类型 | 评级 |
|---|-----------|------|----------|------|
| 1 | CameraDriver | `ReleaseI2c()` | I2C driver | ⭐⭐⭐⭐⭐ |
| 2 | SdCard | `Unmount()` | VFS mount + SPI bus | ⭐⭐⭐⭐⭐ |
| 3 | DisplayDriver | `heap_caps_free ×2` | SPI bus + LVGL + timer + PSRAM buffers | ⭐⭐ (3 泄漏) |
| 4 | AudioIO | `ReleaseAudioResources()` | I2S×2 + task + PSRAM buffers×2 | ⭐⭐⭐⭐⭐ |
| 5 | ChatEngine | `= default` | FreeRTOS task + semaphore + std::deque | ⭐⭐⭐ (析构无害但不规范) |
| 6 | WifiManager | `Disconnect()` | Event group + handlers + WiFi state | ⭐⭐⭐⭐⭐ |
| — | SystemMonitor | 编译器默认 | 无资源 | ⭐⭐⭐⭐⭐ |

---

## 1. CameraDriver

### 构造

```cpp
CameraDriver() = default;  // initialized_=false, i2c_installed_=false, frame_callback_=null
```

✅ L1: 成员全部有类内默认值。

### 生命周期调用链

```
CameraDriver::GetInstance()          // 首次调用 → 构造 static instance
  └─ Initialize()                    // 显式调用
       ├─ i2c_param_config()         // L3: → ReleaseI2c → i2c_driver_delete
       ├─ i2c_driver_install()       // L3: → ReleaseI2c → i2c_driver_delete
       └─ ReadReg ×2                 // 读 CHIP_ID 验证
  └─ ReleaseI2c()                    // ~CameraDriver 析构 → 调用
       └─ i2c_driver_delete()        // L3: ✅ 配对 i2c_driver_install
```

### 析构

```cpp
~CameraDriver() {
    ReleaseI2c();  // initialized_=false, i2c_driver_delete, i2c_installed_=false
}
```

✅ L2: 完整清理。`ReleaseI2c` 有 `i2c_installed_` 守卫，多次调用安全。
✅ L3: `i2c_driver_install` ↔ `i2c_driver_delete` 配对。无堆分配。
✅ L4: 无依赖其他 singleton。

**评级**: ⭐⭐⭐⭐⭐ (5/5) — 标准

---

## 2. SdCard

### 构造

```cpp
SdCard() = default;  // mounted_=false, mount_point_="/sdcard"
```

✅ L1: 成员全部有默认值。

### 生命周期调用链

```
SdCard::GetInstance()
  └─ Mount()
       ├─ [SPI] spi_bus_initialize()  // L3: → Unmount → spi_bus_free
       └─ esp_vfs_fat_sdspi_mount()   // L3: → Unmount → esp_vfs_fat_sdcard_unmount
  └─ Unmount()                        // ~SdCard 析构 → 调用
       ├─ esp_vfs_fat_sdcard_unmount  // L3: ✅ 配对
       └─ [SPI] spi_bus_free()        // L3: ✅ 配对
```

### 析构

```cpp
~SdCard() {
    Unmount();
}
```

✅ L2: 完整清理。
✅ L3: 所有资源配对。`s_card` (sdmmc_card_t*) 由 `esp_vfs_fat_sdcard_unmount` 内部管理，无需手动 free。

⚠️ 注意: `s_host` / `s_card` / `s_spi_bus_initialized` 是全局静态变量（文件作用域），不在类内。这样设计意味 SdCard "实例"的 Mount/Unmount 状态不完全封装在对象内。但因是 singleton，等价于全局状态。

✅ L4: 无依赖其他 singleton。

**评级**: ⭐⭐⭐⭐⭐ (5/5) — 标准

---

## 3. DisplayDriver

### 构造

```cpp
DisplayDriver() = default;
// display_=nullptr, buf1_=nullptr, buf2_=nullptr
// display_width_=240, display_height_=320
```

✅ L1: 成员全部有类内默认值。

### 生命周期调用链

```
DisplayDriver::GetInstance()
  └─ Initialize()
       ├─ ledc_timer_config()           // L3: ⚠️ 无对应 ledc_timer_del/ledc_stop
       ├─ spi_bus_initialize()          // L3: ⚠️ 无对应 spi_bus_free
       ├─ spi_bus_add_device()          // L3: ⚠️ 无对应 spi_bus_remove_device
       ├─ heap_caps_malloc(buf1)        // L3: ✅ ~DisplayDriver → heap_caps_free
       ├─ heap_caps_malloc(buf2)        // L3: ✅ ~DisplayDriver → heap_caps_free
       ├─ lv_init()                     // L3: ⚠️ 无对应 lv_deinit
       ├─ lv_disp_drv_register()        // L3: ⚠️ 无对应 lv_disp_remove
       ├─ esp_timer_create()            // L3: 🔴 无对应 esp_timer_stop + esp_timer_delete
       └─ esp_timer_start_periodic()    //    定时器永久运行！
  └─ RunLvglLoop()                      // 死循环，永不返回
```

### 析构

```cpp
~DisplayDriver() {
    if (buf1_) { heap_caps_free(buf1_); buf1_ = nullptr; }
    if (buf2_) { heap_caps_free(buf2_); buf2_ = nullptr; }
}
```

🔴 L2: **仅释放 LVGL 缓冲区，遗漏 4 类资源**:

| 缺失清理 | 影响 | 风险 |
|----------|------|------|
| `esp_timer_stop(tick_timer)` + `esp_timer_delete(tick_timer)` | LVGL tick 定时器持续触发回调 → 回调访问 `lv_tick_inc`，即使 LVGL 已销毁 | 🔴 悬空回调！虽然 `lv_tick_inc` 是纯函数无依赖指针，但计时器永久运行浪费 CPU + 功耗 |
| `spi_bus_free(SPI_HOST)` | SPI 总线未释放，后续初始化（如 OTA 后重新 InitSpiBus）可能失败 | 🟠 资源泄漏 |
| `lv_deinit()` | LVGL 内部分配的样式表、字体缓存等未释放 | 🟡 内存泄漏 |
| `ledc_timer` / `ledc_channel` | 背光 PWM 定时器未停止（硬件持续输出） | 🟡 背光可能常亮 |

🔴 L3: 资源缺失配对。

⚠️ L4: 实际上 `~DisplayDriver` 在 ESP32 上几乎不可能被调用：`RunLvglLoop()` 是死循环，`Initialize()` 失败时直接 `esp_restart()`，都不会到达析构。但作为规范的 RAII 实现，析构函数应正确清理所有资源（用于 OTA 场景、测试框架、或优雅关闭路径）。

**评级**: ⭐⭐ (2/5) — 三项缺失

---

## 4. AudioIO

### 构造

```cpp
AudioIO() = default;  // state_=kIdle, record_callback_=null
```

✅ L1: 成员全部有类内默认值。

### 生命周期调用链

```
AudioIO::GetInstance()
  └─ Initialize()
       ├─ AllocateCaptureBuffers()      // L3: → ReleaseAudioResources → heap_caps_free ×2
       ├─ i2s_new_channel(RX)           // L3: → ReleaseAudioResources → i2s_del_channel
       ├─ i2s_channel_init_std_mode(RX) // (由 i2s_del_channel 隐式清理)
       ├─ i2s_channel_enable(RX)        // L3: → ReleaseAudioResources → i2s_channel_disable
       └─ xTaskCreate(CaptureTask)      // L3: → ReleaseAudioResources → vTaskDelete
  └─ ReleaseAudioResources()            // ~AudioIO 析构 → 调用
       ├─ vTaskDelete(s_capture_task)   // ✅
       ├─ RX: disable → delete          // ✅
       ├─ TX: disable → delete (if set) // ✅
       └─ heap_caps_free ×2             // ✅
```

### 析构

```cpp
~AudioIO() {
    ReleaseAudioResources();
}
```

✅ L2: 完整清理。
✅ L3: 全部配对。清理顺序正确（先删 task 再清 channel，避免 task 访问已释放 channel）。

⚠️ `s_capture_buffers`, `s_rx_channel`, `s_tx_channel`, `s_capture_task` 为全局静态。同 SdCard，因 singleton 等价于全局状态。`ReleaseAudioResources` 会将这些指针全部置 null。

✅ L4: 无依赖其他 singleton。

**评级**: ⭐⭐⭐⭐⭐ (5/5) — 标准

---

## 5. ChatEngine

### 构造

```cpp
ChatEngine() = default;  // busy_=false, max_history_=20
```

✅ L1: 成员全部有类内默认值。

### 生命周期调用链

```
ChatEngine::GetInstance()
  └─ Initialize()
       ├─ xSemaphoreCreateBinary()      // L3: ⚠️ 无对应 vSemaphoreDelete
       └─ xTaskCreatePinnedToCore()     // L3: ⚠️ 无对应 vTaskDelete
  └─ (析构 = default)                   // 什么都不做
```

### 析构

```cpp
~ChatEngine() = default;
```

🔴 L2: 默认析构，不释放 FreeRTOS 资源:

| 缺失清理 | 影响 | 风险 |
|----------|------|------|
| `vTaskDelete(s_chat_task)` | ChatNetworkTask 永久运行，持有 `this` 指针 | 🔴 析构后 this 悬空 → 崩溃 |
| `vSemaphoreDelete(s_request_sem)` | 信号量泄漏 | 🟡 内存泄漏 |

**但**: 对于 ESP32 嵌入式应用，通常情况下 singleton 在 `app_main` 中初始化后永久存活直到硬件复位。析构函数在当前架构中永远不会被调用（同 DisplayDriver）。因此 **运行时无害**，但若未来引入 OTA 或动态重启模块，会成为隐患。

🔴 L3: 资源不配对。

✅ L4: ChatEngine 依赖 ChatUI（回调），但 ChatEngine 析构时 ChatUI 通常仍未析构（单例生命周期一致）。

**评级**: ⭐⭐⭐ (3/5) — 运行时无害但规范不足

---

## 6. WifiManager

### 构造

```cpp
WifiManager() = default;  // state_=kDisconnected
```

✅ L1: 成员全部有类内默认值。

### 生命周期调用链

```
WifiManager::GetInstance()
  └─ Connect()
       ├─ xEventGroupCreate()                    // L3: → Disconnect → vEventGroupDelete
       ├─ esp_netif_init()                       // (static guard, 不重复)
       ├─ esp_event_loop_create_default()        // (static guard, 不重复)
       ├─ esp_netif_create_default_wifi_sta()    // L3: ⚠️ 无对应 esp_netif_destroy
       ├─ esp_wifi_init()                        // L3: → Disconnect → esp_wifi_stop + esp_wifi_deinit (隐式)
       └─ esp_event_handler_instance_register ×2 // L3: → Disconnect → unregister
  └─ Disconnect()                                // ~WifiManager 析构 → 调用
       ├─ esp_wifi_disconnect()
       ├─ esp_wifi_stop()
       ├─ event_handler unregister ×2
       └─ vEventGroupDelete()
```

### 析构

```cpp
~WifiManager() {
    Disconnect();
}
```

✅ L2: 完整清理（除 `esp_netif_destroy`）。

⚠️ L3: `esp_netif_create_default_wifi_sta` 创建的 netif 未在 `Disconnect` 中 destroy。但 `esp_netif_destroy_default_wifi` 应配对。这是一个轻微泄漏：netif 对象在 ESP-IDF 中通常由系统统一管理，多次 create/destroy 才会累积问题。

✅ L4: 无依赖其他 singleton。

**评级**: ⭐⭐⭐⭐ (4/5) — 一个轻微泄漏

---

## 汇总

| # | Singleton | 析构 | 资源配对 | L2 评级 | 备注 |
|---|-----------|------|----------|---------|------|
| 1 | CameraDriver | `ReleaseI2c()` | 完整 | ⭐⭐⭐⭐⭐ | 新模块标杆 |
| 2 | SdCard | `Unmount()` | 完整 | ⭐⭐⭐⭐⭐ | 新模块标杆 |
| 3 | DisplayDriver | `heap_caps_free ×2` | 缺 4 项 | ⭐⭐ | 🔴 定时器泄漏，SPI 未释放 |
| 4 | AudioIO | `ReleaseAudioResources()` | 完整 | ⭐⭐⭐⭐⭐ | 新模块标杆 |
| 5 | ChatEngine | `= default` | 缺 2 项 | ⭐⭐⭐ | 运行时无害，规范不足 |
| 6 | WifiManager | `Disconnect()` | 基本完整 | ⭐⭐⭐⭐ | ⚠️ netif 未 destroy |

### 析构顺序依赖

所有 singleton 均为函数内 static 实例（Meyers' singleton）。C++ 标准保证同一翻译单元内 static 对象按构造逆序析构。跨翻译单元（不同 .cpp 文件）析构顺序未定义。

当前依赖关系：`ChatEngine` 回调引用 `ChatUI`（但 ChatUI 无析构资源）。`SystemMonitor::Snapshot` 引用所有模块。没有析构期间跨模块调用路径 → ✅ 无析构顺序问题。

但注意：若未来 `ChatEngine::~ChatEngine()` 中 `vTaskDelete(s_chat_task)` 执行时 task 代码可能访问 ChatUI，而 ChatUI 可能已析构（跨翻译单元未定义顺序）。当前无害因为析构为 default。

---

## 问题汇总

| # | 等级 | 模块 | 问题 | 建议 |
|---|------|------|------|------|
| L1 | 🔴 | DisplayDriver | `esp_timer_create` 的定时器未在析构中 stop+delete → 悬空回调 | 析构中加 `esp_timer_stop` + `esp_timer_delete` |
| L2 | 🟠 | DisplayDriver | `spi_bus_initialize` 未对应 `spi_bus_free` | 析构中加 `spi_bus_free` |
| L3 | 🟠 | DisplayDriver | `lv_init` 未对应 `lv_deinit` | 析构中加 `lv_deinit` |
| L4 | 🟡 | DisplayDriver | LEDC 背光定时器/channel 未清理 | 析构中加 `ledc_stop` |
| L5 | 🟡 | ChatEngine | `xSemaphoreCreateBinary` 未对应 `vSemaphoreDelete` | 加显式析构或标注 "嵌入式永久存活" |
| L6 | 🟡 | ChatEngine | `xTaskCreatePinnedToCore` 未对应 `vTaskDelete` | 同上 |
| L7 | 🟡 | WifiManager | `esp_netif_create_default_wifi_sta` 无 destroy 配对 | `Disconnect` 中加 `esp_netif_destroy_default_wifi` |
