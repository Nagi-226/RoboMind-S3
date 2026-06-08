# 硬件规格文档

> ⚠️ **文档状态**: 部分信息待板子到货后补充。标记 `?` 的字段为推测值。

## 目标开发板

**正点原子 (ALIENTEK) ESP32-S3 开发板 — "小智AI聊天机器人"组合套件**

### 套件清单

| 序号 | 组件 | 型号/规格 | 状态 |
|------|------|-----------|------|
| 1 | 主板 | 正点原子 ESP32-S3 开发板 | 📦 在途 |
| 2 | 显示屏 | 2.4" TFT LCD (推测 ILI9341, 240×320) | 📦 在途 |
| 3 | 摄像头 | OV5640 (5MP, 自动对焦) | 📦 在途 |
| 4 | TF 卡 | MicroSD 卡槽 (板上集成) | 📦 在途 |
| 5 | 麦克风 | MEMS 麦克风 (推测 INMP441 或板上集成) | 📦 在途 |
| 6 | 扬声器 | I2S 功放 (推测 MAX98357 或板上集成) | 📦 在途 |

---

## ESP32-S3 模组规格

| 参数 | 值 |
|------|-----|
| 型号 | ESP32-S3-WROOM-1 (推测) |
| CPU | Xtensa LX7 双核 32-bit, 最高 240MHz |
| Flash | 16MB (Quad SPI) |
| PSRAM | 8MB (Octal SPI) |
| WiFi | 2.4GHz 802.11 b/g/n |
| Bluetooth | BLE 5.0 |
| USB | USB-C (USB-Serial-JTAG) |
| GPIO | 约 36 可用 IO (含仅输入引脚) |

---

## 显示屏

| 参数 | 推测值 | 实际值 (待填) |
|------|--------|--------------|
| 尺寸 | 2.4 inch | |
| 分辨率 | 240 × 320 | |
| 驱动 IC | **ILI9341** (或 ST7789) | |
| 接口 | 4-wire SPI | |
| 颜色深度 | 16-bit (RGB565) | |
| 背光 | PWM 可调 | |

---

## GPIO 引脚定义表

> 🎯 **拿到说明书后的第一件事：把下面表格的 `?` 全部替换为实际值！**

### LCD SPI 引脚

| 信号 | 方向 | 推测 GPIO | **实际 GPIO** |
|------|------|-----------|---------------|
| LCD_MOSI | ESP→LCD | 11 | |
| LCD_SCLK | ESP→LCD | 12 | |
| LCD_CS | ESP→LCD | 10 | |
| LCD_DC | ESP→LCD | 9 | |
| LCD_RST | ESP→LCD | 8 | |
| LCD_BL | ESP→LCD | 7 | |
| SPI_HOST | — | SPI3_HOST (2) | |

### 触摸屏引脚

| 信号 | 方向 | 推测 GPIO | **实际 GPIO** |
|------|------|-----------|---------------|
| 触摸芯片型号 | — | XPT2046 (?) | |
| T_CS | ESP→Touch | 15 | |
| T_IRQ | Touch→ESP | 13 | |
| T_MOSI | 与 LCD 共用 | 11 | |
| T_MISO | Touch→ESP | ? | |
| T_CLK | 与 LCD 共用 | 12 | |

### 摄像头 OV5640 引脚 (DVP 并口)

| 信号 | 方向 | 推测 GPIO | **实际 GPIO** |
|------|------|-----------|---------------|
| CAM_XCLK | ESP→CAM | ? | |
| CAM_PCLK | CAM→ESP | ? | |
| CAM_VSYNC | CAM→ESP | ? | |
| CAM_HREF | CAM→ESP | ? | |
| CAM_D0 | CAM→ESP | ? | |
| CAM_D1 | CAM→ESP | ? | |
| CAM_D2 | CAM→ESP | ? | |
| CAM_D3 | CAM→ESP | ? | |
| CAM_D4 | CAM→ESP | ? | |
| CAM_D5 | CAM→ESP | ? | |
| CAM_D6 | CAM→ESP | ? | |
| CAM_D7 | CAM→ESP | ? | |
| CAM_SCL | ESP→CAM | ? | |
| CAM_SDA | ESP↔CAM | ? | |
| CAM_PWDN | ESP→CAM | ? | |
| CAM_RESET | ESP→CAM | ? | |

⚠️ **OV5640 注意事项**:
- XCLK 需 10-24MHz (通常 20MHz)，ESP32-S3 LEDC 产生
- D0-D7 必须连续 GPIO 以配合 DMA 并口读取 (I2S 外设模拟)
- SCCB (I2C) 与普通 I2C 类似，OV5640 地址 0x3C

### TF 卡引脚

| 信号 | 方向 | 推测 GPIO (SPI 模式) | **实际 GPIO** |
|------|------|---------------------|---------------|
| SD_CS | ESP→TF | ? | |
| SD_MOSI | ESP→TF | ? | |
| SD_MISO | TF→ESP | ? | |
| SD_CLK | ESP→TF | ? | |
| SD_CD (Card Detect) | TF→ESP | ? | |

⚠️ 如果板子用 SDMMC 模式, 引脚固定为 CLK/CMD/D0-D3, 查说明书确认。

### I2S 音频引脚

| 信号 | 方向 | 推测 GPIO | **实际 GPIO** |
|------|------|-----------|---------------|
| I2S_BCK | ESP→Codec | ? | |
| I2S_WS | ESP→Codec | ? | |
| I2S_DIN | Mic→ESP | ? | |
| I2S_DOUT | ESP→Amp | ? | |
| I2S_MCLK (可选) | ESP→Codec | ? | |

---

## 引脚冲突检查

当所有引脚填入后，检查以下冲突规则：

- [ ] LCD_RST / LCD_DC / LCD_CS 不能用了 GPIO 35-48 (仅输入)
- [ ] 摄像头 D0-D7 不与其他功能冲突 (DVP 独占)
- [ ] I2S BCK/WS 不与 SPI CLK/CS 冲突
- [ ] GPIO 19/20 保留给 USB-Serial-JTAG
- [ ] GPIO 26-37 被 Flash/PSRAM 占用，不可用
- [ ] 所有输出引脚确认有驱动能力 (GPIO 35-48 不可做输出)

---

## 功耗参考

| 模式 | 电流 (估算) |
|------|------------|
| WiFi 连接 + 屏幕亮 | ~150mA |
| WiFi + 屏幕 + 摄像头拍照 | ~300mA |
| Deep Sleep | ~10µA |

建议使用 5V/1A 以上 USB 供电。
