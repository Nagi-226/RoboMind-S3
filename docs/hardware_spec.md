# 硬件规格文档

> ✅ **确认状态**: 板子已到货，GPIO 引脚已从原理图/xiaozhi-esp32 参考代码提取。
> 标记 `⚠️ 待实测` 的字段需在烧录后验证。

## 目标开发板

**正点原子 (ALIENTEK) ATK-DNESP32S3 开发板 — "小智AI聊天机器人"组合套件**

### 套件清单

| 序号 | 组件 | 型号/规格 | 状态 |
|------|------|-----------|------|
| 1 | 主板 | 正点原子 ATK-DNESP32S3 开发板 | ✅ 已到货 |
| 2 | 显示屏 | 2.4" TFT LCD (ST7789, SPI, 320×240) | ✅ 已确认 |
| 3 | 摄像头 | OV5640 (5MP, 自动对焦, DVP 并口) | ✅ 已确认 |
| 4 | TF 卡 | MicroSD 卡槽 (板上集成) | ✅ 已确认 |
| 5 | 麦克风 | MEMS 麦克风 (经 ES8388 音频 Codec) | ⚠️ 待实测 |
| 6 | 扬声器 | I2S 功放 (经 ES8388 音频 Codec) | ⚠️ 待实测 |

---

## ESP32-S3 模组规格

| 参数 | 值 |
|------|-----|
| 型号 | ESP32-S3-WROOM-1-N16R8 |
| CPU | Xtensa LX7 双核 32-bit, 最高 240MHz |
| Flash | 16MB (Quad SPI) |
| PSRAM | 8MB (Octal SPI) |
| WiFi | 2.4GHz 802.11 b/g/n |
| Bluetooth | BLE 5.0 |
| USB | USB-C (USB-Serial-JTAG) |
| GPIO | 约 36 可用 IO (含仅输入引脚) |

---

## IO 扩展器

| 参数 | 值 |
|------|-----|
| 芯片型号 | **XL9555** (NXP PCA9555 兼容) |
| I2C 地址 | **0x20** |
| I2C 总线 | I2C_NUM_0, SDA=GPIO_41, SCL=GPIO_42 |
| 用途 | LCD 背光控制、LCD 复位、摄像头复位/PWDN、LED |

---

## 显示屏

| 参数 | 值 |
|------|-----|
| 尺寸 | 2.4 inch |
| 分辨率 | **320 × 240** (横屏) |
| 驱动 IC | **ST7789** |
| 接口 | 4-wire SPI (SPI2_HOST) |
| 颜色深度 | 16-bit (RGB565) |
| 背光 | ⚠️ 通过 XL9555 IO 扩展控制 (非直连 GPIO) |
| 旋转 | SWAP_XY=true, MIRROR_X=true |

---

## GPIO 引脚定义表

> ✅ 以下数据来源于正点原子原理图 + xiaozhi-esp32 开源固件的 `atk-dnesp32s3` 板级配置。

### LCD SPI 引脚

| 信号 | 方向 | GPIO | 备注 |
|------|------|------|------|
| LCD_MOSI | ESP→LCD | **11** | SPI MOSI |
| LCD_SCLK | ESP→LCD | **12** | SPI Clock |
| LCD_MISO | LCD→ESP | **13** | SPI MISO (读显示寄存器) |
| LCD_CS | ESP→LCD | **21** | SPI Chip Select |
| LCD_DC | ESP→LCD | **40** | Data/Command |
| LCD_RST | ESP→XL9555 | **XL9555 IO1_2** | ⚠️ 不直连 GPIO，通过 IO 扩展器 |
| LCD_BL | ESP→XL9555 | **XL9555 IO1_3** | ⚠️ PWM 背光通过 IO 扩展器 |
| SPI_HOST | — | **SPI2_HOST** | |

### 触摸屏引脚

| 信号 | 方向 | GPIO | 备注 |
|------|------|------|------|
| 触摸芯片型号 | — | ⚠️ **待确认** (XPT2046 或 FT6x06) | |
| 触摸接口 | — | ⚠️ 待确认 (SPI 共享 或 独立 I2C) | |

### 摄像头 OV5640 引脚 (DVP 并口)

| 信号 | 方向 | GPIO | 备注 |
|------|------|------|------|
| CAM_XCLK | — | **NC** | ✅ 板载 24MHz 有源晶振，无需 ESP 输出 |
| CAM_PCLK | CAM→ESP | **45** | 像素时钟 |
| CAM_VSYNC | CAM→ESP | **47** | 帧同步 |
| CAM_HREF | CAM→ESP | **48** | 行同步 |
| CAM_D0 | CAM→ESP | **4** | DVP 数据位 0 |
| CAM_D1 | CAM→ESP | **5** | DVP 数据位 1 |
| CAM_D2 | CAM→ESP | **6** | DVP 数据位 2 |
| CAM_D3 | CAM→ESP | **7** | DVP 数据位 3 |
| CAM_D4 | CAM→ESP | **15** | DVP 数据位 4 |
| CAM_D5 | CAM→ESP | **16** | DVP 数据位 5 |
| CAM_D6 | CAM→ESP | **17** | DVP 数据位 6 |
| CAM_D7 | CAM→ESP | **18** | DVP 数据位 7 |
| CAM_SCL | ESP→CAM | **38** | SCCB (I2C) 时钟 |
| CAM_SDA | ESP↔CAM | **39** | SCCB (I2C) 数据 |
| CAM_PWDN | XL9555→CAM | **XL9555 IO0_4** | ⚠️ 通过 IO 扩展器 |
| CAM_RESET | XL9555→CAM | **XL9555 IO0_5** | ⚠️ 通过 IO 扩展器 |

> ⚠️ **OV5640 注意事项**:
> - XCLK 由板载 24MHz 晶振提供，无需 ESP32-S3 LEDC 产生
> - D0-D7 连续排列在 GPIO 4-7, 15-18，适配 DMA 并口读取
> - SCCB (I2C) 使用 GPIO 38/39，OV5640 地址 0x3C
> - PWDN/RESET 通过 XL9555 IO 扩展器控制，非直连 GPIO

### TF 卡引脚

| 信号 | 方向 | GPIO | 备注 |
|------|------|------|------|
| SD 接口模式 | — | ⚠️ **待确认** (SPI 或 SDMMC) | |
| SD_CS (SPI) | ESP→TF | ⚠️ 待确认 | |
| SD_MOSI (SPI) | ESP→TF | ⚠️ 待确认 | |
| SD_MISO (SPI) | TF→ESP | ⚠️ 待确认 | |
| SD_CLK (SPI) | ESP→TF | ⚠️ 待确认 | |
| SD_CD (Card Detect) | TF→ESP | ⚠️ 待确认 | |

### I2S 音频引脚

| 信号 | 方向 | GPIO | 备注 |
|------|------|------|------|
| 音频 Codec | — | **ES8388** | I2C 地址 0x10 |
| I2C SDA | ESP↔Codec | **41** | 共享 XL9555 的 I2C 总线 |
| I2C SCL | ESP→Codec | **42** | 共享 XL9555 的 I2C 总线 |
| I2S_MCLK | ESP→Codec | **3** | 主时钟 |
| I2S_WS | ESP→Codec | **9** | 字选 (LRCLK) |
| I2S_BCK | ESP→Codec | **46** | 位时钟 |
| I2S_DIN | Codec→ESP | **14** | 麦克风音频数据输入 |
| I2S_DOUT | ESP→Codec | **10** | 扬声器音频数据输出 |

> ⚠️ **重要**: 音频不是简单的 I2S MEMS 麦克风直连，而是通过 **ES8388 Codec** 芯片。
> ES8388 内部集成 ADC (麦克风) + DAC (扬声器功放)，通过 I2C 配置寄存器，通过 I2S 传输音频数据。

### 其他 GPIO

| 功能 | GPIO | 备注 |
|------|------|------|
| BOOT 按钮 | **0** | 低电平有效，启动时拉低进入下载模式 |
| 用户 LED | **1** | 低电平亮 |
| XL9555 I2C SDA | **41** | 与 ES8388 共享 I2C 总线 |
| XL9555 I2C SCL | **42** | 与 ES8388 共享 I2C 总线 |

---

## I2C 总线拓扑

```
ESP32-S3 I2C_NUM_0 (GPIO 41=SDA, GPIO 42=SCL)
├── XL9555 IO 扩展器 @ 0x20
│   ├── IO0_4: 摄像头 PWDN
│   ├── IO0_5: 摄像头 RESET
│   ├── IO1_2: LCD 复位
│   └── IO1_3: LCD 背光
└── ES8388 音频 Codec @ 0x10
    ├── ADC → MEMS 麦克风
    └── DAC → 扬声器功放
```

---

## 引脚冲突检查

当所有引脚填入后，检查以下冲突规则：

- [x] LCD_CS (21) / LCD_DC (40) 不在 GPIO 35-48 仅输入范围 (DC=40 可用)
- [x] 摄像头 D0-D7 (4-7, 15-18) 连续且不冲突
- [x] I2S BCK (46) / WS (9) 不与 SPI CLK (12) / CS (21) 冲突
- [x] GPIO 19/20 保留给 USB-Serial-JTAG ✅
- [x] GPIO 26-37 被 Flash/PSRAM 占用 ✅
- [x] 所有输出引脚不在 GPIO 35-48 仅输入范围 (除 DC=40 可用)
- [ ] ⚠️ TF 卡引脚待确认后复查
- [ ] ⚠️ 触摸屏引脚待确认后复查

---

## 功耗参考

| 模式 | 电流 (估算) |
|------|------------|
| WiFi 连接 + 屏幕亮 | ~150mA |
| WiFi + 屏幕 + 摄像头拍照 | ~300mA |
| Deep Sleep | ~10µA |

建议使用 5V/1A 以上 USB 供电。

---

## 参考资料

- 正点原子资料下载: http://www.openedv.com/docs/boards/esp32/ATK-DNESP32S3.html
- 小智AI开源固件 (78/xiaozhi-esp32): https://github.com/78/xiaozhi-esp32
- ESP32-S3 数据手册: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- XL9555 数据手册: NXP PCA9555 兼容
- ES8388 数据手册: 顺芯音频 Codec
