# RoboMind-S3
> **Version**: v0.0.8 (pre-board audio I/O + touch drivers) -> [docs/version_roadmap.md](docs/version_roadmap.md)

**正点原子 ESP32-S3 小智AI聊天机器人 — 嵌入式大模型对话终端**

基于 ESP-IDF + LVGL + WiFi，在 2.4" 显示屏上实现实时 AI 对话。对接 OpenAI / Claude / Ollama 兼容 API，支持摄像头视觉输入和语音交互。

> 🎯 **项目定位**: 正点原子套件出厂自带 AI 助手固件——它能对话，但它是封闭的。
> **RoboMind-S3 的目标不是重复造一个聊天机器人，而是把封闭的成品玩具，
> 变成一个开源的、可编程的、可扩展的 AI 机器人平台。**
>
> | 出厂固件（推测） | RoboMind-S3 |
> |---|---|
> | 绑定单一云服务 | 🔓 **模型自由** — OpenAI / Claude / Ollama 任意切换 |
> | 闭源不可改 | 📝 **全栈开源** — 每一行代码你都能改 |
> | 出厂功能固定 | 🧩 **硬件可编程** — 摄像头 / TF 卡 / GPIO 传感器任意扩展 |
> | 孤立桌面玩具 | 🚗 **系统可集成** — 预留车载桥接、ROS2、MQTT 接口 |
> | 黑盒运行 | 📚 **教育可读** — 架构清晰、文档齐全、适合学习 |

> 📋 **版本**: v0.0.4 (启动链完整) → v0.0.5 (文档同步中) → [完整版本路线图](docs/version_roadmap.md)
> ⚠️ **硬件状态**: 开发板已购买，快递寄送中。代码框架已就绪，板子到货后配置引脚即用。详见 [板子到货检查清单](docs/board_arrival_checklist.md)。

## 目标硬件

| 组件 | 型号 |
|------|------|
| 主控 | 正点原子 ESP32-S3 (16MB Flash, 8MB PSRAM) |
| 显示屏 | 2.4" TFT LCD (SPI, ILI9341 / ST7789) |
| 摄像头 | OV5640 (5MP, DVP 并口) |
| 存储 | TF 卡槽 (MicroSD) |
| 音频 | I2S MEMS 麦克风 + I2S 功放 |
| 输入 | 触摸屏 + 串口终端 |

## 功能矩阵

| 功能 | 状态 | 目标版本 |
|------|------|----------|
| WiFi 连接 + 自动重连 | ✅ 已实现 | v0.0.1 |
| LVGL 聊天 UI (对话气泡) | ✅ 已实现 | v0.0.1 |
| AI 云端对话 (SSE 流式) | ✅ 已实现 | v0.0.1 |
| 触摸屏输入 | ⚠️ 接口就绪 | v0.1.5-0.1.6 |
| 首次 AI 对话 | ⬜ 待板子 | v0.1.8 🎯 |
| OV5640 I2C 通信 | ❌ 待开发 | v0.2.0 |
| DVP DMA 帧采集 | ❌ 待开发 | v0.2.1 |
| JPEG 拍照 | ❌ 待开发 | v0.2.2 |
| TF 卡挂载 + 读写 | ❌ 待开发 | v0.2.3 |
| #photo 拍照命令 | ❌ 待开发 | v0.2.4 🎯 |
| 语音对话 (ASR+TTS) | ❌ 待开发 | v0.3.x |
| 视觉多模态 (拍照→LLM) | ❌ 待开发 | v0.4.x |

## 快速开始

```bash
# 1. 安装 ESP-IDF v5.2+
# 2. 激活环境
. ~/esp-idf/export.sh

# 3. 配置 (WiFi / API Key / 显示屏引脚)
idf.py menuconfig

# 4. 编译 & 烧录
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

详细步骤见 [CLAUDE.md](CLAUDE.md) 和 [板子到货检查清单](docs/board_arrival_checklist.md)。

## 项目结构

```
RoboMind-S3/
├── README.md                          # 项目总览 + 版本状态 + 角色分工
├── CLAUDE.md                          # Claude Code 指南 (架构/上下文)
├── CODEWHALE.md                       # CodeWhale 指南 (Plan→Agent 实现)
├── REASONIX.md                        # Reasonix 指南 (Cache-First 实现)
├── CODEX.md                           # Codex 指南 (高级资深开发者, GPT-5.5)
├── HERMES.md                           # Hermes 指南 (知识策展 + 自动化)
├── GUARDRAILS.md                      # 开发行为护栏 (五层防御)
├── CMakeLists.txt                     # ESP-IDF 项目 CMake
├── sdkconfig.defaults                 # 默认配置 (PSRAM, WiFi, mbedTLS, LVGL)
├── partitions.csv                     # Flash 分区表 (16MB, OTA 双区)
├── main/
│   ├── CMakeLists.txt                 # 主组件注册
│   ├── Kconfig.projbuild               # menuconfig 菜单 (WiFi/API/显示/触摸/音频)
│   ├── main.cpp                        # 系统入口
│   ├── wifi_manager.h/cpp              # WiFi STA 管理
│   ├── chat_engine.h/cpp               # AI 对话引擎 (SSE)
│   ├── display_driver.h/cpp            # LCD SPI 驱动 + LVGL 移植
│   ├── chat_ui.h/cpp                   # LVGL 聊天界面
│   ├── audio_io.h/cpp                  # I2S 音频 (v0.0.8 实现中)
│   ├── camera_driver.h/cpp             # OV5640 摄像头驱动 (v0.0.7 骨架)
│   └── sd_card.h/cpp                   # TF 卡驱动 (v0.0.7 骨架)
├── .github/workflows/
│   └── pr-checks.yml                   # CI/CD 门禁流水线 (v0.0.7)
└── docs/
    ├── hardware_spec.md                # 硬件规格 + GPIO 引脚表
    ├── board_arrival_checklist.md      # 板子到货执行清单
    └── version_roadmap.md              # 版本迭代路线图 (v0.0.1 → v0.2.4)
```

## 版本历史 (23 版本路线)

| Phase | 版本 | 里程碑 |
|-------|------|--------|
| 骨架 | v0.0.1 ✅ | 项目骨架 — 15 文件就绪 |
| | v0.0.2 ✅ | Kconfig + include 修复 + 分区验证 |
| | v0.0.3 ✅ | display_driver + wifi_manager 审查修复 |
| | v0.0.4 ✅ | chat_engine + chat_ui + main 审查修复 |
| | v0.0.5 ✅ | 文档同步 (29/29 checks) |
| | v0.0.6 ✅ | 交叉验证修复 (25/25) |
| | v0.0.7 ✅ | CI/CD + camera/SD 骨架 |
| | v0.0.8 🔵 | 音频 I/O + 触摸驱动实现 (当前) |
| 启动 | v0.1.0-0.1.4 ⬜ | 开箱 → GPIO → 烧录 → 屏幕亮 (等板子) |
| 对话 | v0.1.5-0.1.8 ⬜ | 触摸 → WiFi → **首次 AI 对话** 🎯 |
| 打磨 | v0.1.9 ⬜ | UI 打磨 + 异常处理 + 稳定性 |
| 拍照 | v0.2.0-0.2.2 ⬜ | OV5640 驱动 → 首张 JPEG |
| 存储 | v0.2.3-0.2.4 ⬜ | TF 卡 + **#photo 命令集成** 🎯 |

详见 [`docs/version_roadmap.md`](docs/version_roadmap.md) (23 个小版本的任务分解)

## 开发角色分工

| 角色 | 工具 | 负责 | 指南文件 |
|------|------|------|----------|
| **Claude Code** | Claude | 🏗️ 架构设计 · 版本规划 · 代码审查 · 文档 · 验收 | [`CLAUDE.md`](CLAUDE.md) |
| **CodeWhale** | DeepSeek V4 | 🐋 日常版本开发 · Plan→Agent 双模式 · 审批门控 | [`CODEWHALE.md`](CODEWHALE.md) |
| **Reasonix** | DeepSeek V4 | 🧠 长会话调试 · 复杂重构 · Cache-First 低成本 | [`REASONIX.md`](REASONIX.md) |
| **Codex** | OpenAI | 🔧 备选实现者 · OpenAI SDK 兼容任务 | [`CODEX.md`](CODEX.md) |
| **Hermes** | Nous Research | 🕊️ 知识策展 · 技能创建 · 定时自动化 · 多平台通知 | [`HERMES.md`](HERMES.md) |

> Claude Code 设计 → 实现者 (CodeWhale/Reasonix/Codex) 编码 → Hermes 捕获知识并技能化 → Claude Code 审查验收
>
> 🛡️ **护栏**: 项目遵循 [`GUARDRAILS.md`](GUARDRAILS.md) 五层防御体系，防止偏离规划、范围蔓延、不可行承诺。

## 与 Virtual-Vehicle-OS 的关系

同属 "车载-机器人" 大架构下的两个独立项目：

| | Virtual-Vehicle-OS | RoboMind-S3 |
|---|---|---|
| 定位 | 车载↔机器人桥接中间件 | 机器人侧 AI 智能节点 |
| 平台 | Linux/Windows/STM32F429 | ESP32-S3 |
| 依赖 | 无 | 无 (完全独立) |
| 后续互通 | WiFi UDP Transport ↔ | 车载桥接通信 |

## 许可

MIT License
