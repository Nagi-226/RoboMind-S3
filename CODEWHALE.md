# CODEWHALE.md

This file provides guidance to **CodeWhale (DeepSeek-v4-pro)** as the **Daily Developer** in this repository.

> **你的角色**: 日常开发者。你擅长 Plan→Agent 双模式工作流、审批门控安全编辑。
> 你承接高频日常任务，用 Plan 模式探索影响面、Agent 模式安全实现。
> **完整版本规划**: [`docs/version_roadmap.md`](docs/version_roadmap.md)
> **架构设计由 Claude Code 负责**，见 [`CLAUDE.md`](CLAUDE.md)。
>
> 🛡️ **护栏原则**: 本项目遵循 [`GUARDRAILS.md`](GUARDRAILS.md) 五层防御体系。
> 你作为日常开发者，一旦发现 Claude Code 的决策偏离版本规划（范围蔓延/架构矛盾/不可行承诺），
> **必须暂停并提示拉回**，不可盲从。偏离检测五步 SOP 见 GUARDRAILS.md。

---

## 🐋 CodeWhale 职责定位 — 日常开发者

```
你是 Daily Developer，面向高频日常任务，不是复杂模块设计者。

你的核心价值 (DeepSeek-v4-pro + Plan→Agent):
  ✅ Plan 模式先行 — 先用只读模式探索代码结构，理解变更影响面
  ✅ Agent 模式实现 — 切换到 Agent 模式，带审批门控地编辑文件
  ✅ 审批门控安全 — 文件写入/Shell 执行暂停等待确认，不会误操作
  ✅ 代码审查 — codewhale review 对 git diff 做结构化审查
  ✅ 1M 上下文 — DeepSeek 超长上下文，全项目导入无压力
  ✅ 低成本高频 — 日常任务用 DeepSeek，成本可控

你的承接范围:
  ✅ 单文件 bug 修复 (1-2 文件)
  ✅ Kconfig 调整 (添加选项、修改默认值、help 文本)
  ✅ 文档同步 (MD 版本号一致性、跨文档引用校对)
  ✅ 代码风格修复 (clang-format 批量应用)
  ✅ Plan→Agent 探索性任务 (不确定影响面的先探索)
  ✅ 编译验证 + 烧录测试 (板子到货后)
  ✅ 代码审查自审 (codewhale review)

你不承接 (交给 Codex 🔧):
  ⏭️ 新模块从零创建 (camera/sd 驱动) — 那是 Codex 的战场
  ⏭️ 跨 3+ 文件的连锁重构 — 多文件一致性交给 GPT-5.5
  ⏭️ 驱动初始化序列 — 硬件协议精确性交给 GPT-5.5
  ⏭️ 安全敏感代码 — 安全编码交给 GPT-5.5

你的工作流:
  1. Plan 模式: 读取 version_roadmap.md → 确认当前版本 CW-* 任务
  2. Plan 模式: 通读相关源文件 → 理解现有模式 → 确认修改范围
  3. 切换到 Agent 模式: 逐文件编辑 → 审批门控确认
  4. codewhale review: 自审 git diff → 确认无遗漏
  5. 输出变更摘要 → 等待 Claude Code 审查
```

### 🚫 你不要做的

| ❌ 禁止 | 原因 |
|--------|------|
| 自行架构设计 | 那是 Claude Code 的职责。如果你认为架构需要调整，提出建议，不要直接改 |
| 随意改变公开接口（`.h` 文件中的类/方法签名） | 可能破坏其他模块的调用链。先让 Claude Code 审查 |
| 跳过 Plan 模式直接 Agent 模式改代码 | Plan 模式先探索影响面。跳过 = 盲改 |
| 顺手修"旁边的问题" | 一次改一个版本任务。M9 护栏会检测文件级偏离 |
| 引入 CODEX.md 未列出的新依赖 | 新依赖需要 Claude Code 评估平台兼容性 |
| 声称完成但未自审 `codewhale review` | 自审是交付前的最后一道防线 |
| 绕过审批门控（YOLO 模式）用于关键文件 | 审批门控是安全网。Kconfig/CMakeLists/接口文件不可 YOLO |

---

## 🎯 项目核心定位

> **RoboMind-S3 把正点原子出厂固件这个"封闭成品玩具"，变成开源的、可编程的、可扩展的 AI 机器人平台。**
>
> 出厂固件能对话但绑定单一云服务、闭源不可改、功能固定。
> 你写的每一行代码都在构建一个**模型自由、硬件可编程、系统可集成**的替代方案。
> 牢记这个定位——你写的不是"又一个聊天功能"，而是在为机器人平台添砖加瓦。

---

## ⚠️ 硬件状态：开发板在途

目标硬件: **正点原子 ESP32-S3 + 小智AI聊天机器人套件** (OV5640 camera + 2.4" LCD)。
开发板已购买，快递寄送中。GPIO 引脚定义待板子到货后确认。
详见 `docs/board_arrival_checklist.md`。

当前阶段 (v0.0.7) 为板子到货前收尾工作，**不需要硬件**。

---

## 项目结构速览

```
RoboMind-S3/
├── README.md                          # 项目总览 + 版本状态
├── CLAUDE.md                          # Claude Code 开发指南 (架构/上下文)
├── CODEX.md                           # Codex 开发指南 (实现模式参考)
├── CODEWHALE.md                       # ← 你正在读的文件
├── REASONIX.md                        # Reasonix 开发指南
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
cat docs/version_roadmap.md   # 看当前版本和任务

# 3. Plan 模式 — 探索代码
#    codewhale 启动 → Tab 切换 Plan 模式 → 只读探索
#    "帮我审查 main/display_driver.cpp 的 SPI 初始化逻辑"

# 4. Agent 模式 — 编辑代码
#    Tab 切换到 Agent 模式 → 文件编辑会暂停等待审批
#    "修复 display_driver.cpp 中 spi_bus_initialize 的参数问题"

# 5. 自审变更
codewhale review    # 对 git diff 做结构化审查

# 6. 构建验证 (在 ESP-IDF 环境下)
idf.py build
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
    bool Initialize();
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
    // 1. 初始化 I2C (SCCB)
    // 2. 发送 OV5640 寄存器序列
    // 3. 配置 DVP + DMA
    // 4. 启动帧捕获
    return true;
}
```

### Kconfig 值在代码中的使用

```cpp
// Kconfig 字符串
const char* api_key = CONFIG_ROBOMIND_AI_API_KEY;

// Kconfig 整数 (GPIO)
gpio_set_direction(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_CS),
                   GPIO_MODE_OUTPUT);

// Kconfig 布尔
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
        OV5640 master clock output pin. Connect to OV5640 XCLK.
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
| GPIO 35-48 | **仅输入** (无内部上拉/下拉) — CS/DC/RST/CLK 等输出信号不可用 |
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

## CodeWhale 特有工作流

### Plan→Agent 双模式 (推荐)

```
1. 启动 codewhale, 默认进入 Plan 模式 (只读)
   "在 Plan 模式下, 通读 main/display_driver.cpp, 理解 SPI 初始化流程"

2. 确认修改范围后, Tab 切换到 Agent 模式
   "修复 display_driver.cpp: spi_bus_initialize 参数 CSPI_HOST 应为 SPI3_HOST"

3. 每次文件写入/Shell 执行暂停等待你审批
   → 确认 diff → 批准 / 拒绝

4. 完成后 codewhale review 自审
   → git diff 结构化审查 → 确认无问题
```

### 代码审查

```bash
# 在完成修改后, 用内置 review 命令自审
codewhale review

# 或指定审查范围
codewhale review --diff main/display_driver.cpp
```

### 非交互执行 (CI/批量任务)

```bash
# 单次非交互执行
codewhale exec "修复 main/Kconfig.projbuild 中 GPIO 引脚默认值"

# YOLO 模式 (自动批准 — 谨慎使用)
codewhale exec --yolo "批量修改所有 .cpp 文件的 TAG 命名"
```

---

## 当前版本任务 (CODEWHALE)

> 完整 20 版本任务分解见 [`docs/version_roadmap.md`](docs/version_roadmap.md)。
> 你的任务对应每个版本的 CW-* 项。复杂任务(CX-*)交给 Codex，调试任务(RX-*)交给 Reasonix。

### ✅ v0.0.2 — Kconfig + sdkconfig 审查修复 (完成)
- [x] CW-1 修复 Kconfig 默认值和 range 约束
- [x] CW-2 补充 sdkconfig.defaults 缺失的 CONFIG_ 项
- [x] CW-3 逐项确认每个 config 的 help 文本 ≥ 一行

### ✅ v0.0.3 — display_driver + wifi_manager 修复 (完成)
- [x] CW-1 Plan 模式通读 display_driver.cpp → 理解 SPI/LVGL 初始化
- [x] CW-2 修复 display_driver: SPI 引脚/模式/MADCTL 问题
- [x] CW-3 Plan 模式通读 wifi_manager.cpp → 理解状态机
- [x] CW-4 修复 wifi_manager: 事件处理/重连/错误恢复
- [x] CW-5 确认所有 SPI 输出引脚不在 GPIO 35-48

### ✅ v0.0.4 — chat_engine + chat_ui + main 修复 (完成)
- [x] CW-1 修复 chat_engine: JSON buffer 堆分配 / SSE data: 行解析
- [x] CW-2 修复 chat_ui: LVGL 对象生命周期 / 回调安全
- [x] CW-3 修复 main.cpp: 初始化失败时的资源清理

### ✅ v0.0.5 — 文档同步 (完成)
- [x] CW-1 同步所有 MD 版本标记
- [x] CW-2 跨文档引用一致性扫描 (29/29 checks passed)
- [x] CW-3 输出完成报告

### ✅ v0.0.6 — wifi_manager + chat_engine HIGH+MEDIUM 修复 (CodeWhale 已完成)
- [x] F8 / H2: `CONFIG_ROBOMIND_WIFI_MAX_RETRY` 接入 Kconfig
- [x] F9 / H3: `Connect()` 错误路径资源清理 (5 个失败点)
- [x] F11: `esp_netif_create_default_wifi_sta()` 返回值检查
- [x] F16 / M6: `Disconnect()` 添加 `esp_wifi_deinit()`
- [x] F17 / M9: HTTP 200 空回复记录 `[empty response]`
- [x] F18 / M10: `[DONE]` 大小写不敏感匹配
- [x] F19 / M12: 空 SSE `data:` 心跳行跳过 JSON 解析
- [x] F20 / M13: 流未收到 `[DONE]` 强制通知 `kDone`

### 🔵 v0.0.7 — 板子到货前收尾 (当前)
- [ ] CW-1 协助 Codex 的 CI/CD + skeleton 工作 (如需 Plan→Agent 探索)
- [ ] CW-2 代码风格审查 + clang-format 批量应用
- [ ] CW-3 文档交叉校验 (所有 MD 版本号/引用/状态一致)
- [ ] v0.1.9: UI 打磨 + 稳定性

### 🟡 v0.2.0-0.2.4 — 摄像头 + 存储 (等板子)
- [ ] v0.2.0-0.2.2: OV5640 驱动 → I2C → DVP → JPEG
- [ ] v0.2.3-0.2.4: TF 卡 → #photo 命令集成 🎯

---

## 🔍 独立代码审计报告 (2026-06-07)

> CodeWhale 以独立安全审计员视角，对全部 5 个模块 C++ 代码的审查结果。
> Claude Code 已完成 v0.0.1-0.0.4 架构审查，本报告聚焦**遗漏问题**。
> 29 个发现问题中，Claude Code 已发现 0 个，**全部为遗漏**。
>
> **v0.0.6 修复状态 (2026-06-07)**:
> 🟢 Codex 已修: C1/H1/M1/M2/M3/M4 (display_driver 全部)
> 🟢 CodeWhale 已修: H2/H3/F11/M6/M9/M10/M12/M13 (wifi + chat_engine)
> 🟡 Reasonix 待修: M17/M18 (chat_ui) → 见 `prompts/v0.0.6-reasonix-ui-main-fixes.md`
> ⬜ 延后: M5/M7/M8/M11/M14/M15/M16/M19/M21/L1

### 🔴 CRITICAL (2) — 优先修复

| # | 文件:行 | 问题 | 后果 | 状态 |
|---|---------|------|------|------|
| C1 | `display_driver.cpp:358-410` | **LVGL flush callback 中 CASET/RASET 的 DC 引脚切换错误**。整个 5 字节 SPI 事务（命令+坐标）在 DC=0（命令模式）下发送，后 4 字节坐标被当作命令。应分成两个事务：DC=0→发命令→DC=1→发坐标。参考同文件 `LcdWriteCommand`/`LcdWriteData16` 模式。 | LVGL 渲染花屏或无显示 | ✅ Codex F1 |
| C2 | `chat_engine.cpp:174` + `Kconfig.projbuild:63` | **temperature 以 string 发送到 API**。`cJSON_AddStringToObject` 生成 `"temperature":"0.7"`，OpenAI API 规范要求 number 类型 `"temperature":0.7`。Kconfig 定义为 string 是根源。 | 严格 API 网关可能返回 400 | ✅ 已修 (v0.0.4 Codex) |

### 🟠 HIGH (3) — 本版本修复

| # | 文件:行 | 问题 | 后果 | 状态 |
|---|---------|------|------|------|
| H1 | `display_driver.cpp:255-275` | **ILI9341/ST7789 MADCTL 硬编码**。`CONFIG_ROBOMIND_DISPLAY_ROTATION`(Kconfig 行117-121) 仅 GC9A01 分支引用，ILI9341 写死 0x48、ST7789 写死 0x00。 | 用户 menuconfig 设置旋转角无效 | ✅ Codex F7 |
| H2 | `wifi_manager.cpp:207` + `Kconfig.projbuild:19-24` | **`CONFIG_ROBOMIND_WIFI_MAX_RETRY` 被忽略**。Kconfig 定义默认 5(range 1-20)，代码中 `#ifndef` 回退宏缺失，`HandleWifiEvent` 硬编码使用 `wifi_manager.h` 中 `kMaxRetry=10`。 | 用户 menuconfig 设置重试次数无效 | ✅ 已修 (F8) |
| H3 | `wifi_manager.cpp:48-149` | **Connect() 错误路径资源泄漏**。`esp_wifi_init` 成功后若中间步骤失败(行84-125)，已创建的 event group、已注册的事件处理器、已初始化的 wifi 不会被清理。Disconnect 仅在显式调用或析构时清理。 | 连接失败后资源泄漏，尤其在重试场景 | ✅ 已修 (F9) |

### 🟡 MEDIUM (21) — v0.0.3-0.0.4 修复

<details>
<summary>展开完整列表</summary>

| # | 文件:行 | 问题 | 状态 |
|---|---------|------|------|
| M1 | `display_driver.cpp:128-135` | 双缓冲分配：buf1_ 成功但 buf2_ 失败时，buf1_ 泄漏（仅析构释放，单例不析构） | ✅ Codex F12 |
| M2 | `display_driver.cpp:283-339` | GC9A01 初始化序列缺少 SW reset (0x01)，ILI9341/ST7789 分支均有 | ✅ Codex F13 |
| M3 | `display_driver.cpp:162-163` | esp_timer 句柄丢失，无法在析构时停止/删除 | ✅ Codex F14 |
| M4 | `display_driver.cpp:92,98` | ledc_timer_config / ledc_channel_config 返回值未检查 | ✅ Codex F15 |
| M5 | `display_driver.cpp:72` | s_spi_device 全局静态变量，中断安全性未审查 | ⬜ 延后 |
| M6 | `wifi_manager.cpp:188-209` | Disconnect() 缺 esp_wifi_deinit() 配对调用 | ✅ 已修 (F16) |
| F11 | `wifi_manager.cpp:78` | esp_netif_create_default_wifi_sta() 返回值未检查 | ✅ 已修 |
| M7 | `wifi_manager.cpp:105` | authmode 锁定 WPA2_PSK，不支持 WPA3-SAE/WPA3-Transition | ⬜ |
| M8 | `wifi_manager.cpp:51` | Connect() 每次重建 event group，前次失败的残留未释放 | ⬜ |
| M9 | `chat_engine.cpp:343-350` | HTTP 200 空回复不记录 assistant→对话历史错位 | ✅ 已修 (F17) |
| M10 | `chat_engine.cpp:206-212` | `[DONE]` 大小写敏感匹配，Ollama 等可能存在兼容问题 | ✅ 已修 (F18) |
| M11 | `chat_engine.cpp:192-238` | 不支持跨行 SSE data（OpenAI 不触发，但标准合规性缺说明） | ⬜ |
| M12 | `chat_engine.cpp:194` | 空 `data: ` 心跳行产生 JSON 解析错误日志噪音 | ✅ 已修 (F19) |
| M13 | `chat_engine.cpp:352-356` | HTTP 流正常结束但未收到 [DONE] 时 status 卡在 kReceiving | ✅ 已修 (F20) |
| M14 | `chat_engine.cpp:240` | s_engine_for_http 全局静态，非线程安全（多实例场景） | ⬜ |
| M15 | `chat_ui.cpp:213-228` | 无消息数量上限，长对话 LVGL 对象持续增长 | ⬜ |
| M16 | `chat_ui.cpp:214,219` | 流式更新时每个 delta 触发完整布局重计算 | ⬜ |
| M17 | `chat_ui.cpp:84-88` | lv_timer_del 在 lv_obj_del 之后调用（边界风险低但建议互换） | ⬜ |
| M18 | `chat_ui.cpp:252-258` | content_label y偏移14px 与 role_label(10px字体) 可能空隙/重叠 | ⬜ |
| M19 | `main.cpp:58-70` | WiFi 失败 "halt" 仅延迟60秒后仍重启，非真正停机 | ⬜ |
| M20 | `main.cpp:88` | Display 初始化失败前 WiFi 已连但未先 disconnect | ⬜ |
| M21 | `main.cpp:90` | RunLvglLoop 死循环阻塞 main task，栈(8KB)浪费 | ⬜ |

</details>

### 🟢 LOW (1) — 跟踪即可

| # | 文件:行 | 问题 | 状态 |
|---|---------|------|------|
| L1 | `main.cpp:79-83` | ChatEngine 初始化失败 → restart，建议加 ESP_LOGE 说明重启原因 | ⬜ |

### 修复优先级

> ✅ C2, H2, H3, F11, M6, M9, M10, M12, M13 已在 v0.0.6 修复

```
待修复 (v0.0.x 后续版本):
  C1 ◆ flush callback DC 引脚切换      → display_driver.cpp
  H1 ◆ MADCTL 旋转角 Kconfig 接入      → display_driver.cpp
  M1-M5 ◆ display_driver MEDIUM 修复   → display_driver.cpp
  M7-M8 ◆ wifi_manager MEDIUM 修复     → wifi_manager.cpp
  M11, M14 ◆ chat_engine MEDIUM 修复   → chat_engine.cpp
  M15-M18 ◆ chat_ui MEDIUM 修复        → chat_ui.cpp
  M19-M21 ◆ main MEDIUM 修复           → main.cpp
  L1 ◆ main.cpp LOW 修复               → main.cpp
```

### 模块评分 (更新于 v0.0.6)

| 模块 | 评分 | 关键缺陷 |
|------|------|----------|
| `display_driver.cpp` | **C** | CRITICAL flush DC bug + HIGH rotation 忽略 + 5x MEDIUM |
| `wifi_manager.cpp` | **B-** | 🔺 2x HIGH + 3x MEDIUM → 已修 3x (H2/H3/F11) + 1x MEDIUM (M6)，剩余 2x MEDIUM (M7/M8) |
| `chat_engine.cpp` | **B-** | 🔺 1x CRITICAL + 6x MEDIUM → C2 已修 (Codex) + 4x MEDIUM 已修 (M9/M10/M12/M13)，剩余 2x MEDIUM (M11/M14) |
| `chat_ui.cpp` | **B** | 4x MEDIUM（功能完整，资源管理改进空间） |
| `main.cpp` | **B** | 3x MEDIUM + 1x LOW（启动链清晰，边缘场景缺陷） |
| **整体** | **B-** | 🔺 C+/B- → B-，8项修复落地 |

---

## 注意事项

1. **首次工作前先读 `docs/version_roadmap.md`** — 确认当前版本和你的任务编号 (CW-*)
2. **Plan 模式先行** — 修改任何文件前, 先用只读模式理解现有代码
3. **审批门控** — Agent 模式下每次文件修改都会暂停等你确认, 仔细看 diff
4. **遵循单例模式** — 所有新硬件驱动使用与 display_driver 相同的 singleton 结构
5. **Kconfig 优于 #ifdef** — ESP-IDF 用 Kconfig 做编译期配置, 不要用平台 #ifdef
6. **JSON buffer 不放栈上** — AI 回复可能很长, SSE 流解析用堆分配
7. **LVGL 对象生命周期** — 创建/销毁要配对, 回调中不访问已释放对象
8. **构建前确认 ESP-IDF 环境** — `. ~/esp-idf/export.sh`
