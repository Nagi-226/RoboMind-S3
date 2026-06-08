# CLAUDE.md

This file provides guidance to **Claude Code** when working with code in this repository.

> **你的角色**: 架构师 + 规划者 + 审查者。你负责设计、规划、审查、拍板。
> 具体代码实现由实现者池按任务复杂度分级执行。
> 完整版本规划见 [`docs/version_roadmap.md`](docs/version_roadmap.md)。

> 🛡️ **护栏原则**: 本项目遵循 [`GUARDRAILS.md`](GUARDRAILS.md) 五层防御体系。
> Claude Code 作为架构师，一旦因技术认知不足做出偏离版本规划的决策（架构矛盾/范围蔓延/不可行承诺），
> 任何实现者 Agent (CodeWhale/Reasonix/Codex) 有权暂停并提示拉回。
> 偏离检测五步 SOP: 检测→评级→拦截→说明→回拽。详见 [偏离检测与回拽](GUARDRAILS.md#m5-偏离检测与回拽五步-sop)。

---

## 🎯 Claude Code 职责定位
> Current version: v0.0.8 (pre-board audio I/O + touch drivers).

```
你是 Architect，管理一个 3 人实现团队。

你要做的:
  ✅ 架构设计 — "这个功能应该怎么设计？"
  ✅ 版本规划 — "当前版本做什么、下个版本做什么？"
  ✅ 代码审查 — "这段代码正确、安全、可维护吗？"
  ✅ 任务分配 — "这个任务给 CodeWhale/Reasonix/Codex 谁做？"
  ✅ 文档维护 — "所有 MD 是否反映最新状态？"
  ✅ 决策拍板 — "A 方案还是 B 方案？理由是什么？"
  ✅ 验收确认 — "这个版本是否达到交付标准？"

实现者团队 (你分配任务，按复杂度分级):
  🔧 Codex     → 高级资深开发者 (GPT-5.5), 复杂模块/协议实现/多文件重构
  🐋 CodeWhale → 日常开发者 (DS-v4-pro), 单文件修复/Kconfig/编译烧录, Plan→Agent安全网
  🧠 Reasonix → 调试专家 (DS-v4-pro, Cache-First), 复杂bug/内存泄漏/死锁/长会话
  🕊️ Hermes    → 知识策展, 技能创建, 定时自动化, 多平台通知

任务分配决策 (4 问):
  1. 架构/规划/审查/验收？ → 你自己 (Claude Code)
  2. 新模块创建 / 跨3+文件 / 驱动协议 / 安全代码？ → Codex (GPT-5.5 最强)
  3. 复杂 bug 需要10+轮调试 / 内存/死锁/并发？ → Reasonix (Cache-First 最省)
  4. 单文件修复 / Kconfig / 探索性任务 / 编译烧录？ → CodeWhale (Plan→Agent 最安全)

工作流程:
  1. 输出版本规划 → 写入 version_roadmap.md
  2. 审查当前代码 → 输出审查报告, 标注谁修哪个文件
  3. 指派实现者 → "Codex 实现 camera_driver, CodeWhale 修 Kconfig, Reasonix 查内存泄漏"
  4. 审查实现产出 → 通过/驳回
  5. 验收 → 标记版本完成 → 规划下一版本
  6. 知识沉淀 → Hermes 捕获本次版本的经验教训, 更新技能库
```

---

## ⚠️ 硬件状态：开发板在途

| 项目 | 状态 |
|------|------|
| 开发板 | 📦 **已购买，快递寄送中** — 预计近期到货 |
| 说明书/原理图 | ⏳ 到货后上传至本仓库 |
| GPIO 引脚定义 | ⏳ **待板子到货后从说明书/丝印确认** |
| 当前代码 | ✅ 通用 ESP32-S3 AI 聊天框架已就绪，引脚通过 Kconfig 可配 |

**到达后的第一件事**：打开 `docs/board_arrival_checklist.md` 逐项执行。

---

## 目标硬件

**正点原子 (ALIENTEK) ESP32-S3 开发板 — "小智AI聊天机器人"组合套件**

| 组件 | 型号 | 代码支持状态 |
|------|------|-------------|
| 主控 | ESP32-S3 (双核 240MHz, 16MB Flash, 8MB PSRAM) | ✅ 已适配 |
| 显示屏 | 2.4" TFT LCD (SPI, 推测 ILI9341, 240×320) | ✅ 驱动已实现，引脚待确认 |
| 触摸屏 | 待确认 (XPT2046 或 FT6x06) | ✅ 接口已预留 |
| 摄像头 | **OV5640** (5MP, DVP 并口) | ❌ 需新增 `camera_driver` 模块 |
| TF 卡槽 | MicroSD (SPI 或 SDMMC) | ❌ 需新增 `sd_card` 模块 |
| 音频输入 | MEMS 麦克风 (I2S, 推测 INMP441) | ⚠️ `audio_io` 接口已预留，实现为空 |
| 音频输出 | I2S 功放 (推测 MAX98357) | ⚠️ `audio_io` 接口已预留，实现为空 |
| USB | USB-C (烧录/调试/串口) | ✅ 标准 ESP-IDF |

---

## Project Identity

**RoboMind-S3** — 基于正点原子 ESP32-S3 套件的 AI 聊天机器人终端。通过 WiFi 对接云端大模型（OpenAI/Claude/Ollama），在 2.4 寸显示屏上做实时对话，带摄像头视觉输入能力。

> 🎯 **核心定位**: 正点原子「小智AI」套件出厂自带固件就能对话——但它是封闭的成品玩具。
> **RoboMind-S3 把封闭的成品玩具，变成开源的、可编程的、可扩展的 AI 机器人平台。**
>
> 出厂固件绑定单一云服务、闭源不可改、功能固定；RoboMind-S3 实现**模型自由**（OpenAI/Claude/Ollama 任意切换）、**硬件可编程**（摄像头/TF卡/GPIO 传感器）、**系统可集成**（车载桥接/ROS2/MQTT 预留接口）。
> 如果目标只是"跟 AI 聊天"，出厂固件就够用；如果要"在自己项目里嵌入可定制的 AI 能力"，这才是 RoboMind-S3 的战场。

- **Target:** 正点原子 ESP32-S3 + 小智AI套件 (ESP32-S3-WROOM-1, 16MB Flash, 8MB Octal PSRAM)
- **Framework:** ESP-IDF v5.2+ (FreeRTOS, LWIP, mbedTLS, LVGL v8)
- **Language:** C++17 (ESP-IDF component style)
- **Build:** `idf.py build` / `idf.py flash monitor`

---

## 项目当前进度

### 已实现 (可直接编译运行，引脚适配后即用)

| 模块 | 文件 | 状态 | 说明 |
|------|------|------|------|
| 项目骨架 | `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv` | ✅ | ESP-IDF 标准结构，OTA 双分区 |
| 系统入口 | `main/main.cpp` | ✅ | NVS→WiFi→LVGL→Chat 启动流程 |
| WiFi 管理 | `main/wifi_manager.h/cpp` | ✅ | STA 连接 + 自动重连 + 事件驱动 |
| AI 聊天引擎 | `main/chat_engine.h/cpp` | ✅ | OpenAI/Claude 兼容 SSE 流式 API |
| 显示屏驱动 | `main/display_driver.h/cpp` | ✅ | ILI9341/ST7789/GC9A01 SPI 驱动 + LVGL 移植 |
| 聊天 UI | `main/chat_ui.h/cpp` | ✅ | LVGL 对话气泡 + 文本输入 + 状态栏 |
| Kconfig 配置 | `main/Kconfig.projbuild` | ✅ | WiFi/API/显示/触摸/音频 全部可 menuconfig |

### 待实现 (板子到货后按优先级开发)

| 优先级 | 模块 | 文件 | 依赖 | 工作量 |
|--------|------|------|------|--------|
| **P0** | 板级引脚适配 | `sdkconfig.defaults` | 说明书/原理图 | 1h |
| **P0** | 触摸屏驱动 | `display_driver.cpp` (扩展) | 确认触摸芯片型号 | 2h |
| **P1** | OV5640 摄像头驱动 | `main/camera_driver.h/cpp` (新建) | DVP 引脚定义 | 1-2 days |
| **P1** | TF 卡驱动 | `main/sd_card.h/cpp` (新建) | SPI/SDMMC 引脚定义 | 1 day |
| **P2** | I2S 音频输入 (麦克风) | `audio_io.cpp` (充实) | I2S 引脚定义 | 1 day |
| **P2** | I2S 音频输出 (TTS) | `audio_io.cpp` (充实) | I2S 引脚定义 | 1 day |
| **P3** | 拍照→CV→LLM 多模态 | `chat_engine.cpp` (扩展) | P1 完成 | 2 days |
| **P3** | 语音→ASR→LLM→TTS 全双工 | `audio_io.cpp` + `chat_engine.cpp` | P2 完成 | 3 days |

---

## GPIO 引脚占位表

> ⚠️ **以下为推测值，待板子到货后根据说明书/丝印修正！**

| 功能 | 信号 | 推测 GPIO | 实际 GPIO (待填) | 备注 |
|------|------|-----------|------------------|------|
| **LCD SPI** | MOSI | GPIO11 | ? | |
| | SCLK | GPIO12 | ? | |
| | CS | GPIO10 | ? | |
| | DC | GPIO9 | ? | Data/Command |
| | RST | GPIO8 | ? | 可能接 EN |
| | BL | GPIO7 | ? | PWM 背光 |
| **触摸** | T_CS | GPIO15 | ? | 如果 XPT2046 |
| | T_IRQ | GPIO13 | ? | 触摸中断 |
| **摄像头** | XCLK | ? | ? | OV5640 主时钟 |
| | PCLK | ? | ? | 像素时钟 |
| | VSYNC | ? | ? | 帧同步 |
| | HREF | ? | ? | 行同步 |
| | D0-D7 | ? | ? | 8-bit 并口数据 |
| | SCL | ? | ? | SCCB/I2C 配置 |
| | SDA | ? | ? | SCCB/I2C 配置 |
| **TF 卡** | CS | ? | ? | SPI: 独立 CS |
| | MOSI | ? | ? | |
| | MISO | ? | ? | |
| | CLK | ? | ? | |
| **I2S 音频** | BCK | ? | ? | 位时钟 |
| | WS | ? | ? | 字选(LRCLK) |
| | DIN | ? | ? | 麦克风→ESP |
| | DOUT | ? | ? | ESP→功放 |

---

## Quick Start (板子到货后)

```bash
# 0. 先读板子说明书，找到 GPIO 引脚定义表

# 1. 安装 ESP-IDF v5.2+
# https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/

# 2. 克隆本仓库
cd ~/projects
# (已在本地 E:\Github Project\RoboMind-S3)

# 3. 激活 ESP-IDF 环境
. ~/esp-idf/export.sh

# 4. 首次配置 — 根据说明书填入正确的 GPIO 引脚
idf.py menuconfig
#   → RoboMind S3 Configuration
#     → WiFi: 填 SSID/密码
#     → AI API: 填 API Key
#     → Display: 选驱动芯片 + 填入正确的 SPI 引脚
#     → Touch: 启用 + 填引脚
#     → Audio: 暂不启用 (Phase 2)

# 5. 编译
idf.py build

# 6. 烧录 + 监视
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│              正点原子 ESP32-S3 小智AI 套件                  │
│                                                           │
│  Core 0 (UI)              Core 1 (Network + AI + Camera)  │
│  ┌─────────────┐          ┌──────────────────────┐       │
│  │ chat_ui     │          │ wifi_manager          │       │
│  │ (LVGL)      │◄────────►│ (STA + auto-RC)       │       │
│  │             │          │                       │       │
│  │ display     │          │ chat_engine           │       │
│  │ _driver     │          │ (HTTP/SSE→LLM)        │       │
│  │             │          │                       │       │
│  │ [camera     │          │ [audio_io]            │       │
│  │  _driver]   │          │ (I2S mic+TTS)         │       │
│  └─────────────┘          └──────────────────────┘       │
│         ▲                           │                     │
│   2.4" TFT LCD              WiFi → Cloud LLM              │
│   OV5640 Camera              (OpenAI / Claude / Ollama)   │
│   TF Card (photos/logs)                                   │
└──────────────────────────────────────────────────────────┘
```

### Manager Interaction Diagram

```
main.cpp (app_main)
├── nvs_flash_init()
├── WifiManager::Connect()                    // WiFi STA 连接
├── DisplayDriver::Initialize()               // SPI LCD + LVGL 移植
│   ├── spi_bus_initialize()
│   ├── InitLcdController()                   // ILI9341/ST7789/GC9A01 命令序列
│   └── lv_disp_drv_register()
├── ChatUI::Initialize()                      // LVGL 界面构建
├── ChatEngine::Initialize()                  // API endpoint/key/model 配置
│   └── SetMessageCallback() → ChatUI::AppendMessage()
└── DisplayDriver::RunLvglLoop()              // 阻塞主循环, 5ms tick
```

---

## Module Responsibilities

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| `main` | `main.cpp` | 入口: NVS→WiFi→LVGL→Chat 启动串联 |
| `WifiManager` | `wifi_manager.h/cpp` | STA 连接, 自动重连, IP 获取, 事件回调 |
| `ChatEngine` | `chat_engine.h/cpp` | HTTP POST → LLM, SSE 流解析, 对话历史管理 |
| `DisplayDriver` | `display_driver.h/cpp` | SPI 总线, LCD 命令, LVGL flush/touch/tick |
| `ChatUI` | `chat_ui.h/cpp` | 对话气泡, 文本输入, 发送按钮, 状态指示 |
| `AudioIO` | `audio_io.h/cpp` | 预留: I2S 录音/TTS 播放 |
| `[CameraDriver]` | `camera_driver.h/cpp` (待建) | OV5640 DVP 初始化 + JPEG 拍照 |
| `[SdCard]` | `sd_card.h/cpp` (待建) | TF 卡挂载 (FAT), 照片/日志读写 |

---

## FreeRTOS Task Model

```
[Core 0]  lvgl_timer_task (5ms tick, esp_timer callback)
          → lvgl_task (prio 5, 8192 stack, LVGL flush + animations)

[Core 1]  wifi_event_task (prio 3, WiFi/IP event handler)
          → chat_network_task (prio 4, 8192 stack, HTTP request + SSE parse)
          → [camera_task] (prio 2, 4096 stack, OV5640 frame capture)
          → [audio_capture_task] (prio 1, 4096 stack, I2S mic → buffer)
```

---

## Kconfig (menuconfig) Quick Reference

所有可配置项在 `main/Kconfig.projbuild`，`idf.py menuconfig` 进入 `RoboMind S3 Configuration`:

### WiFi
- `ROBOMIND_WIFI_SSID` / `ROBOMIND_WIFI_PASSWORD`
- `ROBOMIND_WIFI_MAX_RETRY` (default: 5)

### AI API
- `ROBOMIND_AI_API_ENDPOINT` (default: OpenAI)
- `ROBOMIND_AI_API_KEY`
- `ROBOMIND_AI_MODEL_NAME` (default: gpt-4o-mini)
- `ROBOMIND_AI_SYSTEM_PROMPT` (中文系统提示词)
- `ROBOMIND_AI_MAX_TOKENS` (default: 1024)
- `ROBOMIND_CHAT_HISTORY_MAX` (default: 20)

### Display
- `ROBOMIND_DISPLAY_DRIVER` (ILI9341 / ST7789 / ST7796 / GC9A01)
- `ROBOMIND_DISPLAY_WIDTH` / `ROBOMIND_DISPLAY_HEIGHT`
- SPI Host + GPIO: MOSI, MISO, CLK, CS, DC, RST, BL

### Touch
- `ROBOMIND_TOUCH_ENABLE`
- `ROBOMIND_TOUCH_DRIVER` (XPT2046 / FT6x06)
- 对应 SPI/I2C 引脚

### Audio (默认关闭)
- `ROBOMIND_ENABLE_AUDIO`
- I2S BCK, WS, DIN, DOUT 引脚

### Advanced
- `ROBOMIND_USE_SERIAL_INPUT` — 串口终端输入 (调试 fallback)
- `ROBOMIND_HTTP_TIMEOUT_MS` — API 超时 (default: 30s)

---

## Common Pitfalls

1. **首次编译前必须 `idf.py menuconfig`** — 至少配好 WiFi、API Key、显示屏 GPIO 引脚
2. **PSRAM 必须开启** — 聊天历史 + LVGL buffer 超 512KB SRAM，菜单 `Component config → ESP PSRAM` 全部打开
3. **TLS 证书包** — 已通过 `sdkconfig.defaults` 开启 `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE`
4. **SPI 引脚冲突检查** — ESP32-S3 的 GPIO 35-48 仅能做输入(无内部上拉)，不要分配给输出信号(CS/DC/RST)
5. **堆栈溢出** — LVGL task ≥4096 words, Chat network task ≥8192 words (TLS 握手需大栈)
6. **摄像头 DVP 与 LCD SPI 引脚隔离** — OV5640 的 8-bit DVP 占用大量 GPIO，必须确认不与 LCD/触摸/TF 冲突
7. **JSON buffer 不放在栈上** — SSE 流解析用动态分配，AI 回复可能很长
8. **Watchdog** — 长时间 SSE 流中需 `vTaskDelay(1)` 喂狗或配置更长超时

---

## Adding a New Module

1. **新建文件**: `main/<module>.h` + `main/<module>.cpp`
2. **注册到 CMake**: 编辑 `main/CMakeLists.txt`，在 `SRCS` 中添加 `.cpp`
3. **Kconfig (如需)**: 编辑 `main/Kconfig.projbuild`，添加配置项
4. **调用**: 在 `main/main.cpp` 中 `Initialize()` 并串联到启动流程

### 示例：新增摄像头模块

```cpp
// main/camera_driver.h
class CameraDriver {
public:
    static CameraDriver* GetInstance();
    bool Initialize();                           // 配置 DVP + SCCB
    bool CaptureJpeg(std::vector<uint8_t>* out); // 拍一张 JPEG
    void SetFrameCallback(FrameCallback cb);      // 连续帧回调
};
```

在新模块稳定之前，先不注册到 `main.cpp` 启动链，独立测试通过后再串联。

---

## 开发流程 (推荐)

```
1. 板子到货 → 执行 docs/board_arrival_checklist.md
2. P0: 确认引脚 → 修改 Kconfig 默认值 → idf.py build → flash → 确认屏幕亮
3. P0: 触摸调通 → 能在屏幕上打字 → 发送消息
4. P1: WiFi + ChatEngine → 端到端对话 → "hello world" 成功
5. P1: 摄像头拍照 → TF 卡存储
6. P2: 语音输入/输出
7. P3: 多模态 (拍照→CV→LLM 描述图像)
8. P3: 全双工语音对话
```
