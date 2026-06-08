# RX-PRE-02: FreeRTOS 并发安全审计报告

> 📅 审计日期: 2026-06-07 | 审计人: Reasonix
> 📋 范围: 跨核心共享数据 — chat_engine / wifi_manager / display_driver / audio_io

---

## 方法论

对每个跨核心/跨任务共享的数据结构，从三角度评估：
- **S1**: 读者/写者分别运行在哪个 core/task？
- **S2**: 是否存在并发写（两个 writer 可能同时执行）？
- **S3**: 并发读改写（read-modify-write 非原子）？

评级: ✅ 线程安全 / ⚠️ 需确认 / 🔴 潜在竞态

---

## ESP32-S3 双核拓扑

```
Core 0 (PRO_CPU):                        Core 1 (APP_CPU):
├── app_main()                            ├── ChatNetworkTask (chat_engine)
│   └── RunLvglLoop()                     │   └── DoChatRequest()
│       └── lv_timer_handler()            │       └── HttpEventHandler
│           ├── LvglFlushCallback         │
│           └── LvglTouchCallback         ├── WiFi Event Task (IDF internal)
│                                         │   └── WifiManager::EventHandler
├── LVGL tick timer (ISR callback)        │
│                                         ├── CaptureTask (audio_io)
└── ChatUI callbacks (UI events)          │   └── CaptureLoop()
    ├── OnSendClicked
    └── SendMessage → ChatEngine          └── (idle task)
```

---

## 1. chat_engine — 跨核心共享数据

### 1.1 `s_engine_for_http` (全局静态指针)

| | 详情 |
|------|------|
| **写者** | `DoChatRequest()` → Core 1, ChatNetworkTask context |
| **读者** | `HttpEventHandler()` → Core 1, 同 task 的同步回调 |
| **评估** | ✅ 线程安全 |
| **理由** | `s_engine_for_http` 在 `DoChatRequest` 开头赋值、`esp_http_client_perform` 阻塞期间被 `HttpEventHandler` 回调读取、`DoChatRequest` 末尾清零。三次操作全部在同一个 task 内串行执行 |

### 1.2 `busy_` (bool 标志位)

| | 详情 |
|------|------|
| **写者 1** | `SendMessage()` → Core 0, UI 回调 context |
| **写者 2** | `ChatNetworkTask()` → Core 1, `busy_ = false` 在请求完成时 |
| **读者 1** | `SendMessage()` → Core 0, `if (busy_)` 守卫 |
| **评估** | 🔴 潜在竞态 |
| **场景** | 时序: Core 0 读 `busy_ == false` → Core 0 准备设为 true → Core 1 同时设为 false（旧请求结束）→ Core 0 设为 true。无数据损坏（都是写 true/false），但逻辑上可能允许双请求穿透 |
| **缓解** | 当前设计中 `SendMessage` 在 UI 回调中触发，LVGL 事件循环在 Core 0 串行，短时间内连续两次 `SendMessage` 几乎不可能。但严格意义上不安全 |
| **建议** | `std::atomic<bool>` 或 FreeRTOS `TaskNotifications` |

### 1.3 `history_` (std::deque<Message>)

| | 详情 |
|------|------|
| **写者 1** | `SendMessage()` → Core 0, `history_.push_back({"user", ...})` + `TrimHistory()` |
| **写者 2** | `DoChatRequest()` → Core 1, `history_.push_back({"assistant", ...})` + `TrimHistory()` |
| **评估** | 🔴 潜在竞态 |
| **场景** | Core 1 正在 `DoChatRequest` 末尾追加 assistant 消息 → Core 0 同时 `SendMessage` 追加 user 消息。`std::deque::push_back` 非线程安全，可能导致内部指针损坏 → 崩溃或内存泄漏 |
| **实际触发概率** | 低（用户发送消息时上一个请求通常已完成），但流式响应期间用户可能发送下一条 — 此时 `busy_` 应拦截，但如 1.2 所述 `busy_` 自身非原子 |
| **建议** | 用 `SemaphoreHandle_t` / `mutex` 保护 `history_` 的所有读写，或改为 `SendMessage` 仅追加到 pending queue，由 Core 1 的 ChatNetworkTask 统一管理 history |

### 1.4 `message_callback_` / `status_callback_` (std::function)

| | 详情 |
|------|------|
| **写者** | `SetMessageCallback()` → Core 0, 启动阶段 |
| **读者** | `HttpEventHandler()` → Core 1 |
| **评估** | ⚠️ 需确认 |
| **理由** | 回调通常在 `app_main` 初始化阶段设置完毕，之后不再修改。初始化阶段 Core 1 的 ChatNetworkTask 尚未开始处理请求（semaphore 阻塞等待），不存在并发。但如果运行时动态修改回调，则不安全 |
| **建议** | 文档中标注 "必须在 ChatEngine::Initialize 之后、SendMessage 之前设置" |

### 1.5 `sse_buffer_` (std::string)

| | 详情 |
|------|------|
| **写者** | `HttpEventHandler()` → Core 1 |
| **读者** | `HttpEventHandler()` → Core 1 |
| **评估** | ✅ 线程安全 (单 task 独占) |

### 1.6 `s_request_sem` / `s_pending_message` (全局静态)

| | 详情 |
|------|------|
| **s_request_sem 写者** | `SendMessage()` → Core 0, `xSemaphoreGive` |
| **s_request_sem 读者** | `ChatNetworkTask()` → Core 1, `xSemaphoreTake` |
| **评估** | ✅ FreeRTOS 信号量自身线程安全 |
| **s_pending_message 写者** | `SendMessage()` → Core 0 |
| **s_pending_message 读者** | `ChatNetworkTask()` → Core 1, `s_pending_message.clear()` |
| **评估** | 🔴 潜在竞态 |
| **理由** | `std::string` 的赋值和 `clear()` 非原子。Core 0 正在 `s_pending_message = user_text` → Core 1 同时 `s_pending_message.clear()` → 未定义行为 |
| **实际触发概率** | 极低（SendMessage 先 Give semaphore → ChatNetworkTask 被唤醒后才 clear）。严格时序下不安全 |
| **建议** | 移除 `s_pending_message`（当前未被实际使用；仅用于 signal 后 clear）。如需保留，加 mutex |

---

## 2. wifi_manager — 跨任务共享数据

### 2.1 `state_` (WifiState enum)

| | 详情 |
|------|------|
| **写者 1** | `Connect()` → Core 0, `state_ = kConnecting` |
| **写者 2** | `HandleWifiEvent()` → WiFi Event Task (IDF internal), `state_ = kConnected/kDisconnected` |
| **写者 3** | `Disconnect()` → Core 0, `state_ = kDisconnected` |
| **读者** | `GetState()` → 任意 core/task |
| **评估** | ⚠️ 需确认（实践中安全） |
| **理由** | `WifiState` 是 enum (int-sized)，在 ESP32 上 32-bit 读写是原子操作（对齐访问）。`Connect()` 在阻塞等待 state 变化期间（`xEventGroupWaitBits` 内），WiFi 事件回调修改 state。Connect 返回后 state 不再被 WiFi 事件 task 写入（因为只触发一次 CONNECTED/DISCONNECTED）。Disconnect 调用时通常没有 WiFi 事件在处理 |
| **建议** | 标注依赖 "enum 原子性 + Connect 串行化"。如严格需要，用 `std::atomic<WifiState>` |

### 2.2 `s_retry_count` (int)

| | 详情 |
|------|------|
| **写者** | `HandleWifiEvent()` → WiFi Event Task |
| **读者** | `Connect()` → Core 0 (仅重置为 0) |
| **评估** | ✅ 线程安全 |
| **理由** | `Connect` 在 `xEventGroupWaitBits` 阻塞期间不访问 `s_retry_count`（该函数只 set EventGroup bits）。Connect 开头的 `s_retry_count = 0` 发生在注册 event handler 之前，此时没有 WiFi 事件。实践安全 |

### 2.3 `ip_address_` (std::string)

| | 详情 |
|------|------|
| **写者** | `HandleIpEvent()` → WiFi Event Task |
| **读者** | `GetIpAddress()` → Core 0 (Connect 返回后) |
| **评估** | ✅ 线程安全 |
| **理由** | `HandleIpEvent` 在 `Connect` 的阻塞等待期间执行（`xEventGroupWaitBits`）。Connect 返回后 IP 已写入完毕，后续只读 |

### 2.4 `state_callback_` (std::function)

| | 详情 |
|------|------|
| **写者** | `SetStateCallback()` → Core 0, 启动阶段 |
| **调用者** | `HandleWifiEvent()` / `HandleIpEvent()` → WiFi Event Task |
| **评估** | ⚠️ 需确认 |
| **理由** | 回调在初始化阶段设置（`app_main` 的 `SetStateCallback` 调用位置？当前代码中 `main.cpp` 未调用 `wifi->SetStateCallback`！回调存在于接口中但未被使用）。如果运行时动态设置，则不安全 |
| **建议** | 文档标注 "必须在 Connect 之前设置" |

---

## 3. display_driver — 跨回调共享数据

### 3.1 `s_spi_device` (全局静态 spi_device_handle_t)

| | 详情 |
|------|------|
| **写者** | `InitSpiBus()` → Core 0, `spi_bus_add_device` 赋值 |
| **读者** | `LvglFlushCallback()` / `LcdWriteCommand()` / `LcdWriteData()` → Core 0, LVGL timer handler |
| **评估** | ✅ 线程安全 |
| **理由** | 写入在 `app_main` 初始化阶段完成。LVGL 任务在 `RunLvglLoop` 中开始运行，初始化完成后才进入 LVGL 任务。所有 LVGL 回调在同一 core 串行。`LvglTouchCallback` 当前为空实现（无实际 SPI 访问） |

### 3.2 `buf1_` / `buf2_` (lv_color_t*)

| | 详情 |
|------|------|
| **写者** | `Initialize()` → Core 0, `heap_caps_malloc` 分配 + `lv_disp_draw_buf_init` |
| **读者/写者** | LVGL 内部 → Core 0, 渲染管线使用双缓冲 |
| **评估** | ✅ 线程安全 (LVGL 内部同步，单 core 运行) |

### 3.3 `display_` (lv_disp_t*)

| | 详情 |
|------|------|
| **写者** | `Initialize()` → Core 0, `lv_disp_drv_register` |
| **读者** | `LvglFlushCallback()` → Core 0 |
| **评估** | ✅ 线程安全 (初始化后只读) |

---

## 4. audio_io — 跨任务共享数据（最复杂）

### 4.1 `state_` (State enum)

| | 详情 |
|------|------|
| **写者 1** | `StartRecording()` / `StopRecording()` / `PlayPcm()` / `Initialize()` → Core 0, main task |
| **写者 2** | `CaptureLoop()` → Core 1, CaptureTask |
| **读者 1** | `GetState()` → 任意 core (system_monitor 调用) |
| **读者 2** | `CaptureLoop()` → Core 1, `while (state_ == kRecording)` |
| **评估** | 🔴 潜在竞态 |
| **场景 1** | Core 0: `StopRecording()` 设 `state_ = kIdle` | Core 1: 同时在 `while (state_ == kRecording)` 循环中。enum 32-bit 读写在 ESP32 上原子，但 Core 1 可能错过这次变化（无内存屏障） |
| **场景 2** | Core 0: `StartRecording()` 设 `state_ = kRecording` + `xTaskNotifyGive` | Core 1: `ulTaskNotifyTake` 被唤醒后读 `state_`。通知前 state 已写，FreeRTOS 通知有隐式内存屏障 → 安全 |
| **建议** | 改 `state_` 为 `std::atomic<State>` 或 `volatile State` + 关键区 `taskENTER_CRITICAL` |

### 4.2 `s_capture_buffers[2]` (双缓冲)

| | 详情 |
|------|------|
| **写者** | `AllocateCaptureBuffers()` → Core 0, 初始化阶段 |
| **读者/写者** | `CaptureLoop()` → Core 1, 交替填充 buffer[0] buffer[1] |
| **评估** | ✅ 线程安全 |
| **理由** | 分配在初始化完成，之后指针不变。`CaptureLoop` 使用 `buffer_index` 在 0/1 之间交替——单生产者（Core 1 CaptureLoop）单消费者（Core 0 record_callback_ 回调），经典双缓冲模式。消费者在回调中读 buffer 内容时，生产者不会写入同一 buffer（已切换到另一个）。安全 |

### 4.3 `record_callback_` (std::function)

| | 详情 |
|------|------|
| **写者** | `SetRecordCallback()` → Core 0, 启动阶段 |
| **调用者** | `CaptureLoop()` → Core 1, `record_callback_(buffer, samples, rate)` |
| **评估** | ⚠️ 需确认 |
| **理由** | 同 chat_engine — 回调在初始化后不再修改则安全。运行时动态修改需原子 |

### 4.4 `s_rx_channel` / `s_tx_channel` (I2S handle)

| | 详情 |
|------|------|
| **写者** | `Initialize()` / `InitTxChannel()` / `ReleaseAudioResources()` → Core 0 |
| **读者/写者** | `CaptureLoop()` → Core 1, 读 `s_rx_channel`；`PlayPcm()` → Core 0, 读/写 `s_tx_channel` |
| **评估** | ⚠️ 需确认 |
| **理由** | `CaptureLoop` 依赖 `s_rx_channel != nullptr` 来继续运行。如果 `ReleaseAudioResources` 在 Core 0 将 `s_rx_channel = nullptr` 的同时 `CaptureLoop` 正在 `i2s_channel_read(s_rx_channel, ...)`，传入 null handle。但当前设计中 `ReleaseAudioResources` 先 `vTaskDelete(s_capture_task)` 再清理 channel，任务删除是同步的 → 安全 |
| **建议** | 维持当前顺序（先删 task 再清 channel）。不改为并行清理 |

---

## 汇总

| 共享数据 | 模块 | Core 0 访问 | Core 1 访问 | 评级 | 建议 |
|----------|------|------------|------------|------|------|
| `s_engine_for_http` | chat_engine | — | R/W (同 task) | ✅ | — |
| `busy_` | chat_engine | R/W | W | 🔴 | `std::atomic<bool>` |
| `history_` | chat_engine | W | W | 🔴 | FreeRTOS mutex |
| `message_callback_` | chat_engine | W (init) | R | ⚠️ | 文档约束 |
| `sse_buffer_` | chat_engine | — | R/W (同 task) | ✅ | — |
| `s_pending_message` | chat_engine | W | R/W | 🔴 | 移除或用 mutex |
| `s_request_sem` | chat_engine | Give | Take | ✅ | FreeRTOS 自身安全 |
| `state_` (Wifi) | wifi_manager | W (init) | W (event) | ⚠️ | 依赖 enum 原子性 |
| `s_retry_count` | wifi_manager | W (reset) | R/W | ✅ | 时序天然串行 |
| `ip_address_` | wifi_manager | R | W (event) | ✅ | Connect 阻塞保证 |
| `state_callback_` | wifi_manager | W (init) | Call | ⚠️ | 文档约束 |
| `s_spi_device` | display_driver | R/W (同 core) | — | ✅ | — |
| `buf1_/buf2_` | display_driver | R/W (同 core) | — | ✅ | LVGL 内部同步 |
| `state_` (Audio) | audio_io | W | R/W | 🔴 | `std::atomic<State>` |
| `s_capture_buffers` | audio_io | W (init) / R (cb) | W | ✅ | 双缓冲+task 同步删 |
| `record_callback_` | audio_io | W (init) | Call | ⚠️ | 文档约束 |
| `s_rx_channel` | audio_io | W (init/clean) | R | ⚠️ | 维持清理顺序 |

### 统计

| 评级 | 数量 |
|------|------|
| ✅ 线程安全 | 7 |
| ⚠️ 需确认（实践安全，缺文档约束） | 6 |
| 🔴 潜在竞态 | 5 |
