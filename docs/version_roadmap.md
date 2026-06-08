# RoboMind-S3 版本迭代路线图

> 从 v0.0.1 到 v0.2.0，拆分为 **20 个小版本**，每个版本可在一个会话内完成。
> Claude Code = 架构/规划/审查/验收 · Codex = 实现/构建/烧录/验证。

---

## 版本号规范

| 层级 | 含义 | 本阶段用法 |
|------|------|-----------|
| MAJOR (0) | 正式发布前 | 固定为 0 |
| MINOR | 功能里程碑 | 0 = 骨架, 1 = 板子启动, 2 = 摄像头+存储 |
| PATCH | 迭代步骤 | 每个 MINOR 内拆 5-10 个小步 |

---

## 角色分工

```
┌──────────────────────────────────────────────────────────────────┐
│                      Claude Code (Architect)                      │
│  架构设计 · 版本规划 · 代码审查 · 文档维护 · 验收拍板             │
│  "这个模块应该怎么设计？当前代码有什么问题？下个版本做什么？"      │
└────────────────────────────┬─────────────────────────────────────┘
                             │ 规划 + 审查 + 验收
                             ▼
┌──────────────────────────────────────────────────────────────────┐
│                    实现者池 (Implementers)                        │
│                                                                   │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────────┐ │
│  │    Codex 🔧      │ │   CodeWhale 🐋   │ │   Reasonix 🧠        │ │
│  │                  │ │                  │ │                      │ │
│  │ 高级资深开发者   │ │ 日常开发者       │ │ 调试专家             │ │
│  │ GPT-5.5 驱动     │ │ DS-v4-pro 驱动   │ │ DS-v4-pro Cache-First│ │
│  │                  │ │                  │ │                      │ │
│  │ 复杂模块从零创建 │ │ 单文件修复       │ │ 复杂 bug 根因分析    │ │
│  │ 协议实现(I2C/SPI)│ │ Kconfig 微调     │ │ 内存泄漏追踪         │ │
│  │ 多文件连锁重构   │ │ 文档同步         │ │ 并发/死锁问题        │ │
│  │ 安全敏感代码     │ │ Plan→Agent 安全网│ │ 10+轮迭代调试        │ │
│  │ 代码审查执行     │ │ 编译烧录验证     │ │ 崩溃转储分析         │ │
│  └─────────────────┘ └─────────────────┘ └─────────────────────┘ │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                      Hermes 🕊️                               │ │
│  │  知识策展 · 技能创建 · 定时自动化 · 多平台通知               │ │
│  │  自改进学习循环 · 跨会话记忆 · agentskills.io 标准           │ │
│  └─────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

### 什么时候用哪个实现者

> **核心原则: 按任务复杂度分级路由，不搞"谁替代谁"。**
> 三者互补: Codex = 复杂实现 · CodeWhale = 日常开发 · Reasonix = 深度调试

| 场景 | 首选 | 理由 |
|------|------|------|
| 新模块从零创建 (camera/SD/sensor) | **Codex** | 复杂协议 + 多文件协调 = GPT-5.5 最强 |
| 跨 3+ 文件的连锁重构 | **Codex** | 多文件一致性 = GPT-5.5 强项 |
| 驱动初始化序列 (SPI/I2C/DVP) | **Codex** | 硬件协议精确性 = GPT-5.5 最强 |
| 安全敏感代码 | **Codex** | 安全编码意识 = GPT-5.5 最强 |
| Claude 审查报告 CRITICAL+HIGH 执行 | **Codex** | 精准理解审查意见 |
| 单文件 bug 修复 (1-2 文件) | **CodeWhale** | Plan→Agent 安全网 + 成本低 |
| Kconfig 调整 / 文档同步 | **CodeWhale** | 简单任务，DS 足够 |
| Plan→Agent 探索性任务 | **CodeWhale** | 先读后改，审批门控安全 |
| 编译验证 + 烧录测试 | **CodeWhale** | 纯执行链，不需要深度推理 |
| 复杂 bug 根因分析 (10+ 轮) | **Reasonix** | Cache hit 90%+, 成本极低 |
| 内存泄漏 / 死锁 / 并发 | **Reasonix** | 调试专家，长会话优势 |
| 崩溃转储分析 / 性能瓶颈 | **Reasonix** | 迭代诊断 = Cache-First 最优 |
| 技能创建/知识持久化 | **Hermes** | 自改进学习循环 |
| 定时任务/多平台通知 | **Hermes** | 内置 cron + Telegram/Discord 网关 |

### 任务编号体系

| 前缀 | 角色 | 文件 |
|------|------|------|
| CC-* | Claude Code (Architect) | CLAUDE.md |
| CX-* | Codex (Senior Developer, **复杂实现首选**) | CODEX.md |
| CW-* | CodeWhale (Daily Developer, **日常任务首选**) | CODEWHALE.md |
| RX-* | Reasonix (Debug Specialist, **调试问题首选**) | REASONIX.md |
| HM-* | Hermes (Knowledge Curator, 知识+自动化) | HERMES.md |

> 版本任务列表中，每个实现任务标注推荐实现者前缀。未标注的按复杂度判断:
> 复杂(新模块/多文件/协议/安全) → CX-* · 日常(单文件/Kconfig/文档) → CW-* · 调试(bug/泄漏/死锁) → RX-*

---

## 23 版本总览

| # | 版本 | 里程碑 | 预计耗时 | 硬件 | 状态 |
|---|------|--------|----------|------|------|
| 1 | v0.0.1 | 项目骨架全模块搭建 | — | 无 | ✅ 完成 |
| 2 | v0.0.2 | Kconfig + sdkconfig 审查修复 | 30min | 无 | ✅ 完成 |
| 3 | v0.0.3 | display_driver + wifi_manager 代码审查 | 45min | 无 | ✅ 完成 |
| 4 | v0.0.4 | chat_engine + chat_ui + main 代码审查 | 45min | 无 | ✅ 完成 |
| 5 | v0.0.5 | 全量构建验证 + 文档同步 | 30min | 无 | ✅ 完成 |
| 6 | v0.0.6 | 交叉验证修复 (25/25) | 45min | 无 | ✅ 完成 |
| 7 | v0.0.7 | 板子到货前收尾 (CI/CD + skeleton) | 30min | 无 | ✅ 完成 |
| 8 | v0.0.8 | 音频 I/O + 触摸驱动实现 | 45min | 无 | 🔵 当前 |
| 9 | v0.1.0 | 开箱 + 组件确认 + 照片归档 | 30min | 板子 | ⬜ |
| 10 | v0.1.1 | GPIO 引脚表确认 (说明书→MD) | 30min | 板子 | ⬜ |
| 11 | v0.1.2 | Kconfig 引脚默认值更新 + menuconfig | 20min | 板子 | ⬜ |
| 12 | v0.1.3 | 首次编译烧录 + 串口启动日志验证 | 30min | 板子 | ⬜ |
| 13 | v0.1.4 | 显示屏背光 + SPI + LVGL 渲染 | 45min | 板子 | ⬜ |
| 14 | v0.1.5 | 触摸驱动确认 + 原始坐标数据 | 30min | 板子 | ⬜ |
| 15 | v0.1.6 | 触摸校准 + 文本输入可用 | 30min | 板子 | ⬜ |
| 16 | v0.1.7 | WiFi 连接 + IP 获取 | 20min | 板子 | ⬜ |
| 17 | v0.1.8 | 首次 AI 对话 ("你好" → 流式回复) | 30min | 板子 | ⬜ |
| 18 | v0.1.9 | UI 打磨 + 异常处理 + 稳定性 | 2h | 板子 | ⬜ |
| 19 | v0.2.0 | OV5640 I2C 通信验证 | 1h | 板子+摄像头 | ⬜ |
| 20 | v0.2.1 | DVP + DMA 原始帧采集 | 2h | 板子+摄像头 | ⬜ |
| 21 | v0.2.2 | 首张 JPEG 照片 | 1h | 板子+摄像头 | ⬜ |
| 22 | v0.2.3 | TF 卡挂载 + 文件读写 | 1h | 板子+TF卡 | ⬜ |
| 23 | v0.2.4 | #photo 命令集成 | 1.5h | 全部 | ⬜ |

---

---

## Phase 0: 基础骨架 (v0.0.x) — 板子到货前

> 目标：代码零缺陷、文档零遗漏、编译零警告，板子一到就能直接烧录。

---

### v0.0.1 ✅ 项目骨架

**状态**: 已完成

**交付物**: CMakeLists.txt, sdkconfig.defaults, partitions.csv, Kconfig.projbuild,
main.cpp, wifi_manager, chat_engine, display_driver, chat_ui, audio_io,
CLAUDE.md, CODEX.md, README.md, hardware_spec.md, board_arrival_checklist.md

**验收**: ✅ 全部 15 个文件就位，架构清晰

---

### v0.0.2 — Kconfig + sdkconfig 审查修复

**目标**: 确保 Kconfig 菜单每个配置项合理，sdkconfig.defaults 无遗漏。

**硬件**: 无 | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 逐项审查 `main/Kconfig.projbuild`：默认值、range 约束、help 文本完整性 |
| **Claude** | CC-2 | 审查 `sdkconfig.defaults`：对照 ESP-IDF 文档确认 PSRAM/mbedTLS/LVGL/WiFi 必选项 |
| **Claude** | CC-3 | 输出审查报告：每项标记 ✅/⚠️/❌，附修复建议 |
| **Codex** | CX-1 | 根据审查报告修复 Kconfig 默认值和约束 |
| **Codex** | CX-2 | 补充 sdkconfig.defaults 缺失项 |
| **Codex** | CX-3 | 逐项确认 help 文本 ≥ 一行 |

**验收标准**:
- [ ] Kconfig 每个 config 项有 help 文本
- [ ] 所有 range 约束正确 (GPIO 0-48, 避开 26-37)
- [ ] sdkconfig.defaults 无遗漏必选项
- [ ] 审查报告 0 个 ❌

---

### v0.0.3 — display_driver + wifi_manager 代码审查

**目标**: 审查显示驱动和 WiFi 模块，这两块是板子启动最关键的路径。

**硬件**: 无 | **预计**: 45min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 审查 `display_driver.h/cpp`：SPI 初始化序列、LVGL flush 回调、引脚方向配置 |
| **Claude** | CC-2 | 审查 `wifi_manager.h/cpp`：状态机完整性、重连策略、事件处理、错误恢复 |
| **Claude** | CC-3 | 输出审查报告：逻辑错误 / 内存泄漏 / 未初始化变量 / GPIO 限制违反 |
| **Codex** | CX-1 | 修复 display_driver 中发现的所有问题 |
| **Codex** | CX-2 | 修复 wifi_manager 中发现的所有问题 |
| **Codex** | CX-3 | 确认 SPI 引脚不在 GPIO 35-48 范围内（仅输入） |

**审查重点**:
- `display_driver.cpp`: `spi_bus_initialize()` 参数、`lv_disp_drv_register()` 回调、MADCTL 寄存器
- `wifi_manager.cpp`: 事件组 bit 定义、重连计数上限、WiFi 事件回调中的阻塞操作

**验收标准**:
- [ ] display_driver 所有 SPI/LVGL 调用符合 ESP-IDF 文档
- [ ] wifi_manager 状态机覆盖所有 WiFi 事件
- [ ] 所有错误路径有 `ESP_LOGE` + `return false`
- [ ] 无 GPIO 35-48 用于输出信号

---

### v0.0.4 — chat_engine + chat_ui + main 代码审查

**目标**: 审查对话引擎、UI 和主入口的代码质量。

**硬件**: 无 | **预计**: 45min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 审查 `chat_engine.h/cpp`：HTTP 客户端配置、SSE 解析器、JSON 处理、内存分配 |
| **Claude** | CC-2 | 审查 `chat_ui.h/cpp`：LVGL 对象生命周期、回调注册、内存释放 |
| **Claude** | CC-3 | 审查 `main.cpp`：初始化顺序、错误处理、FreeRTOS 任务参数 |
| **Codex** | CX-1 | 修复 chat_engine 中发现的所有问题 |
| **Codex** | CX-2 | 修复 chat_ui 中发现的所有问题（LVGL 内存泄漏为重点） |
| **Codex** | CX-3 | 修复 main.cpp 初始化顺序和错误路径 |

**审查重点**:
- `chat_engine.cpp`: JSON buffer 是否在堆上、SSE `data:` 行解析正确性、HTTP 超时处理
- `chat_ui.cpp`: `lv_obj_del()` 是否正确清理子对象、回调中是否访问已释放对象
- `main.cpp`: `nvs_flash_init()` 返回值检查、模块初始化失败时是否清理已分配资源

**验收标准**:
- [ ] 所有 JSON buffer 在堆上分配 (不在栈上放大 buffer)
- [ ] LVGL 对象创建/销毁配对正确
- [ ] main.cpp 启动链任意环节失败都有日志 + 安全退出
- [ ] 审查报告 0 个 Critical

---

### v0.0.5 — 全量构建验证 + 文档同步

**目标**: 确保项目可在 ESP-IDF 环境下编译通过，所有 MD 文档一致。

**硬件**: 无 | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 最终审查：核对 CLAUDE.md / CODEX.md / README.md / version_roadmap.md 内容一致性 |
| **Claude** | CC-2 | 确认 version_roadmap.md 中 v0.1.0 任务清单可直接执行 |
| **Codex** | CX-1 | 在 ESP-IDF 环境下执行 `idf.py fullclean build` |
| **Codex** | CX-2 | 修复所有编译错误和警告 |
| **Codex** | CX-3 | 检查 `idf.py size` 输出：固件大小是否在 ota_0 分区 (4MB) 内 |
| **Codex** | CX-4 | 输出编译报告：固件大小、RAM 使用、编译时间 |

**验收标准**:
- [ ] `idf.py build` 零错误零警告
- [ ] 固件 < 4MB (ota_0 分区限制)
- [ ] 四份 MD 文档版本信息一致，都指向 version_roadmap.md
- [ ] v0.1.0 任务清单可执行 (无模糊描述)

---

---

## Phase 1: 板子启动 (v0.1.x) — 板子到货后

> 目标：从开箱到 "AI 回复显示在屏幕上"，每个环节独立验证。

---

### v0.1.0 — 开箱 + 组件确认 + 照片归档

**目标**: 确认套件完整，拍照存档，为后续所有版本提供硬件信息基础。

**硬件**: 开发板套件 | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 审核套件清单：对照 `docs/hardware_spec.md` 逐项确认 |
| **Codex** | CX-1 | 拆箱，确认所有组件齐全 |
| **Codex** | CX-2 | 拍摄板子正面照片 (带丝印可见) |
| **Codex** | CX-3 | 拍摄板子背面照片 (带丝印可见) |
| **Codex** | CX-4 | 拍摄说明书/原理图 GPIO 引脚定义页 |
| **Codex** | CX-5 | 将照片和说明书 PDF 上传至 `docs/` 目录 |
| **Codex** | CX-6 | 填写组件确认清单 |

**验收标准**:
- [ ] 6 大组件齐全 (主板 + LCD + 摄像头 + TF卡槽 + 麦克风 + 扬声器)
- [ ] 板子正/反面照片清晰可读丝印 → `docs/board_front.jpg`, `docs/board_back.jpg`
- [ ] 说明书 GPIO 引脚页照片 → `docs/pinout_reference.jpg`
- [ ] 说明书 PDF → `docs/board_manual.pdf`

---

### v0.1.1 — GPIO 引脚表确认

**目标**: 从说明书提取所有 GPIO 引脚，填入 `docs/hardware_spec.md`。

**硬件**: 开发板 + 说明书 | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 审查 Codex 填入的 GPIO 表，检查：GPIO 范围合法性、输出引脚不在 35-48、无冲突 |
| **Claude** | CC-2 | 如有冲突或异常，给出替代引脚方案 |
| **Codex** | CX-1 | 从说明书提取 LCD SPI 引脚 (MOSI/SCLK/CS/DC/RST/BL) → 填入表 |
| **Codex** | CX-2 | 从说明书提取触摸芯片型号 + 引脚 (CS/IRQ/MISO) → 填入表 |
| **Codex** | CX-3 | 从说明书提取 OV5640 DVP 引脚 (D0-D7/XCLK/PCLK/VSYNC/HREF/SCL/SDA/PWDN/RESET) |
| **Codex** | CX-4 | 从说明书提取 TF 卡接口模式 (SPI/SDMMC) + 引脚 |
| **Codex** | CX-5 | 从说明书提取 I2S 音频引脚 (BCK/WS/DIN/DOUT) |
| **Codex** | CX-6 | 确认显示驱动 IC 型号 (看 LCD 排线丝印或说明书) |
| **Codex** | CX-7 | 确认 ESP32-S3 模组具体型号 (看金属壳丝印) |

**验收标准**:
- [ ] `docs/hardware_spec.md` 所有 `?` 已替换为实际值
- [ ] GPIO 冲突检查表全部 ✅
- [ ] 显示驱动 IC 型号已确认
- [ ] ESP32-S3 模组型号已确认 (如 N16R8)

---

### v0.1.2 — Kconfig 引脚默认值更新 + menuconfig

**目标**: 将确认的 GPIO 引脚写入 Kconfig 默认值，完成 menuconfig 配置。

**硬件**: 无 (纯配置) | **预计**: 20min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 审查修改后的 `main/Kconfig.projbuild`：默认值与 `hardware_spec.md` 一致 |
| **Claude** | CC-2 | 审查引脚分配：确保 LCD/触摸/摄像头/TF/音频 无 GPIO 冲突 |
| **Codex** | CX-1 | 修改 `main/Kconfig.projbuild` 中所有 GPIO 默认值为实际值 |
| **Codex** | CX-2 | 如显示驱动 IC 不在现有列表，添加新的 `#elif` 分支要求（提交 Claude 设计） |
| **Codex** | CX-3 | 执行 `idf.py menuconfig`：填入 WiFi SSID/密码 + API Key |
| **Codex** | CX-4 | 确认 PSRAM 已开启 (Component config → ESP PSRAM) |
| **Codex** | CX-5 | 确认显示驱动型号选择正确 |

**验收标准**:
- [ ] Kconfig 默认 GPIO 与 `hardware_spec.md` 完全一致
- [ ] `idf.py menuconfig` 无配置冲突警告
- [ ] WiFi / API Key 已配置
- [ ] PSRAM 全部开启

---

### v0.1.3 — 首次编译烧录 + 串口启动日志

**目标**: 固件成功烧录，串口输出启动日志，确认基础系统初始化通过。

**硬件**: 开发板 + USB-C | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 对照预期启动序列，审查串口日志每一步是否正常 |
| **Claude** | CC-2 | 如有异常日志，分析根因并给出排查方案 |
| **Codex** | CX-1 | `idf.py fullclean build` — 确认编译通过 |
| **Codex** | CX-2 | `idf.py -p PORT flash monitor` — 烧录 + 串口监视 |
| **Codex** | CX-3 | 捕获完整启动日志 (从 reset 到 app_main 完成) |
| **Codex** | CX-4 | 逐项确认日志中以下输出：`NVS initialized OK` / `WiFi connecting...` / `Display initializing...` |
| **Codex** | CX-5 | 如有错误，根据 Claude Code 分析修改 → 重新烧录 |
| **Codex** | CX-6 | 截取完整串口日志 → `docs/logs/v0.1.3_boot.log` |

**预期启动日志序列**:
```
I (xxx) cpu_start: Project name:     RoboMind-S3
I (xxx) nvs: NVS initialized OK
I (xxx) wifi: WiFi STA connecting to <SSID>...
I (xxx) display: SPI bus initialized on host 2
I (xxx) display: LCD controller <IC_MODEL> initialized
I (xxx) display: LVGL display driver registered
I (xxx) main: RoboMind-S3 started successfully
```

**验收标准**:
- [ ] `idf.py flash monitor` 烧录成功
- [ ] 串口输出包含 `NVS initialized OK`
- [ ] 串口输出包含 `SPI bus initialized`
- [ ] 串口输出包含 `RoboMind-S3 started successfully`
- [ ] 无 `ESP_LOGE` 错误日志
- [ ] 启动日志文件已存档

---

### v0.1.4 — 显示屏背光 + SPI + LVGL 渲染

**目标**: 屏幕点亮，LVGL 启动画面正常渲染。

**硬件**: 开发板 + LCD | **预计**: 45min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 如屏幕不亮，分析：背光 PWM、RST 引脚、SPI 模式、驱动 IC 命令序列 |
| **Claude** | CC-2 | 如显示异常 (花屏/颜色错/偏移)，分析 MADCTL / 分辨率 / RGB 顺序 |
| **Codex** | CX-1 | 上电观察屏幕：背光是否亮起？ |
| **Codex** | CX-2 | 背光不亮 → 检查 BL 引脚 GPIO 输出 + PWM 配置 |
| **Codex** | CX-3 | 屏幕显示内容？→ 拍摄屏幕照片 |
| **Codex** | CX-4 | 花屏 → 检查 SPI 模式 (MODE0/3)、时钟频率、MADCTL 寄存器 |
| **Codex** | CX-5 | 颜色异常 → 尝试 RGB/BGR 切换 |
| **Codex** | CX-6 | 正常 → 确认 LVGL "RoboMind AI" 启动画面完整渲染 |
| **Codex** | CX-7 | 屏幕照片 → `docs/screenshots/v0.1.4_display.jpg` |

**常见排查路径**:
```
屏幕全黑 → BL 引脚? RST 引脚?
屏幕全白 → CS 未拉低? SPI 模式不对?
花屏闪烁 → 时钟频率太高? 接线不良?
颜色错误 → MADCTL BGR bit? 16-bit 格式?
```

**验收标准**:
- [ ] 屏幕背光亮起
- [ ] LVGL 启动画面渲染正常 (颜色正确、无撕裂、无花屏)
- [ ] 屏幕照片存档
- [ ] 无 SPI 通信错误日志

---

### v0.1.5 — 触摸驱动确认 + 原始坐标数据

**目标**: 确认触摸芯片型号，读取原始触摸坐标。

**硬件**: 开发板 + LCD + 触摸屏 | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 根据触摸芯片型号，确认驱动选择正确 (XPT2046/FT6x06/其他) |
| **Claude** | CC-2 | 如芯片不在已知列表，查阅 datasheet 设计初始化序列 |
| **Codex** | CX-1 | 确认触摸芯片型号 (看板子丝印或说明书) |
| **Codex** | CX-2 | 在 `display_driver.cpp` 中启用正确的 `#if CONFIG_ROBOMIND_TOUCH_DRIVER_XXX` |
| **Codex** | CX-3 | 如型号不匹配已有驱动 → Claude Code 设计新驱动初始化代码 → Codex 实现 |
| **Codex** | CX-4 | 烧录后，在串口打印原始触摸坐标 `ESP_LOGI("touch", "raw: x=%d y=%d", x, y)` |
| **Codex** | CX-5 | 手指点击四角 + 中心，记录原始坐标范围 |

**验收标准**:
- [ ] 触摸芯片型号已确认
- [ ] 触摸驱动初始化无错误
- [ ] 串口能持续输出原始触摸坐标
- [ ] 坐标值随点击位置变化合理

---

### v0.1.6 — 触摸校准 + 文本输入

**目标**: 触摸坐标映射到屏幕坐标，可以通过触摸输入文字。

**硬件**: 开发板 + LCD + 触摸 | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 设计触摸校准算法：3 点校准 + NVS 存储校准参数 |
| **Claude** | CC-2 | 审查坐标映射代码 (旋转/镜像/X-Y 交换) |
| **Codex** | CX-1 | 实现触摸坐标映射 (raw → screen) |
| **Codex** | CX-2 | 实现 3 点校准流程：屏幕上依次显示校准点 → 用户点击 → 记录映射矩阵 |
| **Codex** | CX-3 | 校准参数存入 NVS (`touch_cal` namespace) |
| **Codex** | CX-4 | 实现启动时从 NVS 读取校准参数 |
| **Codex** | CX-5 | 在 `chat_ui.cpp` 文本输入区测试触摸打字 |
| **Codex** | CX-6 | 如触摸屏与 LCD 旋转方向不一致，调整映射 |

**验收标准**:
- [ ] 触摸点击位置与屏幕显示偏差 < 10px
- [ ] 可触摸点击文本输入框
- [ ] 可触摸点击发送按钮
- [ ] 校准参数持久化 (重启不丢失)
- [ ] 四角 + 中心点击测试通过

---

### v0.1.7 — WiFi 连接 + IP 获取

**目标**: WiFi 成功连接路由器，获取 IP 地址。

**硬件**: 开发板 + WiFi AP | **预计**: 20min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 如 WiFi 连不上，分析：SSID 是否正确、加密方式、信号强度、认证失败原因 |
| **Codex** | CX-1 | 观察串口 WiFi 连接日志 |
| **Codex** | CX-2 | 确认 `I (xxx) wifi: connected to <SSID>` |
| **Codex** | CX-3 | 确认 `I (xxx) wifi: got ip: xxx.xxx.xxx.xxx` |
| **Codex** | CX-4 | 在 UI 状态栏上显示 IP 地址 |
| **Codex** | CX-5 | 从 PC `ping` ESP32 IP 地址确认连通 |
| **Codex** | CX-6 | 如连不上 → 确认 SSID/密码 → 确认 2.4GHz (ESP32 不支持 5GHz) → 检查路由器 |

**验收标准**:
- [ ] WiFi 连接成功 (串口显示 `got ip`)
- [ ] UI 状态栏显示 IP 地址
- [ ] PC 可 ping 通 ESP32
- [ ] 自动重连测试：断开路由器 → 恢复 → ESP32 自动重连

---

### v0.1.8 — 首次 AI 对话

**目标**: 🎯 里程碑！发送 "你好"，AI 流式回复逐字显示在屏幕上。

**硬件**: 开发板 + WiFi + 互联网 | **预计**: 30min

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 如 API 调用失败，分析：HTTP 状态码、TLS 握手、JSON 格式、API Key |
| **Codex** | CX-1 | 在文本输入区输入 "你好" |
| **Codex** | CX-2 | 点击发送按钮 |
| **Codex** | CX-3 | 观察串口：`HTTP status: 200` |
| **Codex** | CX-4 | 观察屏幕：AI 回复气泡逐字显示 |
| **Codex** | CX-5 | 验证流式显示正常 (非一次性弹出) |
| **Codex** | CX-6 | 截图 → `docs/screenshots/v0.1.8_first_chat.jpg` |
| **Codex** | CX-7 | 如失败 → 逐项排查：WiFi 外网连通、API endpoint URL、API Key 有效、TLS 证书 |

**失败排查清单**:
```
HTTP -1  → WiFi 未连外网? DNS 解析失败?
HTTP 401 → API Key 无效? endpoint URL 不对?
HTTP 400 → 请求 JSON 格式错误? model 名称不对?
TLS err  → 证书包未启用? 检查 CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
SSE hang → 服务器无响应? 超时设置?
```

**验收标准**:
- [ ] 发送 "你好" → API 返回 200
- [ ] AI 回复以流式气泡逐字显示
- [ ] 回复内容语义合理
- [ ] 状态栏显示 `✓ Done`
- [ ] **🎉 里程碑达成：首次 AI 对话成功！**

---

### v0.1.9 — UI 打磨 + 异常处理 + 稳定性

**目标**: 从 "能用" 到 "好用"，覆盖异常路径，确保长时间运行稳定。

**硬件**: 开发板 + WiFi | **预计**: 2h

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 审查 chat_ui 交互完整度：删除键、清屏、滚屏、多行输入 |
| **Claude** | CC-2 | 审查 WiFi 重连/API 超时 UI 反馈路径 |
| **Claude** | CC-3 | 设计稳定性测试方案：连续对话 + 内存监控 |
| **Codex** | CX-1 | 改进文本输入：退格删除、回车发送、空格支持 |
| **Codex** | CX-2 | WiFi 断连时 UI 状态栏显示 "Disconnected... reconnecting" |
| **Codex** | CX-3 | API 超时 15s 后显示 "请求超时，点此重试" |
| **Codex** | CX-4 | API 返回错误时显示具体错误码和提示 |
| **Codex** | CX-5 | 实现对话气泡滚动 (聊天记录超出屏幕时自动滚) |
| **Codex** | CX-6 | 30 分钟连续对话测试：发送 30+ 条消息 |
| **Codex** | CX-7 | 每 10 条消息打印一次堆内存水位 `ESP_LOGI("mem", "free heap: %d", esp_get_free_heap_size())` |
| **Codex** | CX-8 | 背光 PWM 调光 (如硬件支持 BL 引脚 PWM) |
| **Codex** | CX-9 | 输出稳定性测试报告 |

**验收标准**:
- [ ] 退格/空格/回车 输入正常
- [ ] WiFi 断连 → UI 提示 → 自动重连 → 恢复
- [ ] API 超时有明确提示 (非卡死)
- [ ] 30 分钟连续对话无崩溃
- [ ] 堆内存波动 < 10% (无持续泄漏)
- [ ] 背光可调 (如硬件支持)

---

---

## Phase 2: 摄像头 + 存储 (v0.2.x)

> 目标：拍照、存卡、预览，三个环节逐版本验证。

---

### v0.2.0 — OV5640 I2C 通信验证

**目标**: 通过 SCCB (I2C) 读写 OV5640 寄存器，确认摄像头物理连接正确。

**硬件**: 开发板 + OV5640 摄像头模块 | **预计**: 1h

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 设计 `CameraDriver` 最小接口：仅 `Initialize()` + 寄存器读写 |
| **Claude** | CC-2 | 审核 OV5640 SCCB 时序：地址 0x3C, 寄存器 16-bit 地址, 8-bit 数据 |
| **Codex** | CX-1 | 创建 `main/camera_driver.h` — 最小接口 (单例 + Init + ReadReg/WriteReg) |
| **Codex** | CX-2 | 实现 I2C (SCCB) 初始化：配置 I2C 引脚 (SCL/SDA)、频率 100kHz |
| **Codex** | CX-3 | 实现 `WriteReg(uint16_t reg, uint8_t val)` 和 `ReadReg(uint16_t reg)` |
| **Codex** | CX-4 | 读取 OV5640 CHIP_ID 寄存器 (0x300A/0x300B → 期望 0x5640) |
| **Codex** | CX-5 | 验证：读到的 CHIP_ID == 0x5640 |
| **Codex** | CX-6 | 注册 `camera_driver.cpp` 到 `main/CMakeLists.txt` (SRCS + REQUIRES `driver`) |
| **Codex** | CX-7 | 添加 Kconfig: `ROBOMIND_CAMERA_PIN_SCL`, `ROBOMIND_CAMERA_PIN_SDA` |

**验收标准**:
- [ ] I2C 通信正常：`OV5640 CHIP_ID: 0x5640` 打印在串口
- [ ] 读写寄存器无 I2C 超时错误
- [ ] Kconfig 摄像头 I2C 引脚已配置

---

### v0.2.1 — DVP + DMA 原始帧采集

**目标**: 配置 DVP 8-bit 并口 + DMA，在 PSRAM 中捕获一帧原始数据。

**硬件**: 开发板 + OV5640 | **预计**: 2h

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 设计 DVP+DMA 配置方案：VSYNC/HSYNC/PCLK 信号、DMA 描述符链、帧缓冲大小 |
| **Claude** | CC-2 | 审查 DVP 引脚分配 (D0-D7 必须连续 ESP32-S3 GPIO 以适配 I2S 并口模式) |
| **Codex** | CX-1 | 添加 OV5640 DVP 引脚 Kconfig: XCLK/PCLK/VSYNC/HREF/D0-D7 |
| **Codex** | CX-2 | 配置 LEDC 产生 XCLK (20MHz) |
| **Codex** | CX-3 | 配置 I2S 外设为 8-bit 并口输入模式 (ESP32-S3 无硬件 DVP，用 I2S 模拟) |
| **Codex** | CX-4 | 配置 DMA 描述符链 → PSRAM 缓冲 (2 × frame_buffer_size) |
| **Codex** | CX-5 | 发送 OV5640 软复位 + 基本配置 (PLL, 时钟分频) |
| **Codex** | CX-6 | 启动帧捕获 → DMA 完成中断 → 打印 "Frame captured: N bytes" |

**验收标准**:
- [ ] DMA 传输完成，帧数据写入 PSRAM
- [ ] 串口输出 `Frame captured: XXXXX bytes`
- [ ] 帧大小在合理范围 (QVGA ≈ 150KB raw, VGA ≈ 600KB raw)
- [ ] 无 DMA 溢出或超时错误

---

### v0.2.2 — 首张 JPEG 照片

**目标**: 发送 OV5640 JPEG 初始化序列，捕获有效的 JPEG 图片。

**硬件**: 开发板 + OV5640 | **预计**: 1h

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 审查 OV5640 JPEG 寄存器初始化序列 (参考 OV5640 应用笔记) |
| **Codex** | CX-1 | 实现 `SendInitSequence()`：发送 OV5640 JPEG 模式 (320x240 或 640x480) 寄存器表 |
| **Codex** | CX-2 | 实现 `CaptureJpeg(std::vector<uint8_t>*)`：触发拍照 + 等待 DMA → 拷贝 JPEG 数据 |
| **Codex** | CX-3 | 实现 JPEG 数据搜索：在帧缓冲中查找 JPEG SOI (0xFF 0xD8) 和 EOI (0xFF 0xD9) |
| **Codex** | CX-4 | 将 JPEG 数据通过串口 hex dump 前 32 字节 |
| **Codex** | CX-5 | 验证 JPEG header: `FF D8 FF E0 ... JFIF` 或 `FF D8 FF DB` |
| **Codex** | CX-6 | 将 JPEG 数据以 hex 格式打印到串口 → PC 端脚本保存为 .jpg 文件验证 |

**验收标准**:
- [ ] 捕获的数据以 `FF D8` 开头 (JPEG SOI marker)
- [ ] 捕获的数据以 `FF D9` 结尾 (JPEG EOI marker)
- [ ] 保存为 .jpg 文件后可在 PC 上正常打开
- [ ] 照片内容为摄像头实际拍摄画面

---

### v0.2.3 — TF 卡挂载 + 文件读写

**目标**: TF 卡初始化、FAT 挂载、文件读写验证。

**硬件**: 开发板 + MicroSD 卡 | **预计**: 1h

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 设计 `SdCard` 接口：Mount/Unmount/SaveFile/ReadFile/ListDir/GetFreeSpace |
| **Claude** | CC-2 | 审核 SD 接口模式 (SPI vs SDMMC) 和引脚分配 |
| **Codex** | CX-1 | 创建 `main/sd_card.h/cpp` — 单例 + 完整接口 |
| **Codex** | CX-2 | 实现 SD 卡初始化：SPI 模式或 SDMMC 模式 (取决于硬件) |
| **Codex** | CX-3 | 实现 FAT 文件系统挂载 (`esp_vfs_fat_spiflash_mount` 或 `esp_vfs_fat_sdmmc_mount`) |
| **Codex** | CX-4 | 实现 `SaveFile(path, data, len)` / `ReadFile(path, out)` |
| **Codex** | CX-5 | 实现 `ListDir("/sdcard")` → 打印文件列表 |
| **Codex** | CX-6 | 测试：写入 `test.txt` → 读回 → 内容比对 |
| **Codex** | CX-7 | 拔出 TF 卡 → PC 读取 `test.txt` 验证 |
| **Codex** | CX-8 | 注册到 CMakeLists.txt + Kconfig (SD 接口引脚) |

**验收标准**:
- [ ] TF 卡挂载成功 (串口显示 `SD mounted: /sdcard`)
- [ ] `test.txt` 写入 → 读回内容一致
- [ ] PC 读卡器可读取 ESP32 写入的文件
- [ ] `ListDir` 正确列出目录内容
- [ ] 拔出 TF 卡时给出明确错误而非崩溃

---

### v0.2.4 — #photo 命令集成 🎯

**目标**: 用户在聊天中输入 `#photo` → 拍照 → 存 TF 卡 → 屏幕上预览缩略图。

**硬件**: 开发板 + OV5640 + TF 卡 | **预计**: 1.5h

| 角色 | # | 任务 |
|------|---|------|
| **Claude** | CC-1 | 设计 `#photo` 命令处理流程 + JPEG→LVGL 缩略图方案 |
| **Codex** | CX-1 | 在 `main.cpp` 中串联 CameraDriver + SdCard 初始化 (display 之后) |
| **Codex** | CX-2 | 在 `chat_engine.cpp` 中拦截 `#photo` 命令 → 触发拍照流程 |
| **Codex** | CX-3 | 拍照 → 以时间戳命名文件 (`photo_YYYYMMDD_HHMMSS.jpg`) → 存到 `/sdcard/photos/` |
| **Codex** | CX-4 | JPEG 解码 + 缩放至 240×180 → 显示在 LVGL 聊天区作为图片气泡 |
| **Codex** | CX-5 | 状态栏显示 "Photo saved: photo_xxx.jpg" |
| **Codex** | CX-6 | 实现 `#ls` 命令 → 列出 `/sdcard/photos/` 下文件 |
| **Codex** | CX-7 | 连续拍照 5 次 → 确认无内存泄漏 |
| **Codex** | CX-8 | TF 卡未插入时 `#photo` → 友好提示 "请插入 TF 卡" |
| **Codex** | CX-9 | 截图 → `docs/screenshots/v0.2.4_photo_preview.jpg` |

**验收标准**:
- [ ] `#photo` → 2 秒内完成拍照 + 保存 + 预览
- [ ] JPEG 文件可通过读卡器在 PC 上打开
- [ ] 缩略图在 LVGL 气泡中正常显示
- [ ] `#ls` 列出已拍照文件
- [ ] 连续 5 次拍照无崩溃、内存稳定
- [ ] TF 卡未插入时有明确提示
- [ ] **🎉 里程碑达成：RoboMind-S3 v0.2.0 完整体验！**

---

---

## 版本依赖链

```
v0.0.1 ✅ → v0.0.2 ✅ → v0.0.3 ✅ → v0.0.4 ✅ → v0.0.5 ✅ → v0.0.6 ✅ → v0.0.7 ✅ → v0.0.8   [板子到货前]
                                            │
                                            ▼
v0.1.0 → v0.1.1 → v0.1.2 → v0.1.3 → v0.1.4   [板子启动]
                                            │
              ┌─────────────────────────────┘
              ▼
v0.1.5 → v0.1.6 → v0.1.7 → v0.1.8 🎉 → v0.1.9   [对话 + 打磨]
                                            │
                                            ▼
v0.2.0 → v0.2.1 → v0.2.2                     [摄像头]
                                            │
              ┌─────────────────────────────┘
              ▼
        v0.2.3 → v0.2.4 🎉                    [存储 + 集成]
```

所有版本严格线性依赖，不可跳步——每个版本验证前一版本的输出。

---

## v0.2.4 之后展望

| 版本 | 里程碑 | 简述 |
|------|--------|------|
| v0.3.0-v0.3.4 | 语音输入/输出 | I2S 麦克风录音 + TTS 播放 (拆 5 版本) |
| v0.4.0-v0.4.4 | 视觉多模态 | 拍照 → GPT-4o Vision → AI 描述图像 (拆 5 版本) |
| v0.5.0+ | 全双工语音 | 打断式语音交互 (ASR→LLM→TTS pipeline) |
| v0.6.0+ | 车载桥接 | WiFi UDP Transport ↔ Virtual-Vehicle-OS |

---

## CI/CL 门禁流水线分析

### 可行性矩阵

| 检查项 | 自动化 | 方案 | 触发时机 |
|--------|--------|------|----------|
| 代码风格 (C++) | ✅ 可行 | `clang-format --dry-run` | 每 PR |
| 静态分析 | ✅ 可行 | `cppcheck main/` | 每 PR |
| Kconfig 完整性 | ✅ 可行 | 脚本检查 sdkconfig.defaults vs Kconfig.projbuild | 每 PR |
| CMake 语法 | ✅ 可行 | `cmake --check-stamp` | 每 PR |
| Markdown 一致性 | ✅ 可行 | 脚本校验版本号一致性 (详见下方) | 每 PR |
| **idf.py build** | ⚠️ 需自托管 | 自托管 Runner (Windows/Linux + ESP-IDF v5.2+) | 版本 tag |
| Flash + 硬件测试 | ❌ 不可行 | 需物理开发板 + 人工验证 | 手动 (每版本) |

### 推荐流水线结构

```
┌─ PR 级别 (轻量, 5min) ─────────────────────────────┐
│ 触发: 每个 PR / commit push                          │
│                                                      │
│  ① clang-format-check   → 代码风格一致               │
│  ② cppcheck             → 静态分析 (无硬件依赖)       │
│  ③ kconfig-check        → Kconfig 默认值 vs 范围     │
│  ④ md-version-check     → 所有 MD 版本号一致          │
│  ⑤ guardrail-check      → 护栏原则是否被引用          │
└──────────────────────────────────────────────────────┘
                         │
                         ▼
┌─ 版本级别 (重量, 30min) ───────────────────────────┐
│ 触发: version tag push (v*.*.*)                     │
│ 需要: 自托管 Runner (ESP-IDF v5.2+ 已安装)          │
│                                                      │
│  ① idf.py fullclean build  → 全量编译               │
│  ② idf.py size             → 固件大小 < 4MB         │
│  ③ partition-check         → 分区表 vs Flash 大小    │
│  ④ build-artifact-upload   → .bin 产物归档          │
└──────────────────────────────────────────────────────┘
```

### 当前可行性评估

| 阶段 | 可行性 | 行动 |
|------|--------|------|
| **v0.0.x (板子到货前)** | ✅ 立即可搭建 | PR 级检查全部可行，无需硬件 |
| **v0.1.x (板子启动)** | ⚠️ 需要自托管 Runner | 在开发机上安装 ESP-IDF + 注册 Runner |
| **v0.2.x (摄像头+存储)** | ⚠️ 同上 | 同上，build 依赖 camera/sd_card 组件 |

### 推荐立即执行

```bash
# 1. 在项目根创建 .github/workflows/pr-checks.yml
# 2. 包含: clang-format, cppcheck, kconfig-check, md-version-check

# 3. 版本 tag 时手动运行 build (板子到货前不需要自托管 Runner):
#    idf.py fullclean build
#    idf.py size
```

### CL (Changelog) 方案

采用 [Conventional Commits](https://www.conventionalcommits.org/) + 版本 tag 自动生成：

```
feat(display): add ST7796 driver support      → v0.1.4
fix(wifi): handle reconnect after AP reboot   → v0.1.7
refactor(chat): SSE parser heap allocation    → v0.0.4
docs(roadmap): update to 20-version plan       → v0.0.5
```

每版本发布时，`git log v0.0.3..v0.0.4 --oneline` 自动生成变更日志。

### 护栏集成

CI 流水线最后一关：`guardrail-check` 验证：
- [ ] `GUARDRAILS.md` 存在于仓库
- [ ] `CLAUDE.md` / `CODEWHALE.md` / `REASONIX.md` / `CODEX.md` 均引用护栏原则
- [ ] `docs/version_roadmap.md` 中的当前版本标记为完成时，护栏日志已填写

---

## 版本状态追踪

| 版本 | 开始 | 完成 | Claude 审查 | Codex 验证 | 备注 |
|------|------|------|-------------|------------|------|
| v0.0.1 | 2026-05 | 2026-05 | ✅ | ✅ | 骨架已完成 |
| v0.0.2 | 2026-06-07 | 2026-06-07 | ✅ | ✅ | Kconfig+include 修复 |
| v0.0.3 | 2026-06-07 | 2026-06-07 | ✅ | ✅ | display+wifi 修复 |
| v0.0.4 | 2026-06-07 | 2026-06-07 | ✅ | ✅ | chat+ui+main 修复 |
| v0.0.5 | 2026-06-07 | 2026-06-07 | ✅ | ✅ | 文档同步 (29/29 checks) |
| v0.0.6 | 2026-06-08 | 2026-06-08 | ✅ | ✅ | 交叉验证修复 (25/25) |
| v0.0.7 | 2026-06-08 | 2026-06-08 | ✅ | ✅ | CI/CD + camera/SD skeleton |
| v0.0.8 | 2026-06-08 | — | 🔵 | 🔵 | 音频 I/O + 触摸驱动 |
| v0.1.0 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.1 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.2 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.3 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.4 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.5 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.6 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.7 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.8 | — | — | ⬜ | ⬜ | 等板子 |
| v0.1.9 | — | — | ⬜ | ⬜ | 等板子 |
| v0.2.0 | — | — | ⬜ | ⬜ | 等板子 |
| v0.2.1 | — | — | ⬜ | ⬜ | 等板子 |
| v0.2.2 | — | — | ⬜ | ⬜ | 等板子 |
| v0.2.3 | — | — | ⬜ | ⬜ | 等板子 |
| v0.2.4 | — | — | ⬜ | ⬜ | 等板子 |
