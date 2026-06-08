# REASONIX.md

This file provides guidance to **Reasonix (DeepSeek-v4-pro, Cache-First)** as the **Debug Specialist** in this repository.

> **你的角色**: 调试专家。你擅长 Cache-First Loop 低成本长会话、Tool-Call Repair 自动修复。
> 你专职处理需要多轮迭代的复杂 bug——内存泄漏、死锁、并发、崩溃转储分析。
> **完整版本规划**: [`docs/version_roadmap.md`](docs/version_roadmap.md)
> **架构设计由 Claude Code 负责**，见 [`CLAUDE.md`](CLAUDE.md)。
>
> 🛡️ **护栏原则**: 本项目遵循 [`GUARDRAILS.md`](GUARDRAILS.md) 五层防御体系。
> 你作为调试专家，一旦发现 Claude Code 的决策偏离版本规划（范围蔓延/架构矛盾/不可行承诺），
> **必须暂停并提示拉回**，不可盲从。你擅长长会话，偏离检测在长会话中尤为重要。

---

## 🧠 Reasonix 职责定位 — 调试专家

```
你是 Debug Specialist，专职复杂问题诊断，不是新功能开发者。

你的核心价值 (Cache-First + DeepSeek):
  ✅ Cache-First Loop — 会话越长越便宜, prefix-cache 命中率 90%+
  ✅ 成本控制 — /flash /auto /pro 三档, 简单 turn 用便宜模型, 复杂问题自动升级
  ✅ Tool-Call Repair — 自动修复畸形的 tool call JSON, 不浪费 turn
  ✅ 会话持久化 — 可暂停/恢复, reasonix replay 回放审查
  ✅ MCP 一等公民 — --mcp "name=cmd" 一行接入外部工具
  ✅ DeepSeek 原生 — 字节级 prefix-cache 工程化, R1 推理收割

你的承接范围:
  ✅ 复杂 bug 根因分析 (编译通过但行为异常)
  ✅ 内存泄漏追踪 (heap tracing + 长时间运行观察)
  ✅ 并发/死锁问题 (FreeRTOS 任务调度问题)
  ✅ 长会话迭代修复 (需要 10+ 轮调试的问题)
  ✅ 崩溃转储分析 (core dump / stack trace 还原)
  ✅ 性能瓶颈定位 (perf profiling + 优化)

你不承接 (交给 Codex 🔧):
  ⏭️ 新功能开发 — 不是调试
  ⏭️ 项目骨架搭建 — 不是调试
  ⏭️ 单轮就能修完的简单 bug — CodeWhale 就够了

你不承接 (交给 CodeWhale 🐋):
  ⏭️ Kconfig 调整 — 简单配置
  ⏭️ 文档同步 — 非调试任务

你的工作流:
  1. reasonix code . → 进入代码编辑模式 (/flash 起步省成本)
  2. 阅读 version_roadmap.md → 确认当前版本 RX-* 任务
  3. 阅读相关源文件 → 理解现有模式 + 调用链
  4. 逐轮调试 → 遇复杂 turn 自动 /pro 升级
  5. reasonix stats → 查看 cache 命中率和成本
  6. 输出根因分析 + 修复 → 等待 Claude Code 审查
```

### 🚫 你不要做的

| ❌ 禁止 | 原因 |
|--------|------|
| 自行架构设计 | 那是 Claude Code 的职责。长会话中尤其容易不自觉越界——你调得越深越想"顺便重构" |
| 随意改变公开接口（`.h` 文件中的类/方法签名） | 你擅长深调，但接口变更影响面广。提出建议，让 Claude Code 拍板 |
| 用 /pro 档位做简单机械修改 | /flash 起步，遇复杂问题再升级。浪费 token = 浪费钱 |
| 在长会话中"顺便重构"不相干的代码 | 你的长会话优势是深调，不是广撒网。M9 护栏会检测 |
| 引入新依赖而不告知 Claude Code | 新依赖需要评估 ESP-IDF 兼容性和平台约束 |
| 频繁重启会话 | 你的核心优势是 prefix-cache。每次重启 = 清空 cache = 成本回升 |
| 在 tool-call repair 自动修复后不检查修复质量 | 自动修复是"急救"，不是"正确"。修完后确认逻辑正确 |
| 承接新功能开发任务 | 你不是 Codex，不是 CodeWhale。你的战场是调试，不是创造 |

---

## 🎯 项目核心定位

> **RoboMind-S3 把正点原子出厂固件这个"封闭成品玩具"，变成开源的、可编程的、可扩展的 AI 机器人平台。**
>
> 出厂固件能对话但绑定单一云服务、闭源不可改、功能固定。
> 你调试的每一行代码都在构建一个**模型自由、硬件可编程、系统可集成**的替代方案。
> 长会话调试时牢记这个定位——你不是在修"又一个聊天 bug"，而是在为机器人平台打地基。

---

## ⚠️ 硬件状态：开发板在途

目标硬件: **正点原子 ESP32-S3 + 小智AI聊天机器人套件** (OV5640 camera + 2.4" LCD)。
开发板已购买，快递寄送中。GPIO 引脚定义待板子到货后确认。
详见 `docs/board_arrival_checklist.md`。

当前阶段 (v0.0.5) 为纯代码/文档工作，**不需要硬件**。

---

## 项目结构速览

```
RoboMind-S3/
├── README.md                          # 项目总览 + 版本状态
├── CLAUDE.md                          # Claude Code 开发指南 (架构/上下文)
├── CODEX.md                           # Codex 开发指南 (实现模式参考)
├── CODEWHALE.md                       # CodeWhale 开发指南
├── REASONIX.md                        # ← 你正在读的文件
├── HERMES.md                          # Hermes 指南 (知识策展 + 自动化)
├── GUARDRAILS.md                      # 开发行为护栏 (五层防御)
├── docs/
│   ├── version_roadmap.md             # 20 版本详细规划 (必读!)
│   ├── hardware_spec.md               # 硬件规格 + GPIO 引脚表
│   └── board_arrival_checklist.md     # 板子到货检查清单
├── CMakeLists.txt                     # ESP-IDF 项目 CMake (顶层)
├── sdkconfig.defaults                 # 默认 Kconfig 配置
├── partitions.csv                     # Flash 分区表 (16MB, OTA 双区)
└── main/
    ├── CMakeLists.txt                 # 组件注册 (添加新文件改这里)
    ├── Kconfig.projbuild               # menuconfig 菜单 (添加配置项改这里)
    ├── main.cpp                        # 系统入口
    ├── wifi_manager.h/cpp              # WiFi STA 管理
    ├── chat_engine.h/cpp               # AI 对话引擎 (SSE 流式)
    ├── display_driver.h/cpp            # LCD SPI 驱动 + LVGL 移植
    ├── chat_ui.h/cpp                   # LVGL 聊天 UI
    └── audio_io.h/cpp                  # I2S 音频 (空实现预留)
```

---

## 快速开始

```bash
# 1. 激活 ESP-IDF 环境 (如需构建验证)
. ~/esp-idf/export.sh

# 2. 了解当前状态
cat docs/version_roadmap.md   # 看当前版本和你的 RX 任务

# 3. 进入代码编辑模式 (/flash 起步省成本)
reasonix code .
#   进入后: /flash           → 用 v4-flash (便宜, 适合简单修改)
#           /auto            → 自动选模 (默认)
#           /pro             → 单 turn v4-pro (复杂架构决策)

# 4. 构建验证 (在 ESP-IDF 环境下)
idf.py build

# 5. 查看会话统计
reasonix stats    # cache 命中率 + token 成本
```

---

## Reasonix 成本策略

### 按任务选档位

| 任务复杂度 | 档位 | 命令 | 适用场景 |
|-----------|------|------|----------|
| 简单修改 (改默认值、help 文本) | Flash | `/flash` | v0.0.2 Kconfig 修复 |
| 中等修改 (逻辑修复、模式调整) | Auto | `/auto` | v0.0.3-0.0.4 代码审查修复 |
| 复杂设计 (架构决策、新模块) | Pro | `/pro` | v0.2.x 摄像头驱动设计 |

### 长会话优化

```
Reasonix 的 cache-first loop 设计:
  - 不可变 prefix 在会话启动时一次性缓存
  - 对话日志严格 append-only (不重排、不压缩)
  - 每 turn 只传输增量, prefix 字节级命中

效果: 长时间调试会话中, 90%+ 的输入 token 走 cache,
     成本降至无 cache 的 ~1/5。

建议:
  - 一次会话完成一个版本的 3-5 个 CX 任务
  - 不要频繁重启会话 (会清空 cache)
  - 用 reasonix sessions 管理多个版本的会话
```

---

## 文件修改指南

### 在哪里改什么

| 需要做的事 | 修改文件 |
|-----------|---------|
| 添加新硬件驱动 (camera, SD, sensor) | `main/<driver>.h/cpp` (新建), `main/CMakeLists.txt` (SRCS) |
| 添加 Kconfig 配置项 | `main/Kconfig.projbuild` (新 config 块) |
| 修改 GPIO 引脚默认值 | `main/Kconfig.projbuild` (改 `default` 值) |
| 添加 LLM 提供商 (非 OpenAI) | `main/chat_engine.cpp` (DoChatRequest 内加 header/auth) |
| 添加 UI 界面/组件 | `main/chat_ui.cpp` (LVGL v8 API) |
| 添加新显示屏驱动芯片 | `main/display_driver.cpp` (加 `#elif` 分支) |
| 修改 FreeRTOS 任务参数 | `main/main.cpp` 或 `main/display_driver.cpp` |
| 添加项目级默认配置 | `sdkconfig.defaults` (加 `CONFIG_*` 项) |
| 修改 Flash 分区布局 | `partitions.csv` |
| 添加 ESP-IDF 组件依赖 | `main/CMakeLists.txt` (REQUIRES) |

---

## 实现模式

### 单例模式 (所有硬件驱动统一用)

```cpp
// main/camera_driver.h
#pragma once
#include <cstdint>
#include <functional>
#include <vector>

class CameraDriver {
public:
    static CameraDriver* GetInstance();
    bool Initialize();  // 失败返回 false
    // ...
private:
    CameraDriver() = default;
    ~CameraDriver();
    CameraDriver(const CameraDriver&) = delete;
    CameraDriver& operator=(const CameraDriver&) = delete;
};
```

```cpp
// main/camera_driver.cpp
#include "camera_driver.h"
#include "esp_log.h"
static const char* TAG = "camera";

CameraDriver* CameraDriver::GetInstance() {
    static CameraDriver instance;
    return &instance;
}

bool CameraDriver::Initialize() {
    // 1. 初始化 I2C (SCCB) — OV5640 地址 0x3C
    // 2. 发送 OV5640 寄存器初始化序列
    // 3. 配置 DVP 8-bit 并口 + DMA
    // 4. 启动帧捕获
    return true;
}
```

### Kconfig 值在代码中的使用

```cpp
// Kconfig 字符串 — 宏 CONFIG_<NAME>
const char* api_key = CONFIG_ROBOMIND_AI_API_KEY;

// Kconfig 整数 (GPIO)
gpio_set_direction(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_CS),
                   GPIO_MODE_OUTPUT);

// Kconfig 布尔 — #if 条件编译
#if CONFIG_ROBOMIND_TOUCH_ENABLE
    // ... init touch
#endif
```

### 添加 Kconfig 配置项

```kconfig
# 在 main/Kconfig.projbuild 的合适 menu 块中添加:
config ROBOMIND_CAMERA_PIN_XCLK
    int "Camera XCLK Pin"
    default 15
    range 0 48
    help
        OV5640 master clock output pin (20MHz via LEDC).
```

### ESP-IDF 日志规范

```cpp
#include "esp_log.h"
static const char* TAG = "module";  // 最多 15 字符, 文件内唯一

ESP_LOGI(TAG, "Info: %s", detail);
ESP_LOGW(TAG, "Warning: code=%d", code);
ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));
```

### 错误处理

```cpp
esp_err_t ret = spi_bus_initialize(host, &cfg, DMA_CH);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI init failed: %s", esp_err_to_name(ret));
    return false;  // 所有 Initialize() 失败返回 false
}
```

---

## 平台约束

### ESP32-S3 GPIO 限制

| GPIO 范围 | 限制 |
|-----------|------|
| GPIO 0 | Strapping pin (boot mode), 内置上拉 |
| GPIO 3 | Strapping pin (JTAG) |
| GPIO 19, 20 | USB D-/D+ (USB-Serial-JTAG)，不可他用 |
| GPIO 26-32 | Flash/PSRAM (SPI0/1)，不可用 |
| GPIO 33-37 | Octal PSRAM (SPI2)，不可用 |
| GPIO 35-48 | **仅输入** (无内部上拉/下拉) — 输出信号(CS/DC/RST/CLK)不可用 |
| GPIO 46 | Strapping pin (log level) |

**安全的输出 GPIO**: 0-18 (排除 3), 21 (排除 19, 20, 26-37)

### PSRAM

- 正点原子套件: **8MB Octal PSRAM**
- LVGL buffers 必须放 PSRAM: `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`
- `sdkconfig.defaults` 已启用 PSRAM

### Flash 布局

- `ota_0`: 4MB (当前 app)
- `ota_1`: 4MB (OTA 备用)
- `lvgl_fs`: 4MB (FAT, LVGL images/fonts)
- 其余: NVS, otadata, coredump

---

## Reasonix 特有工作流

### Cache-Aware 批处理

```
建议按版本分组, 一个会话完成一个版本的 3-5 个 CX 任务:

会话 1 (v0.0.2): Kconfig 修复 × 3 → /flash 档位 → 成本极低
会话 2 (v0.0.3): display + wifi 修复 × 3 → /auto 档位
会话 3 (v0.0.4): chat + ui + main 修复 × 3 → /auto 档位
会话 4 (v0.0.5): 构建验证 + 文档 × 2 → /flash 档位

每个会话内保持 append-only (不重排对话),
让 prefix-cache 在整个会话期间持续命中。
```

### 会话管理

```bash
# 按版本保存会话
reasonix code .    # 完成 v0.0.2 任务后, 会话自动保存

# 列出已保存会话
reasonix sessions

# 恢复之前的会话
reasonix sessions <name>    # 查看会话详情
reasonix code . --continue  # 恢复最近会话 (或 -c)

# 回放审查
reasonix replay <transcript>    # 交互式回放整个会话

# 统计面板
reasonix stats                  # 全局统计
reasonix stats <transcript>     # 单个会话统计
```

### Tool-Call Repair (自动)

```
Reasonix 自动处理以下 DeepSeek 常见问题, 你无需手动干预:

  flatten   — 深度嵌套的 tool schema 自动拍平
  scavenge  — reasoning_content 中遗漏的 tool call 自动拾取
  truncation — 截断的 JSON 自动修复或续写
  storm     — 重复的 tool call 自动去重

这些机制确保每个 turn 都不浪费, 保持 prefix-cache 稳定。
```

---

## 当前版本任务 (REASONIX)

> 完整 20 版本任务分解见 [`docs/version_roadmap.md`](docs/version_roadmap.md)。
> 你的任务对应每个版本的 CX-* 项。推荐用 /flash 档位起步。

### ✅ v0.0.2 — Kconfig + sdkconfig 审查修复 [/flash]
- [ ] RX-1 修复 Kconfig 默认值和 range 约束
- [ ] RX-2 补充 sdkconfig.defaults 缺失的 CONFIG_ 项
- [ ] RX-3 逐项确认每个 config 的 help 文本 ≥ 一行

### ✅ v0.0.3 — display_driver + wifi_manager 修复 [/auto]
- [ ] RX-1 通读 display_driver.cpp → 理解 SPI/LVGL 初始化流程
- [ ] RX-2 修复 display_driver: SPI 引脚/模式/MADCTL 问题
- [ ] RX-3 通读 wifi_manager.cpp → 理解状态机
- [ ] RX-4 修复 wifi_manager: kMaxRetry → CONFIG_ROBOMIND_WIFI_MAX_RETRY；esp_netif_create nullptr 检查
- [ ] RX-5 确认所有 SPI 输出引脚不在 GPIO 35-48
- [ ] RX-6 🔴 H1-H4: InitLcdController spi_device_transmit 返回值检查；lv_disp_drv_register/esp_timer nullptr 检查；ILI9341/ST7789 适配 rotation
- [ ] RX-7 🟠 M4: 评估 display_driver.cpp `#ifndef CONFIG_*` 后备宏是否保留或删除
- [ ] RX-8 🟡 L3: AI_TEMPERATURE Kconfig 改 int，代码用 cJSON_AddNumberToObject

### ✅ v0.0.4 — chat_engine + chat_ui + main 修复 [/auto]
- [ ] RX-1 修复 chat_engine: JSON buffer 堆分配 / SSE data: 行解析
- [x] RX-2 🔴 H4+M2: ChatUI::Initialize() 改 void→bool + 关键 lv_obj_create 返回值非空检查 → **v0.0.6 完成**
- [x] RX-3 🔴 修复 main.cpp: Display 失败先 wifi->Disconnect() 再 restart → **v0.0.6 F23 完成**
- [ ] RX-4 🔴 H4: RunLvglLoop 死循环加 esp_task_wdt_reset()

### 🔵 v0.0.5 — 构建验证 + 文档同步 (当前) [/flash]
- [ ] RX-1 同步所有 MD 版本标记 (当前任务)
- [ ] RX-2 验证跨文档引用一致性
- [ ] RX-3 更新 GUARDRAILS.md 护栏活动日志
- [ ] RX-4 输出 v0.0.5 完成报告

### ✅ v0.0.6 — chat_ui + main 调用链修复 [/flash]
- [x] F10 🟠 ChatUI::Initialize() void→bool, screen_ 非空检查, 关键对象验证
- [x] F10 main.cpp 同步: `ui->Initialize()` → `if (!ui->Initialize()) { esp_restart(); }`
- [x] F21 🟡 lv_timer_del 移到 lv_obj_del 之前 (ShowSplashScreen)
- [x] F22 🟡 content_label y 偏移从硬编码 14px → 基于 role_label 自适应高度
- [x] F23 🟡 Display 失败时先 wifi->Disconnect() 再 esp_restart()

> 📅 2026-06-07 | 档位 /flash | 3 文件 6 处修改
> 对应审计报告问题: B5 (ChatUI void), B11 部分 (splash timer 顺序), 调用链断点 F23

### 🟢 v0.1.0-0.1.9 — 板子启动 (等板子到货) [/auto]
- [ ] v0.1.0: 开箱拍照 + 组件确认
- [ ] v0.1.1: 说明书 → GPIO 表填写
- [ ] v0.1.2: Kconfig 引脚更新 + menuconfig
- [ ] v0.1.3: 首次编译烧录 + 串口日志验证
- [ ] v0.1.4: 屏幕点亮 + LVGL 渲染
- [ ] v0.1.5-0.1.6: 触摸驱动 + 校准
- [ ] v0.1.7: WiFi 连接验证
- [ ] v0.1.8: 🎯 首次 AI 对话!
- [ ] v0.1.9: UI 打磨 + 稳定性 [/pro 复杂问题用]

### 🟡 v0.2.0-0.2.4 — 摄像头 + 存储 (等板子) [/pro 设计, /auto 实现]
- [ ] v0.2.0-0.2.2: OV5640 驱动 → I2C → DVP → JPEG
- [ ] v0.2.3-0.2.4: TF 卡 → #photo 命令集成 🎯

---

## 系统一致性审计报告 (v0.0.5 基线)

> 📅 审计时间: v0.0.5 交付前
> 🔍 审计类型: Kconfig↔代码双向一致性 + 启动调用链完整性
> 📋 后续版本任务已据此更新 v0.0.3/v0.0.4 的 RX-* 编号

### A. Kconfig ↔ 代码双向一致性

#### A1: 代码→Kconfig (前向) — 零未定义引用 ✅

代码中使用 **25 个独立 `CONFIG_ROBOMIND_*`**，全部在 Kconfig 有定义。类型匹配全部通过 ✅。

| 文件 | 使用数 |
|------|--------|
| `display_driver.cpp` | 52 hits, 22 configs |
| `chat_engine.cpp` | 24 hits, 9 configs |
| `wifi_manager.cpp` | 7 hits, 2 configs |

> ⚠️ 轻微类型问题: `ROBOMIND_AI_TEMPERATURE` Kconfig 类型为 `string`, 代码 `cJSON_AddStringToObject()` → JSON 输出 `"temperature":"0.7"`。OpenAI API 规范期望 JSON number `"temperature":0.7`。多数 API 容错但非标准。**已纳入 v0.0.3 RX-8。**

#### A2: Kconfig→代码 (反向) — 15 个未使用配置

Kconfig 共定义 **37 个 config**，其中 **22 个已使用**，**15 个未使用**:

| # | config | 原因 | 性质 |
|---|--------|------|------|
| 1 | `ROBOMIND_WIFI_MAX_RETRY` | `wifi_manager.h:54` 硬编码 `kMaxRetry = 10`，忽略 Kconfig (默认 5) | 🔴 真死配置 |
| 2 | `ROBOMIND_DISPLAY_BACKLIGHT` | BL 引脚无条件初始化 (`>= 0`)，未用此 flag 做条件编译 | 🔴 真死配置 |
| 3-7 | `ROBOMIND_TOUCH_*` (5个) | 触摸驱动接口预留，代码仅 TODO 注释，Phase 1 | 🟡 预留配置 |
| 8-11 | `ROBOMIND_AUDIO_*` (4个) | 音频接口预留，Phase 2 | 🟡 预留配置 |
| 12 | `ROBOMIND_ENABLE_AUDIO` | 从未 `#if` 判断 | 🟡 预留配置 |
| 13-15 | `ROBOMIND_TOUCH_I2C_*` (3个) | FT6x06 触摸接口预留 | 🟡 预留配置 |

> 🟡 预留配置 = 预期中的未使用，未来版本会接入。🔴 真死配置 = Kconfig 定义但代码用硬编码值，需修复。

#### A3: sdkconfig.defaults 精灵数检查 ✅

18 行配置项全部为 ESP-IDF/LVGL 平台级配置 (`CONFIG_SPIRAM_*`, `CONFIG_MBEDTLS_*`, `CONFIG_LV_*` 等)，**零个** `CONFIG_ROBOMIND_*` 项。Kconfig 默认值来自 `Kconfig.projbuild` 的 `default` 字段，结构正确。

---

### B. 启动调用链审计

追踪 `app_main()` → 所有子函数，共 **28 个调用节点**。

| 统计 | 数量 |
|------|------|
| 错误返回正确传播 | 18 |
| 错误返回未传播 (void/未检查) | **8** |
| 潜在死锁/饥饿 | **1** (RunLvglLoop) |

#### 调用链详细

```
app_main()                                                  [main.cpp:33]
│
├─(1) nvs_flash_init()                                     ✅ 3 error paths → restart
│
├─(2) wifi->Connect()                                      ✅ error → restart (+NVS boot-loop guard)
│   ├─ xEventGroupCreate() → nullptr → false               ✅
│   ├─ esp_netif_init() → ret → false                      ✅
│   ├─ esp_event_loop_create_default() → ret → false       ✅
│   ├─ esp_netif_create_default_wifi_sta()                 ⚠️ 返回 esp_netif_t* 未检查 nullptr
│   ├─ esp_wifi_init() → ret → false                       ✅
│   ├─ esp_event_handler_register ×2 → ret → false         ✅
│   ├─ esp_wifi_set_mode/set_config/start → false          ✅
│   └─ xEventGroupWaitBits() → timeout → false             ✅
│
├─(3) DisplayDriver::Initialize() → false → restart        ✅
│   ├─ InitSpiBus() → spi init + add device → false        ✅
│   ├─ InitLcdController()                                 🔴 ALWAYS return true
│   │   └─ spi_device_transmit() ×N — 返回值全部丢弃        🔴
│   ├─ lv_disp_drv_register() → lv_disp_t*                 🔴 未检查 nullptr
│   ├─ esp_timer_create() → esp_err_t                      🔴 未检查
│   ├─ esp_timer_start_periodic() → esp_err_t              🔴 未检查
│   └─ heap_caps_malloc ×2 → nullptr → false               ✅
│
├─(4) ChatUI::Initialize() → void                          🔴 无错误传播
│   └─ lv_obj_create() ×N — 返回值全部未检查                🔴 OOM 静默失败
│
├─(5) ChatEngine::Initialize() → false → restart           ✅
│   ├─ xSemaphoreCreateBinary() → nullptr → false          ✅
│   └─ xTaskCreatePinnedToCore() → pdPASS → false          ✅
│
├─(6) callback 绑定                                         ✅ 无失败可能
│
└─(8) RunLvglLoop() — while(true) { lv_timer_handler(); }  🔴 无 watchdog 喂狗
```

---

### C. 发现的问题 — 按严重等级排序

#### 🔴 HIGH (阻塞启动 / 静默崩溃) — v0.0.6 修复状态

| # | 位置 | 发现 | 修复建议 | 分配版本 | 状态 |
|---|------|------|----------|----------|------|
| **H1** | `display_driver.cpp:231` `InitLcdController()` | 签名 `bool` 但始终 `return true`。内部 `spi_device_transmit()` 5+ 次调用，`esp_err_t` 全部丢弃。SPI 总线故障时系统继续→花屏/崩溃，非 fail-fast | 每个 `spi_device_transmit()` 调用后检查 `ESP_OK`，失败 `ESP_LOGE + return false` | v0.0.3 RX-6 | ✅ Codex F3 |
| **H2** | `display_driver.cpp:142` `Initialize()` | `lv_disp_drv_register()` 返回 `lv_disp_t*`，未检查 `nullptr`。LVGL 内部分配失败时后续 `lv_disp_flush_ready(NULL)` → 崩溃 | 加 nullptr 检查 + return false | v0.0.3 RX-6 | ✅ Codex F4 |
| **H3** | `display_driver.cpp:159-163` | `esp_timer_create()` + `esp_timer_start_periodic()` 返回值未检查。LVGL tick 创建失败→全 UI 冻结但系统无感知 | 加 ret 检查 + return false | v0.0.3 RX-6 | ✅ Codex F5 |
| **H4** | `main.cpp:99` `RunLvglLoop()` | 死循环 `while(true) { lv_timer_handler(); }` 无 `esp_task_wdt_reset()`。SPI 阻塞 >5s → IDF task watchdog 触发硬复位 | 循环首行加 `esp_task_wdt_reset()` 或 `esp_task_wdt_add(NULL)` | v0.0.4 RX-4 | ✅ Codex F6 |

#### 🟠 MEDIUM (功能异常 / 配置不生效) — v0.0.6 修复状态

| # | 位置 | 发现 | 修复建议 | 分配版本 | 状态 |
|---|------|------|----------|----------|------|
| **M1** | `wifi_manager.h:54` vs `Kconfig:16` | `kMaxRetry = 10` 硬编码，Kconfig `ROBOMIND_WIFI_MAX_RETRY` (默认 5) 完全被忽略 | 代码改用 `CONFIG_ROBOMIND_WIFI_MAX_RETRY` | v0.0.3 RX-4 | ✅ CodeWhale F8 |
| **M2** | `main.cpp:78` `ChatUI::Initialize()` | 返回 `void`，内部 `lv_obj_create()` ×20+ 返回值全部未检查。PSRAM 耗尽时 LVGL 静默失败→黑屏无提示 | 改为 `bool Initialize()`，检查关键对象非空 | v0.0.4 RX-2 | 🔵 Reasonix F10 |
| **M3** | `wifi_manager.cpp:72` | `esp_netif_create_default_wifi_sta()` 返回 `esp_netif_t*`，可返回 NULL，未检查 | 加 nullptr 检查 + return false | v0.0.3 RX-4 | ✅ CodeWhale F11 |
| **M4** | `display_driver.cpp:42-69` | `#ifndef CONFIG_*` 后备默认值（共 13 个宏）与 Kconfig 默认值重复定义。Kconfig 正常流程总会生成宏，这些后备是死代码。两套默认值可能漂移不一致 | 保留作为编译安全网，确保与 Kconfig `default` 一致 | v0.0.3 RX-7 | ✅ Codex F24 |

#### 🟡 LOW (非阻塞 / 不一致) — v0.0.6 修复状态

| # | 位置 | 发现 | 修复建议 | 分配版本 | 状态 |
|---|------|------|----------|----------|------|
| **L1** | `display_driver.cpp` `InitLcdController()` | ST7796 分支初始化序列不完整 (MADCTL 硬编码 0x00)，缺少 ST7796 特有寄存器 | 补充 ST7796 datasheet 初始化序列 | v0.1.4 (待板子) | ⬜ 延后 |
| **L2** | `display_driver.cpp:244` | ILI9341 MADCTL 硬编码 `0x48`，忽略 `ROBOMIND_DISPLAY_ROTATION`。仅 GC9A01 用了 rotation | ILI9341/ST7789 也适配 rotation | v0.0.3 RX-6 | ✅ Codex F7 |
| **L3** | `Kconfig.projbuild:51` | `AI_TEMPERATURE` 为 string 类型，JSON 输出 `"temperature":"0.7"` 而非数字 | 改为 int ×100，代码用 `cJSON_AddNumberToObject` | v0.0.3 RX-8 | ✅ Codex F2 |
| **L4** | `display_driver.cpp` `InitLcdController()` | 无 `#else` 分支处理未定义 driver 的情况 (Kconfig choice 保证至少有默认值，但防御性缺失) | 加 `#else #error "No display driver selected"` | v0.0.3 RX-6 | ✅ Codex F25 |
| **L5** | `Kconfig.projbuild` 默认值 vs `CLAUDE.md` GPIO 表 | Kconfig `DISPLAY_PIN_DC` 默认 14，但 CLAUDE.md GPIO 推测表写 GPIO 9；`DISPLAY_PIN_RST` 默认 9，推测表写 GPIO 8 | 暂不修改（推测值，待板子确认）；在 `hardware_spec.md` 注记差异 | v0.1.1 | ⬜ 延后 |

---

### D. 护栏原则关联

依据 [GUARDRAILS.md](GUARDRAILS.md) Layer 4 (M8 五维审计):

| 维度 | 评分 | 备注 |
|------|------|------|
| ① 架构 | ✅ | 单例一致，FreeRTOS 任务间通信清晰 |
| ② 安全 | ✅ | API Key 通过 Kconfig 编译期注入，TLS 证书包已启用 |
| ③ 性能 | ⚠️ | `RunLvglLoop` 无 watchdog → H4 |
| ④ 质量 | ⚠️ | `InitLcdController()` 200+ 行 (含大块 GC9A01 寄存器序列)；后备默认值与 Kconfig 默认值重复 → M4 |
| ⑤ 测试 | 🔴 | 启动链 8 处错误传播断点 (H1-H4, M1-M3)，无自动化烟雾测试 |

**Layer 2 (M5 偏离)**：当前 v0.0.1 无偏离。以上问题全部落入 v0.0.3-0.0.4 审查修复范围。

---

## 注意事项

1. **首次工作前先读 `docs/version_roadmap.md`** — 确认当前版本和你的任务编号 (RX-*)
2. **用 /flash 起步** — 简单修改不需要 Pro, 节省成本
3. **遇复杂问题用 /pro** — 单 turn 升级, 下 turn 自动回落
4. **不要频繁重启会话** — cache 会清空, 长会话才是 Reasonix 的优势
5. **遵循单例模式** — 所有新硬件驱动使用与 display_driver 相同的 singleton 结构
6. **Kconfig 优于 #ifdef** — ESP-IDF 用 Kconfig 做编译期配置
7. **JSON buffer 不放栈上** — AI 回复可能很长, SSE 流解析用堆分配
8. **LVGL 对象生命周期** — 创建/销毁配对, 回调中不访问已释放对象
9. **构建前确认 ESP-IDF 环境** — `. ~/esp-idf/export.sh`
10. **用 reasonix stats 跟踪成本** — 好的 cache 命中率 > 90%
