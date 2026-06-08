# Codex 任务: v0.2.x 骨架预埋 — camera_driver + sd_card

> 版本: 预埋 v0.2.0-0.2.3 所需文件骨架
> 护栏: GUARDRAILS.md — 只创建文件骨架，不实现硬件逻辑
> 指南: CODEX.md — 遵循单例模式、Kconfig 规范

---

## 背景

v0.0.1-0.0.5 全部完成，板子到货前还有时间。
你现在提前创建 v0.2.0 所需的 camera_driver 和 sd_card 模块骨架。
这些是**纯 C++ 样板代码**，遵循项目已有的单例模式。
不需要任何硬件，不需要实现实际的硬件初始化逻辑。

---

## 任务 1: 创建 `main/camera_driver.h`

新建文件，遵循 display_driver.h 的单例模式:

```cpp
/**
 * @file camera_driver.h
 * @brief OV5640 摄像头驱动 — DVP 并口 + SCCB (单例)
 *
 * 职责:
 *   - 通过 SCCB (I2C-like) 配置 OV5640 寄存器
 *   - DVP 8-bit 并口 + DMA 帧采集
 *   - JPEG 拍照 (OV5640 内部 JPEG 编码)
 *   - 连续帧回调 (预览流)
 *
 * 硬件: OV5640 (5MP, DVP 并口, SCCB 地址 0x3C)
 * 依赖: ESP32-S3 I2C + I2S(模拟DVP) + DMA
 * 目标版本: v0.2.0-0.2.2
 */

#ifndef ROBOMIND_CAMERA_DRIVER_H
#define ROBOMIND_CAMERA_DRIVER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class CameraDriver {
public:
    static CameraDriver* GetInstance();

    /// 初始化 OV5640: SCCB(I2C) + DVP 并口 + DMA
    /// @return false if any hardware init step fails
    bool Initialize();

    /// 拍一张 JPEG 照片，存入 out_buffer
    /// @return false if capture fails
    bool CaptureJpeg(std::vector<uint8_t>* out_buffer);

    /// JPEG 输出分辨率
    enum class Resolution {
        kQVGA_320x240,
        kVGA_640x480,
        kHD_1280x720,
    };
    bool SetResolution(Resolution res);

    /// 连续帧回调 (用于预览流，JPEG 数据)
    using FrameCallback = std::function<void(const uint8_t* data, size_t len)>;
    void SetFrameCallback(FrameCallback cb);

    /// 获取最后一次错误信息
    const char* GetLastError() const { return last_error_.c_str(); }

private:
    CameraDriver() = default;
    ~CameraDriver();
    CameraDriver(const CameraDriver&) = delete;
    CameraDriver& operator=(const CameraDriver&) = delete;

    bool InitSccb();            // SCCB (I2C) 初始化 + OV5640 寄存器配置
    bool SendInitSequence();    // 发送 OV5640 JPEG 模式寄存器序列
    bool InitDvp();             // DVP 8-bit 并口 + DMA 配置

    std::string last_error_;
    Resolution resolution_{Resolution::kQVGA_320x240};
    FrameCallback frame_callback_;
};

#endif  // ROBOMIND_CAMERA_DRIVER_H
```

## 任务 2: 创建 `main/camera_driver.cpp`

新建文件，空壳实现:

```cpp
/**
 * @file camera_driver.cpp
 * @brief OV5640 摄像头驱动实现 (骨架)
 *
 * 当前状态: 骨架 — 接口已定义，硬件逻辑待板子到货后填充
 * 目标版本: v0.2.0-0.2.2
 */

#include "camera_driver.h"

#include "esp_log.h"

static const char* TAG = "camera";

CameraDriver* CameraDriver::GetInstance() {
    static CameraDriver instance;
    return &instance;
}

CameraDriver::~CameraDriver() {
    // TODO v0.2.0: Deinit I2C + DVP + DMA
}

bool CameraDriver::Initialize() {
    ESP_LOGI(TAG, "CameraDriver initializing (skeleton — HW logic pending v0.2.0)");

    // TODO v0.2.0: 实现
    // 1. InitSccb() — 配置 I2C (SDA/SCL 引脚来自 Kconfig)，OV5640 地址 0x3C
    // 2. SendInitSequence() — 发送 OV5640 JPEG 模式 (320x240) 寄存器表
    // 3. InitDvp() — 配置 I2S 外设模拟 8-bit 并口 + DMA 描述符链

    ESP_LOGW(TAG, "CameraDriver hardware init not yet implemented — requires board");
    return false;  // 板子到货前返回 false
}

bool CameraDriver::CaptureJpeg(std::vector<uint8_t>* out_buffer) {
    if (!out_buffer) {
        last_error_ = "null output buffer";
        return false;
    }

    // TODO v0.2.2: 触发单帧拍照 → DMA 完成 → 搜索 JPEG SOI/EOI → 拷贝到 out_buffer
    ESP_LOGW(TAG, "CaptureJpeg not yet implemented");
    return false;
}

bool CameraDriver::SetResolution(Resolution res) {
    resolution_ = res;
    // TODO v0.2.1: 通过 SCCB 修改 OV5640 输出分辨率寄存器
    return true;
}

void CameraDriver::SetFrameCallback(FrameCallback cb) {
    frame_callback_ = std::move(cb);
}

bool CameraDriver::InitSccb() {
    // TODO v0.2.0: 实现 I2C 初始化，SCL/SDA 引脚从 Kconfig 取
    return false;
}

bool CameraDriver::SendInitSequence() {
    // TODO v0.2.0: 发送 OV5640 寄存器初始化序列 (参考 OV5640 应用笔记)
    return false;
}

bool CameraDriver::InitDvp() {
    // TODO v0.2.1: 配置 I2S 为 8-bit 并口输入 + DMA
    return false;
}
```

## 任务 3: 创建 `main/sd_card.h`

```cpp
/**
 * @file sd_card.h
 * @brief MicroSD/TF 卡驱动 — FAT 文件系统 (单例)
 *
 * 职责:
 *   - SD 卡初始化 (SPI 或 SDMMC)
 *   - FAT 文件系统挂载
 *   - 文件读写 + 目录列表 + 空间查询
 *
 * 目标版本: v0.2.3
 */

#ifndef ROBOMIND_SD_CARD_H
#define ROBOMIND_SD_CARD_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class SdCard {
public:
    static SdCard* GetInstance();

    /// 初始化 SD 卡并挂载 FAT 文件系统
    /// @return false if mount fails (no card, format error, etc.)
    bool Mount();

    /// 是否已挂载
    bool IsMounted() const { return mounted_; }

    /// 卸载文件系统
    bool Unmount();

    /// 写文件 (覆盖模式)
    bool SaveFile(const char* path, const uint8_t* data, size_t len);

    /// 读文件
    bool ReadFile(const char* path, std::vector<uint8_t>* out);

    /// 检查文件是否存在
    bool FileExists(const char* path);

    /// 获取文件大小 (bytes)
    size_t GetFileSize(const char* path);

    /// 列出目录内容
    std::vector<std::string> ListDir(const char* path);

    /// 磁盘空间 (bytes)
    uint64_t GetFreeSpace();
    uint64_t GetTotalSpace();

    /// 获取挂载点路径
    const char* GetMountPoint() const { return mount_point_.c_str(); }

    /// 获取最后一次错误信息
    const char* GetLastError() const { return last_error_.c_str(); }

private:
    SdCard() = default;
    ~SdCard();
    SdCard(const SdCard&) = delete;
    SdCard& operator=(const SdCard&) = delete;

    bool mounted_{false};
    std::string mount_point_{"/sdcard"};
    std::string last_error_;
};

#endif  // ROBOMIND_SD_CARD_H
```

## 任务 4: 创建 `main/sd_card.cpp`

```cpp
/**
 * @file sd_card.cpp
 * @brief MicroSD/TF 卡驱动实现 (骨架)
 *
 * 当前状态: 骨架 — 接口已定义，硬件逻辑待板子到货后填充
 * 目标版本: v0.2.3
 */

#include "sd_card.h"

#include <cstring>

#include "esp_log.h"

static const char* TAG = "sd_card";

SdCard* SdCard::GetInstance() {
    static SdCard instance;
    return &instance;
}

SdCard::~SdCard() {
    if (mounted_) {
        Unmount();
    }
}

bool SdCard::Mount() {
    ESP_LOGI(TAG, "SD card mounting (skeleton — HW logic pending v0.2.3)");

    // TODO v0.2.3: 根据硬件选择 SPI 或 SDMMC 模式
    // - 判断接口模式 (检查 Kconfig 或板子引脚)
    // - SPI: spi_bus_initialize + sdspi_dev_handle
    // - SDMMC: sdmmc_host_init + sdmmc_slot_config
    // - 挂载 FAT: esp_vfs_fat_sdspi_mount 或 esp_vfs_fat_sdmmc_mount

    ESP_LOGW(TAG, "SD card hardware init not yet implemented — requires board");
    return false;
}

bool SdCard::Unmount() {
    if (!mounted_) return true;

    // TODO v0.2.3: esp_vfs_fat_sdmmc_unmount 或等效
    mounted_ = false;
    return true;
}

bool SdCard::SaveFile(const char* path, const uint8_t* data, size_t len) {
    if (!mounted_) {
        last_error_ = "SD card not mounted";
        return false;
    }
    if (!path || !data || len == 0) {
        last_error_ = "invalid arguments";
        return false;
    }

    // TODO v0.2.3: fopen(path, "wb") → fwrite → fclose
    ESP_LOGW(TAG, "SaveFile not yet implemented");
    return false;
}

bool SdCard::ReadFile(const char* path, std::vector<uint8_t>* out) {
    if (!mounted_) {
        last_error_ = "SD card not mounted";
        return false;
    }
    if (!path || !out) {
        last_error_ = "invalid arguments";
        return false;
    }

    // TODO v0.2.3: fopen(path, "rb") → fread → fclose
    ESP_LOGW(TAG, "ReadFile not yet implemented");
    return false;
}

bool SdCard::FileExists(const char* path) {
    if (!mounted_ || !path) return false;
    // TODO v0.2.3: stat(path) or fopen check
    return false;
}

size_t SdCard::GetFileSize(const char* path) {
    if (!mounted_ || !path) return 0;
    // TODO v0.2.3: stat → st_size
    return 0;
}

std::vector<std::string> SdCard::ListDir(const char* path) {
    std::vector<std::string> result;
    if (!mounted_ || !path) return result;
    // TODO v0.2.3: opendir → readdir → closedir
    return result;
}

uint64_t SdCard::GetFreeSpace() {
    if (!mounted_) return 0;
    // TODO v0.2.3: statvfs → f_frsize * f_bavail
    return 0;
}

uint64_t SdCard::GetTotalSpace() {
    if (!mounted_) return 0;
    // TODO v0.2.3: statvfs → f_frsize * f_blocks
    return 0;
}
```

## 任务 5: 添加 Camera Kconfig 配置项

在 `main/Kconfig.projbuild` 的 `menu "Display"` 之后 (或 audio 之后)，新增:

```kconfig
    menu "Camera (OV5640)"
        config ROBOMIND_ENABLE_CAMERA
            bool "Enable OV5640 Camera"
            default n
            help
                Enable OV5640 camera module via DVP 8-bit parallel interface.
                Requires OV5640 camera module connected to DVP pins.

        config ROBOMIND_CAMERA_PIN_XCLK
            int "Camera XCLK Pin (Master Clock)"
            default 21
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 master clock input (20MHz generated by ESP32-S3 LEDC).

        config ROBOMIND_CAMERA_PIN_PCLK
            int "Camera PCLK Pin (Pixel Clock)"
            default 22
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 pixel clock output → ESP32-S3 input.

        config ROBOMIND_CAMERA_PIN_VSYNC
            int "Camera VSYNC Pin (Frame Sync)"
            default 23
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 vertical sync (frame start).

        config ROBOMIND_CAMERA_PIN_HREF
            int "Camera HREF Pin (Row Sync)"
            default 24
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 horizontal reference (row valid).

        config ROBOMIND_CAMERA_PIN_D0
            int "Camera D0 Pin (Data Bit 0)"
            default 25
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_D1
            int "Camera D1 Pin"
            default 26
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_D2
            int "Camera D2 Pin"
            default 27
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_D3
            int "Camera D3 Pin"
            default 28
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_D4
            int "Camera D4 Pin"
            default 29
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_D5
            int "Camera D5 Pin"
            default 30
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_D6
            int "Camera D6 Pin"
            default 31
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_D7
            int "Camera D7 Pin"
            default 32
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA

        config ROBOMIND_CAMERA_PIN_SCL
            int "Camera SCCB SCL Pin (I2C Clock)"
            default 17
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 SCCB clock (I2C-like, address 0x3C).

        config ROBOMIND_CAMERA_PIN_SDA
            int "Camera SCCB SDA Pin (I2C Data)"
            default 18
            range 0 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 SCCB data (I2C-like).

        config ROBOMIND_CAMERA_PIN_PWDN
            int "Camera PWDN Pin (Power Down)"
            default -1
            range -1 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 power down. Set to -1 if not connected.

        config ROBOMIND_CAMERA_PIN_RESET
            int "Camera RESET Pin"
            default -1
            range -1 48
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 hardware reset. Set to -1 if not connected.

        config ROBOMIND_CAMERA_JPEG_QUALITY
            int "JPEG Quality (OV5640 internal encoder)"
            default 80
            range 10 100
            depends on ROBOMIND_ENABLE_CAMERA
            help
                OV5640 internal JPEG compression quality. Higher = better quality, larger file.
    endmenu
```

> ⚠️ **重要**: 上述 Kconfig 中的 D0-D7 引脚 default 值均在 25-32 范围。
> 但 GPIO 26-32 被 Flash/PSRAM 占用！这是**故意的占位值**——
> 板子到货后必须根据说明书修改为正确的 GPIO。
> 在 D0-D7 的 help 文本中加注释:
> `⚠ DVP data pins must be on a contiguous GPIO block. Defaults are placeholders — update from board manual!`

> 更正: 把 D0-D7 的 default 改为合理的安全占位值 (不在 26-37):
> D0=4, D1=5, D2=6, D3=7, D4=15, D5=16, D6=17, D7=18
> 并标注 ⚠ "DVP data pins must be contiguous GPIO for DMA. Verify against board manual."

## 任务 6: 添加 SD Card Kconfig 配置项

在 Camera menu 之后新增:

```kconfig
    menu "SD Card (TF/MicroSD)"
        config ROBOMIND_ENABLE_SD_CARD
            bool "Enable SD Card"
            default n
            help
                Enable MicroSD/TF card slot for photo storage and log writing.

        choice ROBOMIND_SD_CARD_MODE
            prompt "SD Card Interface Mode"
            default ROBOMIND_SD_CARD_MODE_SPI
            depends on ROBOMIND_ENABLE_SD_CARD
            help
                Select SD card interface mode. SPI is more common on ESP32-S3 dev boards.

            config ROBOMIND_SD_CARD_MODE_SPI
                bool "SPI Mode"
            config ROBOMIND_SD_CARD_MODE_SDMMC
                bool "SDMMC Mode (4-bit)"
        endchoice

        config ROBOMIND_SD_CARD_PIN_CS
            int "SD Card CS Pin"
            default 33
            range 0 48
            depends on ROBOMIND_ENABLE_SD_CARD && ROBOMIND_SD_CARD_MODE_SPI
            help
                SPI chip select for SD card.

        config ROBOMIND_SD_CARD_PIN_MOSI
            int "SD Card MOSI Pin"
            default 34
            range 0 48
            depends on ROBOMIND_ENABLE_SD_CARD && ROBOMIND_SD_CARD_MODE_SPI
            help
                SPI MOSI for SD card.

        config ROBOMIND_SD_CARD_PIN_MISO
            int "SD Card MISO Pin"
            default 35
            range 0 48
            depends on ROBOMIND_ENABLE_SD_CARD && ROBOMIND_SD_CARD_MODE_SPI
            help
                SPI MISO for SD card.

        config ROBOMIND_SD_CARD_PIN_CLK
            int "SD Card CLK Pin"
            default 36
            range 0 48
            depends on ROBOMIND_ENABLE_SD_CARD && ROBOMIND_SD_CARD_MODE_SPI
            help
                SPI clock for SD card.

        config ROBOMIND_SD_CARD_PIN_CD
            int "SD Card Detect Pin"
            default -1
            range -1 48
            depends on ROBOMIND_ENABLE_SD_CARD
            help
                Card detect switch (active low). Set to -1 if not connected.

        config ROBOMIND_SD_CARD_MOUNT_POINT
            string "SD Card Mount Point"
            default "/sdcard"
            depends on ROBOMIND_ENABLE_SD_CARD
            help
                FAT filesystem mount point path.
    endmenu
```

> ⚠️ SD 卡 SPI 默认引脚 (33-36) 与 GPIO 限制冲突 (33-37 被 PSRAM 占用)。
> 在 help 文本中加 ⚠ 标注，板子到货后修改。

## 任务 7: 注册到 CMakeLists.txt

编辑 `main/CMakeLists.txt`，在 SRCS 中添加:

```cmake
# 在现有 SRCS 列表末尾添加 (camera 和 sd_card 暂不参与编译，注释掉):
# "camera_driver.cpp"   # TODO v0.2.0: uncomment when hardware available
# "sd_card.cpp"          # TODO v0.2.3: uncomment when hardware available
```

> 注意: 先**注释掉**不参与编译。等板子到货后 v0.2.0 再取消注释。
> 原因: Kconfig `ROBOMIND_ENABLE_CAMERA` default n，未启用时编译会浪费 Flash。

---

## 约束

✅ 创建: main/camera_driver.h, main/camera_driver.cpp
✅ 创建: main/sd_card.h, main/sd_card.cpp
✅ 修改: main/Kconfig.projbuild (追加 Camera + SD Card menu)
✅ 修改: main/CMakeLists.txt (注释形式注册)

❌ 不实现: 任何硬件初始化逻辑 (I2C/DVP/DMA/FAT mount)
❌ 不修改: 已有 .cpp/.h 源码
❌ 不修改: main.cpp 启动链 (v0.2.0 之后再串联)
❌ 不修改: sdkconfig.defaults (camera 和 sd 暂不启用)

---

## 输出

完成后输出:

## v0.2.x 骨架预埋报告

### 新建文件
| 文件 | 行数 | 说明 |
|------|------|------|

### Kconfig 新增
- Camera: N 个 config 项
- SD Card: M 个 config 项

### CMakeLists 修改
- 注释注册 camera_driver + sd_card

### GPIO 占位警告
- [ ] D0-D7 默认引脚标注了 ⚠ 占位符警告
- [ ] SD SPI 默认引脚标注了 ⚠ 冲突警告
- [ ] 所有 help 文本说明了板子到货后需要修改

### 护栏检查
- [ ] 仅创建新文件 + 修改 Kconfig/CMakeLists
- [ ] 未修改已有源码
- [ ] 未实现硬件逻辑
- [ ] 遵循单例模式 + ESP-IDF 日志规范
