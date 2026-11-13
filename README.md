<p align="center">
  <h1 align="center">🤖 RoboMind-S3</h1>
  <p align="center">
    <strong>开源 · 可编程 · 可扩展的 AI 机器人平台</strong>
    <br>
    基于正点原子 ESP32-S3 + 2.4" LCD + OV5640 摄像头
  </p>
</p>

<p align="center">
  <a href="https://github.com/Nagi-226/RoboMind-S3/blob/master/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License"></a>
  <a href="docs/version_roadmap.md"><img src="https://img.shields.io/badge/version-v0.0.9-orange" alt="Version"></a>
  <a href="https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/"><img src="https://img.shields.io/badge/ESP--IDF-v5.2%2B-green" alt="ESP-IDF"></a>
  <a href=".github/workflows/pr-checks.yml"><img src="https://img.shields.io/badge/CI-5%20checks%20passing-brightgreen" alt="CI"></a>
  <img src="https://img.shields.io/badge/language-C%2B%2B17-00599C" alt="C++17">
  <img src="https://img.shields.io/badge/platform-ESP32--S3-E7352C" alt="ESP32-S3">
</p>

---

## 💡 这是什么？

**正点原子「小智AI」套件**出厂自带 AI 助手固件——它能对话，但它是封闭的：绑定单一云服务、闭源不可改、功能固定。

**RoboMind-S3 把封闭的成品玩具，变成开源的、可编程的、可扩展的 AI 机器人平台。**

| | 出厂固件（推测） | RoboMind-S3 |
|---|---|---|
| 🔓 模型 | 绑定单一云服务 | **模型自由** — OpenAI / Claude / Ollama 任意切换 |
| 📝 代码 | 闭源不可改 | **全栈开源** — MIT 许可，每一行代码你都能改 |
| 🧩 硬件 | 出厂功能固定 | **硬件可编程** — 摄像头 / TF 卡 / GPIO 传感器任意扩展 |
| 🚗 集成 | 孤立桌面玩具 | **系统可集成** — 预留车载桥接 / ROS2 / MQTT 接口 |
| 📚 教育 | 黑盒运行 | **教育可读** — 架构清晰、文档齐全、适合学习 |

---

## 🎯 目标硬件

| 组件 | 型号 | 驱动状态 |
|------|------|---------|
| 主控 | ESP32-S3 (双核 240MHz, 16MB Flash, 8MB Octal PSRAM) | ✅ 已适配 |
| 显示屏 | 2.4" TFT LCD (SPI, ILI9341 / ST7789 / GC9A01 / ST7796) | ✅ 4 款驱动就绪 |
| 触摸屏 | XPT2046 (电阻) / FT6x06 (电容) | ✅ 双协议实现 |
| 摄像头 | OV5640 (5MP, DVP 并口) | ✅ SCCB 骨架就绪 |
| 存储 | TF 卡槽 (MicroSD, SPI / SDMMC) | ✅ 完整驱动就绪 |
| 音频输入 | MEMS 麦克风 (I2S, INMP441) | ✅ I2S RX 实现 |
| 音频输出 | I2S 功放 (MAX98357) | ✅ I2S TX 实现 |
| 连接 | USB-C (烧录/调试/串口) | ✅ 标准 ESP-IDF |

---

## 🏗️ 架构

```
┌──────────────────────────────────────────────────────────┐
│              正点原子 ESP32-S3 小智AI 套件                  │
│                                                           │
│  Core 0 (UI)              Core 1 (Network + AI + I/O)    │
│  ┌─────────────┐          ┌──────────────────────┐       │
│  │ chat_ui     │          │ wifi_manager          │       │
│  │ (LVGL v8)   │◄────────►│ (STA + auto-reconnect)│       │
│  │             │          │                       │       │
│  │ display     │          │ chat_engine           │       │
│  │ _driver     │          │ (HTTP/SSE → LLM)      │       │
│  │ (SPI LCD +  │          │                       │       │
│  │  Touch)     │          │ audio_io              │       │
│  │             │          │ (I2S mic + speaker)   │       │
│  │ camera      │          │                       │       │
│  │ _driver     │          │ system_monitor        │       │
│  │ (OV5640)    │          │ (health + RSSI)       │       │
│  │             │          │                       │       │
│  │ sd_card     │          │                       │       │
│  │ (TF/FAT)    │          │                       │       │
│  └─────────────┘          └──────────────────────┘       │
│         ▲                           │                     │
│   2.4" TFT LCD              WiFi → Cloud LLM              │
│   OV5640 Camera              (OpenAI / Claude / Ollama)   │
│   TF Card (photos/logs)                                   │
└──────────────────────────────────────────────────────────┘
```

### 启动流程

```
app_main()
├── nvs_flash_init()                    # NVS 存储
├── WifiManager::Connect()              # WiFi STA 连接
├── DisplayDriver::Initialize()         # SPI LCD + LVGL + Touch
├── ChatUI::Initialize()                # LVGL 界面构建
├── ChatEngine::Initialize()            # API endpoint/key/model
├── [CameraDriver::Initialize()]        # 按需: OV5640 I2C
├── [SdCard::Mount()]                   # 按需: TF 卡挂载
├── [AudioIO::Initialize()]             # 按需: I2S 音频
├── ui->ShowSplashScreen("RoboMind-S3") # 启动画面
└── DisplayDriver::RunLvglLoop()        # 主循环 (5ms tick)
```

---

## ⚡ 功能矩阵

| 功能 | 状态 | 说明 |
|------|------|------|
| WiFi 连接 + 自动重连 | ✅ | STA 模式，事件驱动，Kconfig 可配（**已真机连通**） |
| LVGL 聊天 UI (对话气泡) | ✅ | 自适应气泡 + 滚动 + 状态栏 |
| AI 流式对话 (SSE) | ⚠️ | 代码完整，但 **API Key 仍是占位符** → 实测 HTTP 401 |
| OV5640 摄像头驱动 | ✅ | **已真机验证**，帧统计在跑（`[cam] normal f0 len=76800`） |
| **PPM 感知集成** | ✅ | 摄像头 → 感知决策，**400 帧 `ok=400 run_fail=0`** |
| **TCP 数据上报** | ✅ | 11 字段 wire 帧，**538 帧端到端 `Decode` 全过** |
| TF 卡文件系统 | ⏭️ | 代码完整，但 **SPI 引脚与 LCD 冲突**，运行时跳过挂载 |
| I2S 音频输入/输出 | ✅ | 16kHz/16-bit/mono RX+TX |
| 触摸屏 | ⚠️ | **驱动代码存在**（`display_driver.cpp:105-127`，XPT2046，Kconfig 已启用 `TOUCH_ENABLE=y`），但**功能未实测验证** |
| 系统健康监控 | ✅ | Heap / WiFi RSSI / 模块状态 |
| 本地命令 | ✅ | `#status` `#photo` `#voice` `#help` `#clear` |
| CI/CD 流水线 | ✅ | clang-format + cppcheck + Kconfig + MD + Guardrail |
| 触摸校准 | ⏳ | 等板子到货 |
| 首次端到端 AI 对话 | 🎯 | v0.1.8 里程碑 |
| JPEG 拍照 | ⏳ | v0.2.2 |
| #photo 命令集成 | 🎯 | v0.2.4 里程碑 |
| 语音对话 (ASR+TTS) | ⏳ | Phase 3 |
| 视觉多模态 (拍照→LLM) | ⏳ | Phase 4 |

---

## 🖥️ 配套 PC 端控制台

板子的感知数据除了本地小屏显示，还可以**经 WiFi 推给 PC 端的 Qt 应用**：

**[RoboMind-Console](https://github.com/Nagi-226/RoboMind_Console)** — 接收 11 字段 wire 帧并实时显示 PPM 状态

### 数据流

```
RoboMind-S3（ESP32-S3）                        RoboMind-Console（PC）
┌──────────────────────────┐                  ┌────────────────────────┐
│ 摄像头                    │                  │  QTcpServer :9000      │
│   ↓                      │                  │        ↓               │
│ ppm_producer             │                  │  frame_stream          │
│   ↓ ppm_run() ~105ms/帧  │                  │  （流式切帧，处理半包）  │
│ ppm_semantics_t          │                  │        ↓               │
│   ↓                      │                  │  MessageCodec::Decode  │
│ wire_codec               │   TCP 9000       │        ↓               │
│ 11 字段文本信封 ──────────┼─────────────────►│  PpmPanel 实时显示      │
│ [u32 BE 长度] + 文本      │                  │                        │
└──────────────────────────┘                  └────────────────────────┘
```

### 开关（**两个都默认关闭 ⇒ 默认固件零影响**）

| Kconfig | 默认 | 作用 |
|---|---|---|
| `CONFIG_ROBOMIND_PPM_PRODUCER_ENABLED` | `n` | 感知生产者（本地小屏也需要它） |
| `CONFIG_ROBOMIND_TCP_CLIENT_ENABLED` | `n` | TCP 上报（需上面同时打开） |
| `CONFIG_ROBOMIND_TCP_CLIENT_HOST` | `192.168.101.4` | PC 的 IP |
| `CONFIG_ROBOMIND_TCP_CLIENT_PORT` | `9000` | 端口 |

**关闭时零影响**：`tcp_client.cpp` 用整文件 `#if` 门控 ⇒ 默认态该 TU 的符号不在镜像里、`.bss` 零增量。

> ⚠️ 反面例子：`ppm_producer.cpp` 是「文件始终编译、只由 `main.cpp` 的 `#if` 控制是否调用」，所以它 **57 B 静态存储仍会进默认态 `.bss`**。加新功能时**照 `tcp_client` 那样整文件门控**。

### 资源代价（真机实测）

| 项 | 值 |
|---|---|
| 任务栈 | 4,096 B（峰值 **2,268 B**，余量 45%） |
| `free_internal` 下降 | **4,444 B** |
| 创建后 `largest` | **1,728 B** |
| 帧率 | **~9.5 fps（105 ms/帧）** |

⚠️ **两个必须知道的实测事实**：

1. **`largest` 掉到 1,728 B ⇒ 这是这块板上最后一个能加的任务。** 后续任何需要连续内存的功能（音频、第二个网络连接）都得先砍别的。
2. **帧率受相机约束，不是 70 ms 设计值。** `vTaskDelayUntil` 只在循环体短于周期时起作用；`esp_camera_fb_get()` 等一帧约 **105 ms > 70 ms**，所以那个 70 ms 的 delay **一直在空转**。**任何按 70 ms 推算缓冲 / 超时的计算都要改成 105 ms。**

### 协议契约的真相源

wire 格式的**真相源在 VVOS**（`MessageCodec` + wire 规格）。本仓库的 `main/wire_codec.cpp` 是**独立实现，不是共享代码**。

| 想改什么 | 真相源 | 必须同步 |
|---|---|---|
| **帧格式 / 字段数** | VVOS `MessageCodec` + 规格 | 本仓库 `wire_codec.cpp` + Console 的 `testdata/` 基准包 |
| 新增 topic | 本仓库 `wire_codec.cpp` | Console **无需改**（未知 topic 落 `RawPanel`） |
| 新增 payload 字段 | PPM profile 的 `output.fields` | `wire_codec.cpp` 的 payload 语法 + Console 解析 |

**「副本永远不是真相源」**：改 `wire_codec.cpp` 前**必须先看 `message_codec.cpp`**，只看规格不够 —— 本项目就发生过「规格写 8 字段、实现已支持 11」。

跨仓库改动的完整联动清单见 [`docs/PROJECTS.md`](docs/PROJECTS.md)。

---

## 🚀 快速开始

### 前置条件

- **ESP-IDF v5.2+** ([安装指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/))
- **正点原子 ESP32-S3 小智AI套件** (或兼容硬件)
- Python 3.10+ (仅 CI/Graphify)

### 编译 & 烧录

```bash
# 1. 克隆仓库
git clone https://github.com/Nagi-226/RoboMind-S3.git
cd RoboMind-S3

# 2. 激活 ESP-IDF
. ~/esp-idf/export.sh

# 3. 首次配置 — 填入 WiFi / API Key / GPIO 引脚
idf.py menuconfig
#   → RoboMind S3 Configuration
#     → WiFi: 填 SSID / 密码
#     → AI API: 填 API Key + endpoint
#     → Display: 选驱动芯片 + GPIO 引脚

# 4. 编译
idf.py build

# 5. 烧录并查看串口日志
idf.py -p /dev/ttyUSB0 flash monitor
```

> ⚠️ **首次使用前必读**: [`docs/board_arrival_checklist.md`](docs/board_arrival_checklist.md) — 板子到货后的逐项执行清单。

---

## 📂 项目结构

```
RoboMind-S3/
├── README.md                          # 项目总览 (你在读)
├── CLAUDE.md                          # Claude Code 架构师指南
├── CODEWHALE.md                       # CodeWhale 日常开发者指南
├── REASONIX.md                        # Reasonix 调试专家指南
├── CODEX.md                           # Codex 高级资深开发者指南
├── HERMES.md                          # Hermes 知识策展指南
├── GUARDRAILS.md                      # 五层防御护栏
├── CMakeLists.txt                     # ESP-IDF 顶层 CMake
├── sdkconfig.defaults                 # Kconfig 默认值
├── partitions.csv                     # Flash 分区表 (16MB, OTA 双区)
│
├── main/                              # 应用代码
│   ├── CMakeLists.txt                 # 组件注册
│   ├── Kconfig.projbuild              # menuconfig 菜单 (51 configs)
│   ├── main.cpp                       # 系统入口
│   ├── wifi_manager.h/cpp             # WiFi STA 管理
│   ├── chat_engine.h/cpp              # AI 对话引擎 (HTTP/SSE)
│   ├── display_driver.h/cpp           # LCD SPI + LVGL + Touch
│   ├── chat_ui.h/cpp                  # LVGL 聊天界面
│   ├── audio_io.h/cpp                 # I2S 音频输入/输出
│   ├── camera_driver.h/cpp            # OV5640 摄像头驱动
│   ├── sd_card.h/cpp                  # TF 卡文件系统
│   └── system_monitor.h/cpp           # 系统健康监控
│
├── .github/workflows/
│   └── pr-checks.yml                  # CI 门禁 (5 jobs)
│
└── docs/
    ├── version_roadmap.md             # 24 版本迭代路线图
    ├── hardware_spec.md               # 硬件规格 + GPIO 引脚表
    ├── board_arrival_checklist.md     # 板子到货检查清单
    └── reviews/                       # 代码审查报告归档
```

---

## 👥 开发团队 (5-Agent AI 协作)

> 本项目采用 **AI 多智能体协作开发** 模式，由 Claude Code 担任架构师，三名实现者按任务复杂度分级路由，Hermes 负责知识沉淀。

| 角色 | Agent | 模型 | 职责 |
|------|-------|------|------|
| 🏗️ **架构师** | Claude Code | Claude | 架构设计 · 版本规划 · 代码审查 · 验收拍板 |
| 🔧 **高级开发者** | Codex | GPT-5.5 | 复杂模块 · 协议实现 · 多文件重构 · 安全代码 |
| 🐋 **日常开发者** | CodeWhale | DeepSeek V4 | 单文件修复 · Kconfig · 编译烧录 · Plan→Agent 安全网 |
| 🧠 **调试专家** | Reasonix | DeepSeek V4 | 复杂 Bug · 内存泄漏 · 死锁 · Cache-First 长会话 |
| 🕊️ **知识策展** | Hermes | DeepSeek V4 | 技能创建 · 跨会话记忆 · 定时任务 · 多平台通知 |

```
Claude Code (架构/审查/验收)
    │
    ├── 复杂任务 (新模块/协议/多文件) → Codex (GPT-5.5)
    ├── 日常任务 (单文件/Kconfig/编译) → CodeWhale (DS V4)
    └── 调试任务 (Bug/泄漏/死锁)      → Reasonix (DS V4, Cache-First)
    │
    └── 知识沉淀 → Hermes → 技能化 → 下次一秒找到
```

🛡️ **护栏**: 项目遵循 [`GUARDRAILS.md`](GUARDRAILS.md) 六层防御体系，包含 Worktree 隔离、文件级偏离检测、偏离回拽五步 SOP。

---

## 📊 版本进度

| Phase | 版本 | 状态 | 里程碑 |
|-------|------|------|--------|
| 🔵 **骨架** | v0.0.1 ~ v0.0.9 | ✅ 9/9 | 全部模块实现 + CI/CD + 系统集成 |
| ✅ **启动** | v0.1.0 ~ v0.1.4 | ✅ **完成** | 开箱 → GPIO → 烧录 → **屏幕亮** |
| 🟡 **对话** | v0.1.5 ~ v0.1.8 | 🟡 **部分** | WiFi ✅ / 触摸 ❌（无驱动）/ AI 对话 ⚠️（API Key 未填 → HTTP 401） |
| ⬜ **打磨** | v0.1.9 | ⬜ 未开始 | UI 打磨 + 异常处理 |
| 🟡 **拍照** | v0.2.0 ~ v0.2.2 | 🟡 **部分** | OV5640 驱动 ✅（帧统计实际在跑）/ JPEG 编码 ⏳ |
| ⏭️ **存储** | v0.2.3 ~ v0.2.4 | ⏭️ **跳过** | TF 卡 SPI 引脚与 LCD 冲突（`Skipping SD auto-mount`） |
| ⬜ **语音** | v0.3.x | 未来 | ASR + TTS 全双工语音 |
| ⬜ **视觉** | v0.4.x | 未来 | 拍照 → Vision → AI 描述 |

### ⚠️ 原 roadmap 之外新增：感知链路（v0.5.x）

板子到货后按实际需求新增的三项，**不在上面 24 个版本的规划里**：

| 能力 | 状态 | 实测数据 |
|---|---|---|
| **PPM 集成** | ✅ | 摄像头 → 感知决策，400 帧 `ok=400 run_fail=0` |
| **TCP 数据上报** | ✅ | 11 字段 wire 帧，538 帧 `Decode` 全过；栈峰值 2,268 B |
| **PC 端控制台** | ✅ | [RoboMind-Console](https://github.com/Nagi-226/RoboMind_Console) — 实时显示感知结果 |

> **为什么单独列**：上面那个"9/24 版本完成"的进度条**已经不能反映实际能力了**。板子在跑感知、在发 TCP、PC 端在显示，而这些在 roadmap 里都还没有编号。

```
版本编号: ██████████░░░░░░░░░░ 9/24
实际能力: 上面 9 个 + 3 项 roadmap 外新增（感知链路 / TCP 上报 / PC 控制台）
```

> **进度条为什么不改数字**：24 个版本编号是原规划，改数字会让历史对不上。但它**已经不代表实际能力**——板子早就在跑感知和 TCP 上报了，那些在 roadmap 里没有编号。**看能力请看「功能矩阵」和「感知链路」那一节。**

详见 [`docs/version_roadmap.md`](docs/version_roadmap.md) — 24 个版本的详细任务分解。

---

## 🤝 贡献

本项目欢迎所有形式的贡献！但请注意其特殊的开发模式：

- **架构决策**由 Claude Code 统筹，实现由 Codex / CodeWhale / Reasonix 按复杂度分级执行
- 如果你想贡献代码，请先阅读相关 Agent 的指南文件（`CODEX.md` / `CODEWHALE.md` / `REASONIX.md`）
- 所有 PR 会自动触发 5 项 CI 检查（clang-format / cppcheck / Kconfig / MD / Guardrail）
- 提交信息遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范

---

## 📄 许可

[MIT License](LICENSE) © 2026 Nagi-226

---

<p align="center">
  <sub>Built with ❤️ by human + 5 AI agents · <a href="docs/version_roadmap.md">Roadmap</a> · <a href="CLAUDE.md">Architecture</a> · <a href="GUARDRAILS.md">Guardrails</a></sub>
</p>
