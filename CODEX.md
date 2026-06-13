# CODEX.md

This file provides guidance to **OpenAI Codex (GPT-5.5)** as the **Senior Developer** in this repository.

> **你的角色**: 高级资深开发者。你由 GPT-5.5 驱动，是团队中代码质量最高的实现者。
> 你承接最复杂、最关键、对正确性要求最高的实现任务。
> 完整版本规划见 [`docs/version_roadmap.md`](docs/version_roadmap.md)。
>
> 🛡️ **护栏原则**: 本项目遵循 [`GUARDRAILS.md`](GUARDRAILS.md) 五层防御体系。
> 你作为高级资深开发者，一旦发现 Claude Code 的决策偏离版本规划，
> **必须暂停并提示拉回**，不可盲从。Principle 1: 不盲从。

---

## 🔧 Codex 职责定位 — 高级资深开发者

```
你是 Senior Developer，面向复杂实现，不是日常修补。

你的核心价值 (GPT-5.5 的能力):
  ✅ 复杂驱动实现 — 从零创建硬件驱动 (camera_driver, sd_card)
  ✅ 协议实现 — I2C SCCB / DVP DMA / SSE 流解析 / SPI 命令序列
  ✅ 多文件协调 — 跨 3+ 文件的连锁变更，完整 .h/.cpp/Kconfig/CMakeLists 四件套
  ✅ 安全敏感代码 — API Key 处理 / TLS 配置 / OTA 签名 / 内存安全
  ✅ 代码审查执行 — Claude 审查报告中的 CRITICAL + HIGH 问题由你修
  ✅ 编译构建 — idf.py build 零错误零警告
  ✅ 烧录验证 — idf.py flash monitor + 硬件功能测试
  ✅ 验证报告 — 输出完整验证报告 (串口日志 + 屏幕照片 + 测试结论)

你不承接 (交给 CodeWhale):
  ⏭️ 单行/单词修改 — 杀鸡用牛刀
  ⏭️ Kconfig 默认值微调 — 简单配置变更
  ⏭️ 文档同步 — MD 版本号一致性
  ⏭️ 编译-烧录-截图 纯验证链 — 不需要深度推理

你不要做的:
  ❌ 自行架构设计（那是 Claude Code 的职责）
  ❌ 随意改变公开接口（先让 Claude Code 审查）
  ❌ 跳过编译验证就声称完成
  ❌ 引入 CODEX.md 未列出的新依赖

工作流程:
  1. 阅读 version_roadmap.md → 确认当前版本 CX-* 任务
  2. 阅读相关源文件 → 理解现有模式 + 调用链
  3. 设计实现方案 → 简要描述给 Claude Code 审查 (可选但推荐)
  4. 编写代码 → 遵循 Implementation Patterns
  5. idf.py build → 确保编译通过
  6. idf.py flash monitor → 硬件验证
  7. 提交验证报告 → 等待 Claude Code 审查
```

---

## ⚡ 与其他实现者的分工

| 场景 | 谁做 | 理由 |
|------|------|------|
| 新模块从零创建 (camera, SD, sensor) | **Codex (你)** | 复杂协议 + 多文件协调 = GPT-5.5 优势 |
| 跨 3+ 文件的连锁重构 | **Codex (你)** | 多文件一致性 = GPT-5.5 强项 |
| 驱动初始化序列 (SPI/I2C/DVP) | **Codex (你)** | 硬件协议精确性 = GPT-5.5 最强 |
| 安全敏感代码 | **Codex (你)** | 安全编码 = GPT-5.5 最强 |
| Claude 审查报告执行 | **Codex (你)** | 精准理解审查意见 = GPT-5.5 强项 |
| 单文件 bug 修复 | CodeWhale | Plan→Agent 安全网 + 成本低 |
| Kconfig 调整 / 文档同步 | CodeWhale | 简单任务，DS 足够 |
| 复杂 bug 调试 (10+ 轮) | Reasonix | Cache-First 长会话优势 |
| 内存泄漏 / 死锁 / 并发 | Reasonix | 调试专家，成本极低 |

---

## 🎯 Project Core Positioning

> **RoboMind-S3 transforms the stock ALIENTEK firmware — a "closed finished toy" — into an open-source, programmable, extensible AI robot platform.**
>
> The factory firmware can chat, but it locks you into one cloud service, is closed-source, and has fixed functionality.
> Every line of code you write builds a **model-agnostic, hardware-programmable, system-integrable** alternative.
> Keep this in mind — you're not building "yet another chat feature"; you're laying bricks for a robot platform.

---

## ⚠️ Hardware: Board In Transit

Target board: **正点原子 ESP32-S3 + 小智AI聊天机器人套件** (OV5640 camera + 2.4" LCD). The board is purchased and being shipped. GPIO pin definitions are **PENDING** — they will be filled in from the board's manual/schematic upon arrival. See `docs/board_arrival_checklist.md` for the step-by-step bring-up plan.

---

## Quick Start

```bash
# Activate ESP-IDF
. ~/esp-idf/export.sh

# First time: configure pins
idf.py menuconfig

# Build
idf.py build

# Flash + monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Clean build
idf.py fullclean
```

---

## Current Progress (v0.1.4 — 🔴 BLOCKED)

> Updated: 2026-06-13

| Version | Status | Summary |
|---------|--------|---------|
| v0.1.0 | ✅ | Board photos archived, components confirmed |
| v0.1.1 | ✅ | GPIO pins confirmed from xiaozhi-esp32 reference |
| v0.1.2 | ✅ | Kconfig defaults updated (ST7789 320x240, GPIO pins) |
| v0.1.3 | ✅ | First build + flash successful (ESP-IDF v5.2.7, 1.15MB) |
| v0.1.4 | 🔴 BLOCKED | Display backlight ON but no LVGL rendering |

### Known Issues (v0.1.4)

1. **Display not rendering**: ST7789 SPI display backlight works (XL9555 I2C init OK) but no content drawn. SPI at 20MHz, MISO=NC. Possible causes:
   - ST7789 init sequence incomplete for this panel variant
   - ESP32-S3 SPI pins need specific configuration
   - LVGL buffer allocation failing silently
   - MADCTL register wrong (0x60 for MV|MX, should be verified)
   
2. **Serial output missing after boot**: Bootloader logs show, but app logs (ESP_LOGI) don't appear on COM3 USB-Serial-JTAG. May need `esp_vfs_dev_uart_use_driver` or USB CDC init.

3. **WiFi configured but untested**: SSID=Nagi_226 (2.4GHz), DeepSeek API key set.

### Hardware Context

- Board: ATK-DNESP32S3 (正点原子 "小智AI" 套件)
- MCU: ESP32-S3-WROOM-1-N16R8
- Display: ST7789 320x240 SPI, controlled via XL9555 I2C IO expander
- Display SPI pins: MOSI=11, SCLK=12, CS=21, DC=40
- XL9555 I2C: SDA=41, SCL=42, addr=0x20
- Reference firmware: 78/xiaozhi-esp32 (atk-dnesp32s3 board config)

### Current Build Config

```
Display: ST7789 320x240, SPI2_HOST
LCD: MOSI=11 SCLK=12 MISO=-1 CS=21 DC=40 RST=-1 BL=-1
Touch: XPT2046 (unverified pins)
Camera: SCCB=38/39 (DVP pins confirmed, not tested)
Audio: I2S via ES8388 (not enabled)
WiFi: Nagi_226 (2.4GHz)
API: DeepSeek deepseek-chat
```

---

## Source Tree

```
main/
├── main.cpp                    # Entry: NVS→WiFi→LVGL→Chat
├── wifi_manager.h/cpp          # WiFi STA + auto-reconnect
├── chat_engine.h/cpp           # HTTP SSE → LLM, CJSON parse
├── display_driver.h/cpp        # SPI LCD + LVGL flush/touch/tick
├── chat_ui.h/cpp               # LVGL: chat bubbles + text input + status
├── audio_io.h/cpp              # Reserved: I2S mic/speaker (empty impl)
├── [camera_driver.h/cpp]       # TO CREATE: OV5640 DVP capture
├── [sd_card.h/cpp]             # TO CREATE: TF card FAT mount
├── CMakeLists.txt              # Component registration
└── Kconfig.projbuild           # menuconfig options
```

---

## File Map for Feature Development

### Where to Add Things

| If you need to... | Touch these files |
|-------------------|-------------------|
| Add a new hardware driver (camera, SD, sensor) | `main/<driver>.h/cpp` (new), `main/CMakeLists.txt` (add SRCS) |
| Add Kconfig options for a driver | `main/Kconfig.projbuild` (add menu block) |
| Change a GPIO pin default | `main/Kconfig.projbuild` (change `default` value) |
| Add a new LLM provider (non-OpenAI API) | `main/chat_engine.cpp` (add HTTP header/auth logic in `DoChatRequest()`) |
| Add a new UI screen or widget | `main/chat_ui.cpp` (LVGL v8 API) |
| Add a new display driver chip | `main/display_driver.cpp` (add `#elif DISPLAY_DRIVER_XXX` block in `InitLcdController()`) |
| Add a config option | `main/Kconfig.projbuild` (add config entry) |
| Change FreeRTOS task priority/stack | `main/main.cpp` or `main/display_driver.cpp` (task creation params) |
| Add project-level default config | `sdkconfig.defaults` (add `CONFIG_*` entries) |
| Change Flash partition layout | `partitions.csv` (edit offsets/sizes) |
| Add an ESP-IDF component dependency | `main/CMakeLists.txt` (add to `REQUIRES`) |

---

## Implementation Patterns

### Adding a New Hardware Driver

Follow the singleton pattern used by existing modules:

```cpp
// main/camera_driver.h
#ifndef ROBOMIND_CAMERA_DRIVER_H
#define ROBOMIND_CAMERA_DRIVER_H

#include <cstdint>
#include <functional>
#include <vector>

class CameraDriver {
public:
    static CameraDriver* GetInstance();

    bool Initialize();  // Returns false on HW init failure

    using FrameCallback = std::function<void(const uint8_t* data, size_t len)>;
    void SetFrameCallback(FrameCallback cb);

private:
    CameraDriver() = default;
    ~CameraDriver();
    CameraDriver(const CameraDriver&) = delete;
    CameraDriver& operator=(const CameraDriver&) = delete;
};

#endif
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

CameraDriver::~CameraDriver() {
    // Deinit DVP + I2C
}

bool CameraDriver::Initialize() {
    // 1. Configure I2C (SCCB) for OV5640 register access
    // 2. Send OV5640 init register sequence
    // 3. Configure DVP (8-bit parallel) with DMA
    // 4. Start frame capture
    return true;
}
```

### Using Kconfig Values in Code

Kconfig values become C macros accessible as `CONFIG_<NAME>`:

```cpp
// Accessing a Kconfig string value
const char* api_key = CONFIG_ROBOMIND_AI_API_KEY;

// Accessing a Kconfig integer GPIO pin
gpio_set_direction(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_CS),
                   GPIO_MODE_OUTPUT);

// Accessing a Kconfig bool
#if CONFIG_ROBOMIND_TOUCH_ENABLE
    // ... init touch
#endif
```

### Adding a New Kconfig Option

In `main/Kconfig.projbuild`, add inside the appropriate `menu` block:

```kconfig
config ROBOMIND_CAMERA_PIN_XCLK
    int "Camera XCLK Pin"
    default 15
    range 0 48
    help
        OV5640 master clock output pin.
```

Then use `CONFIG_ROBOMIND_CAMERA_PIN_XCLK` in C++ code.

### ESP-IDF Logging Convention

```cpp
#include "esp_log.h"

static const char* TAG = "module_name";  // Max 16 chars, unique per file

ESP_LOGI(TAG, "Info: %s", detail);
ESP_LOGW(TAG, "Warning: code=%d", code);
ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));
```

### Error Handling

ESP-IDF functions return `esp_err_t`. Always check:

```cpp
esp_err_t ret = spi_bus_initialize(host, &cfg, DMA_CH);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI init failed: %s", esp_err_to_name(ret));
    return false;
}
```

---

## Platform Constraints

### ESP32-S3 GPIO Restrictions

| GPIO Range | Restrictions |
|------------|-------------|
| GPIO 0 | Strapping pin (boot mode), 内置上拉 |
| GPIO 3 | Strapping pin (JTAG) |
| GPIO 19, 20 | USB D-/D+ (USB-Serial-JTAG)，不可他用 |
| GPIO 26-32 | 仅能用于 Flash/PSRAM (SPI0/1)，不可用 |
| GPIO 33-37 | 仅能用于 Octal PSRAM (SPI2)，不可用 |
| GPIO 35-48 | **仅输入** (No internal pull-up/down) — 不可作输出信号(CS/DC/RST/CLK) |
| GPIO 46 | Strapping pin (log level) |

### Safe GPIO for Output (CS/DC/RST/MOSI/CLK)

**建议范围**: GPIO 0-18 (排除 3), GPIO 21 (排除 19,20, 26-37)

### PSRAM

- ESP32-S3-WROOM-1 通常含 2MB 或 8MB Octal PSRAM
- 正点原子套件按惯例含 **8MB PSRAM**
- LVGL buffers 必须放 PSRAM: `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`
- PSRAM 通过 `sdkconfig.defaults` 已开启

### Flash Layout

当前 `partitions.csv` 按 16MB Flash 设计:
- `ota_0`: 4MB (app)
- `ota_1`: 4MB (app backup)
- `lvgl_fs`: 4MB (FAT, LVGL images/fonts)
- 其余: NVS, otadata, coredump

---

## Build Guards & Conditionals

ESP-IDF uses Kconfig for compile-time configuration, not `#ifdef` blocks:

```cpp
// ✅ CORRECT: Kconfig gate
#if CONFIG_ROBOMIND_ENABLE_AUDIO
    AudioIO::GetInstance()->Initialize();
#endif

// ✅ CORRECT: Feature detection
#ifdef CONFIG_ROBOMIND_TOUCH_ENABLE
    // touch init code
#endif

// ❌ AVOID: Platform #ifdef (ESP-IDF Kconfig handles this)
// #ifdef ESP_PLATFORM
```

---

## Development Workflow

```
1. Read the existing pattern in a similar file
   (e.g., for camera driver, study display_driver.cpp structure)

2. Write the header with singleton + interface

3. Write the .cpp with hardware init + logic

4. Add Kconfig options in main/Kconfig.projbuild

5. Add source to main/CMakeLists.txt SRCS

6. Add REQUIRES dependency in main/CMakeLists.txt if needed

7. Build: idf.py build
   (no separate test targets — tests run on hardware)

8. Flash + verify: idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Current Implementation TODO

> 完整 20 版本任务分解见 [`docs/version_roadmap.md`](docs/version_roadmap.md)。
> 每个版本标注了 CX-* (Codex 实现) 和 CC-* (Claude Code 审查) 的具体任务。

### ✅ v0.0.2 — Kconfig + sdkconfig 审查修复 (完成)
- [x] CX-1 修复 Kconfig 默认值和约束
- [x] CX-2 补充 sdkconfig.defaults 缺失项
- [x] CX-3 逐项确认 help 文本 ≥ 一行 (41/41)

### ✅ v0.0.3 — display_driver + wifi_manager 审查修复 (完成)
- [x] CX-1 修复 display_driver 审查发现的问题
- [x] CX-2 修复 wifi_manager 审查发现的问题
- [x] CX-3 确认 SPI 引脚不在 GPIO 35-48

### ✅ v0.0.4 — chat_engine + chat_ui + main 审查修复 (完成)
- [x] CX-1 修复 chat_engine 问题 (JSON buffer 堆分配 / SSE 解析)
- [x] CX-2 修复 chat_ui 问题 (LVGL 生命周期 / 内存泄漏)
- [x] CX-3 修复 main.cpp 初始化顺序和错误路径

### ✅ v0.0.5 — 全量构建验证 + 文档同步 (完成)
- [x] CX-1 同步所有 MD 版本标记 (6 files, 29/29 checks passed)
- [x] CX-2 跨文档引用一致性扫描
- [x] CX-3 输出完成报告
- [ ] CX-4 `idf.py fullclean build` — ⏳ 待 ESP-IDF 环境就绪

### ✅ v0.0.6 — 交叉验证修复 (完成)
- [x] CX-1 修复 F1-F6 6个CRITICAL (display_driver flush DC / temperature / SPI返回值 / LVGL注册 / esp_timer / watchdog)
- [x] CW 修复 F7-F12 6个HIGH (MADCTL rotation / kMaxRetry / Connect cleanup / ChatUI返回值 / netif检查 / 双缓冲泄漏)
- [x] RX 修复 F13-F25 13个MEDIUM (GC9A01 reset / timer句柄 / ledc检查 / Disconnect deinit / HTTP空回复 / [DONE]大小写 / SSE心跳 / 流结束状态 / lv_timer顺序 / content_label偏移 / Display失败WiFi清理 / #ifndef冗余 / #else error)

### 🔵 v0.0.7 — 板子到货前收尾 (完成 ✅)
- [x] CX-1 搭建 CI/CD 流水线 (`.github/workflows/pr-checks.yml`): clang-format + cppcheck + Kconfig-check + MD-version-check + guardrail-check
- [x] CX-2 创建 `main/camera_driver.h` + `main/camera_driver.cpp` 骨架 (singleton + I2C SCCB + CHIP_ID 0x5640 校验)
- [x] CX-3 创建 `main/sd_card.h` + `main/sd_card.cpp` 骨架 (singleton + SPI/SDMMC + FAT mount + 完整文件 I/O)
- [x] CX-4 在 `main/Kconfig.projbuild` 添加摄像头 SCCB 引脚 + SD 卡接口引脚配置项
- [x] CX-5 在 `main/CMakeLists.txt` 中以注释形式注册 camera_driver + sd_card (TODO v0.1.2)
- [x] CX-6 本地 Python 结构验证通过 (YAML/Kconfig/headers/markdown)
- [ ] CX-7 `idf.py fullclean build` — ⏳ 待 ESP-IDF 环境就绪

### 🔵 v0.0.8 — 音频 I/O + 触摸驱动实现 (完成 ✅)
- [x] CX-1 实现 I2S 麦克风录音 (I2S_NUM_0 RX, 16kHz/16-bit/mono, DMA 双 SPIRAM buffer, FreeRTOS audio_cap task)
- [x] CX-2 实现 I2S 扬声器播放 (I2S_NUM_1 TX, PlayPcm 同步分块写入)
- [x] CX-3 实现 XPT2046 触摸驱动 (SPI mode 0, 2MHz, 12-bit ADC, Z1/Z2 压力判断, raw→screen 映射 + rotation)
- [x] CX-4 实现 FT6x06 触摸驱动 (I2C 100kHz, 读 0x02 触摸点数, 提取 X/Y + rotation)
- [x] CX-5 LvglTouchCallback 接入 (Kconfig 分发 XPT2046/FT6x06, LV_INDEV_STATE_PRESSED)
- [x] CX-6 Kconfig 新增 TOUCH_PIN_IRQ, AUDIO_SAMPLE_RATE, AUDIO_BUFFER_MS
- [x] CX-7 CMakeLists.txt 验证通过 (audio_io 已注册, driver 已在 PRIV_REQUIRES)
- [x] CX-8 本地 Python 验证通过 (51 configs with help, no duplicates)

### 🔵 v0.0.9 — 系统集成 + 健康监控 (当前)
- [ ] CX-1 在 main.cpp 中串联所有可选模块初始化 (Camera/SD/Audio 按 Kconfig 条件初始化)
- [ ] CX-2 创建 system_monitor.h/cpp (FreeRTOS task stats + heap watermark + module status)
- [ ] CX-3 在 chat_engine 中拦截本地系统命令 (#status / #photo / #voice / #help)
- [ ] CX-4 WiFi RSSI 监控 + 状态栏信号强度显示
- [ ] CX-5 实现 #status 命令 (heap/RSSI/uptime/module states 格式化输出)
- [ ] CX-6 实现 #photo 命令桩 (触发 CameraDriver → 暂存 RAM → 准备存 TF 卡)
- [ ] CX-7 实现 #voice 命令桩 (触发 AudioIO 录音 → 准备送 ASR)
- [ ] CX-8 本地 Python 验证 + 输出 v0.0.9 完成报告

### 🟢 v0.1.0-0.1.4 — 板子启动 (等板子到货)
- [ ] v0.1.0: 开箱 + 拍照 + 组件确认
- [ ] v0.1.1: 说明书 → GPIO 表填入 `docs/hardware_spec.md`
- [ ] v0.1.2: Kconfig 引脚默认值更新 + menuconfig
- [ ] v0.1.3: 首次编译烧录 + 串口日志验证
- [ ] v0.1.4: 显示屏背光 + SPI + LVGL 渲染

### 🟢 v0.1.5-0.1.8 — 对话打通 (等板子到货)
- [ ] v0.1.5: 触摸驱动确认 + 原始坐标
- [ ] v0.1.6: 触摸校准 + 文本输入
- [ ] v0.1.7: WiFi 连接 + IP 获取
- [ ] v0.1.8: 🎯 首次 AI 对话 ("你好" → 流式回复)

### 🟢 v0.1.9 — UI 打磨 + 稳定性
- [ ] CX-1 退格/空格/回车 输入改进
- [ ] CX-2 WiFi 断连 UI → 重连中提示
- [ ] CX-3 API 超时有明确提示
- [ ] CX-4 对话气泡滚动
- [ ] CX-5 30min 稳定性测试 + 内存监控

### 🟡 v0.2.0-0.2.2 — 摄像头驱动
- [ ] v0.2.0: OV5640 I2C → CHIP_ID 验证
- [ ] v0.2.1: DVP + DMA → 原始帧采集
- [ ] v0.2.2: 首张 JPEG 照片

### 🟡 v0.2.3-0.2.4 — TF 卡 + 集成
- [ ] v0.2.3: TF 卡挂载 + 文件读写
- [ ] v0.2.4: 🎯 #photo 命令 → 拍照 → 存卡 → 预览

---

## 🔴 CX-9: v0.1.4 Display Debug — ST7789 not rendering (P0)

### Assigned: Codex (GPT-5.5) | Priority: P0 | Status: 🔴 BLOCKED

### Context

Board is ATK-DNESP32S3 (正点原子 ESP32-S3 "小智AI" 套件). Firmware builds and flashes OK (ESP-IDF v5.2.7). 

**Symptom**: XL9555 IO expander backlight turns ON (screen glows), but no LVGL content is visible. Screen stays uniformly black/glowing. No app-level serial logs appear on COM3 USB-Serial-JTAG (bootloader logs DO appear).

**What works**: Bootloader, PSRAM (8MB Octal @ 80MHz), Flash (16MB), XL9555 I2C (backlight control).

### Root Cause Investigation Scope

You need to determine WHY the ST7789 SPI display is not rendering. Investigate these hypotheses:

1. **SPI bus init fails silently** → Check if `spi_bus_initialize(SPI2_HOST, ...)` returns ESP_OK. Add ESP_ERROR_CHECK or explicit error logging.

2. **ST7789 init sequence incomplete** → The current init is minimal: SW reset → sleep out → MADCTL(0x60) → pixel format(0x55) → display on. Some panels need color inversion (0x21), VCOM, gamma, or porch settings. Compare against `esp_lcd_panel_st7789` in ESP-IDF which does a complete init.

3. **MADCTL wrong for this panel** → Current MADCTL=0x60 (MV|MX, no BGR). Reference xiaozhi-esp32 uses SWAP_XY=true, MIRROR_X=true, MIRROR_Y=false. Try 0x68 (MV|MX|BGR) and 0x60.

4. **PSRAM buffer allocation fails** → LVGL draw buffers allocated via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`. Add error check — if PSRAM alloc fails, try internal RAM fallback.

5. **Serial output blocked** → Add `esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM)` at start of app_main, or use `ets_printf()` for early debug output.

6. **SPI pin conflict** → GPIO 13 was used for MISO (now set to NC). Check if any other pin conflicts exist.

### Files to Investigate

| File | What to Check |
|------|---------------|
| `main/display_driver.cpp:InitSpiBus()` | SPI bus init return value |
| `main/display_driver.cpp:InitLcdController()` | ST7789 init command sequence |
| `main/display_driver.cpp:Initialize()` | LVGL buffer alloc, display driver registration |
| `main/main.cpp:app_main()` | Init order: NVS → Display → UI → WiFi |
| `main/Kconfig.projbuild` | SPI pin defaults (line 131-187) |
| `sdkconfig.defaults` | MISO=-1, SPI clock |

### Reference

- Working ST7789 init for this board: [78/xiaozhi-esp32 atk-dnesp32s3 config](https://github.com/78/xiaozhi-esp32/blob/main/main/boards/atk-dnesp32s3/config.h)
- ESP-IDF ST7789 panel driver: `components/esp_lcd/src/esp_lcd_panel_st7789.c`
- Hardware spec with confirmed GPIOs: `docs/hardware_spec.md`

### Acceptance Criteria

- [ ] SPI bus init returns ESP_OK (confirm via serial log)
- [ ] ST7789 init each step returns true (add per-step logging)
- [ ] LVGL draw buffers allocated successfully
- [ ] At minimum: solid color fill visible on screen (prove SPI works)
- [ ] App-level ESP_LOGI messages visible on COM3 serial monitor

### Build & Flash

```bash
# From project root, double-click:
build_flash.cmd

# Or manually:
D:\Espressif\idf_cmd_init.bat
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

### Key Config

```
CONFIG_ROBOMIND_DISPLAY_ST7789=y
CONFIG_ROBOMIND_DISPLAY_WIDTH=320
CONFIG_ROBOMIND_DISPLAY_HEIGHT=240
CONFIG_ROBOMIND_DISPLAY_PIN_MOSI=11
CONFIG_ROBOMIND_DISPLAY_PIN_CLK=12
CONFIG_ROBOMIND_DISPLAY_PIN_MISO=-1
CONFIG_ROBOMIND_DISPLAY_PIN_CS=21
CONFIG_ROBOMIND_DISPLAY_PIN_DC=40
CONFIG_ROBOMIND_DISPLAY_PIN_RST=-1
CONFIG_ROBOMIND_DISPLAY_PIN_BL=-1
CONFIG_ROBOMIND_DISPLAY_SPI_HOST=2
```
