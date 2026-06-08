# Kconfig GPIO Default vs hardware_spec.md 一致性审计

> **审计日期**: 2026-06-08
> **审计人**: CodeWhale (CW-PRE-02)
> **规则**: 只报告差异，**不修改任何值**。板子到货后由说明书确认。
> **对比源**: `main/Kconfig.projbuild` (default 值) vs `docs/hardware_spec.md` (推测值)

---

## 摘要

- **已对比项**: 27 个 GPIO/config default 值
- **一致**: 7 个
- **不一致**: 3 个（Display DC/RST/BL 引脚）
- **hw_spec 标记为 `?` (推测值未知)**: 14 个
- **hw_spec 未列出对应项**: 3 个

---

## 逐项对比

### Display SPI 引脚

| Config 项 | Kconfig default | hw_spec 推测值 | 状态 |
|-----------|----------------|----------------|------|
| `ROBOMIND_DISPLAY_PIN_MOSI` | 11 | 11 | ✅ 一致 |
| `ROBOMIND_DISPLAY_PIN_MISO` | 13 | 未单独列出 (与LCD共用) | ⬜ N/A |
| `ROBOMIND_DISPLAY_PIN_CLK` | 12 | 12 | ✅ 一致 |
| `ROBOMIND_DISPLAY_PIN_CS` | 10 | 10 | ✅ 一致 |
| `ROBOMIND_DISPLAY_PIN_DC` | **14** | **9** | ❌ 不一致 |
| `ROBOMIND_DISPLAY_PIN_RST` | **9** | **8** | ❌ 不一致 |
| `ROBOMIND_DISPLAY_PIN_BL` | **8** | **7** | ❌ 不一致 |
| `ROBOMIND_DISPLAY_SPI_HOST` | 2 | SPI3_HOST (2) | ✅ 一致 |
| `ROBOMIND_DISPLAY_WIDTH` | 240 | 240 | ✅ 一致 |
| `ROBOMIND_DISPLAY_HEIGHT` | 320 | 320 | ✅ 一致 |
| `ROBOMIND_DISPLAY_ROTATION` | 0 | N/A | ⬜ N/A |
| `ROBOMIND_DISPLAY_BACKLIGHT` | y | N/A | ⬜ N/A |

### Touch 引脚

| Config 项 | Kconfig default | hw_spec 推测值 | 状态 |
|-----------|----------------|----------------|------|
| `ROBOMIND_TOUCH_PIN_CS` | 15 | 15 | ✅ 一致 |
| `ROBOMIND_TOUCH_PIN_IRQ` | 13 | 13 | ✅ 一致 |
| `ROBOMIND_TOUCH_SPI_HOST` | 2 | (与LCD共用SPI) | ⬜ N/A |
| `ROBOMIND_TOUCH_I2C_SDA` | 17 | ? | ⚠️ 待确认 |
| `ROBOMIND_TOUCH_I2C_SCL` | 18 | ? | ⚠️ 待确认 |
| `ROBOMIND_TOUCH_I2C_ADDR` | 0x38 | ? | ⚠️ 待确认 |

### Camera OV5640 引脚

| Config 项 | Kconfig default | hw_spec 推测值 | 状态 |
|-----------|----------------|----------------|------|
| `ROBOMIND_CAMERA_PIN_SCL` | 17 | ? | ⚠️ 待确认 |
| `ROBOMIND_CAMERA_PIN_SDA` | 18 | ? | ⚠️ 待确认 |

### SD Card (TF Card) 引脚

| Config 项 | Kconfig default | hw_spec 推测值 | 状态 |
|-----------|----------------|----------------|------|
| `ROBOMIND_SD_MODE` | 0 (SPI) | N/A | ⚠️ 待确认 |
| `ROBOMIND_SD_PIN_CS` | 21 | ? | ⚠️ 待确认 |
| `ROBOMIND_SD_PIN_MOSI` | 11 | ? | ⚠️ 待确认 |
| `ROBOMIND_SD_PIN_MISO` | 13 | ? | ⚠️ 待确认 |
| `ROBOMIND_SD_PIN_CLK` | 12 | ? | ⚠️ 待确认 |

### Audio I2S 引脚

| Config 项 | Kconfig default | hw_spec 推测值 | 状态 |
|-----------|----------------|----------------|------|
| `ROBOMIND_AUDIO_I2S_BCK` | 4 | ? | ⚠️ 待确认 |
| `ROBOMIND_AUDIO_I2S_WS` | 5 | ? | ⚠️ 待确认 |
| `ROBOMIND_AUDIO_I2S_DIN` | 6 | ? | ⚠️ 待确认 |
| `ROBOMIND_AUDIO_I2S_DOUT` | 7 | ? | ⚠️ 待确认 |
| `ROBOMIND_AUDIO_SAMPLE_RATE` | 16000 | N/A | ⚠️ 待确认 |
| `ROBOMIND_AUDIO_BUFFER_MS` | 100 | N/A | ⚠️ 待确认 |

---

## 不一致项详情

### 🔴 DC (Data/Command) 引脚
| 来源 | GPIO |
|------|------|
| Kconfig default | **14** |
| hardware_spec.md | **9** |
| 影响 | MADCTL/像素数据写入时 DC=1，命令时 DC=0。错误引脚会导致整个 LCD 无法正常显示 |
| 处理 | 板子到货后查说明书/丝印确认，以实际硬件为准 |

### 🔴 RST (Reset) 引脚
| 来源 | GPIO |
|------|------|
| Kconfig default | **9** |
| hardware_spec.md | **8** |
| 影响 | LCD 上电复位时序。如果接 EN 可设为 -1 |
| 处理 | 确认是否硬件连接到 EN。如独立引脚，查丝印 |

### 🔴 BL (Backlight) 引脚
| 来源 | GPIO |
|------|------|
| Kconfig default | **8** |
| hardware_spec.md | **7** |
| 影响 | PWM 背光亮度控制 |
| 处理 | 查是否有背光控制引脚。如无独立 BL，设 -1 |

---

## ⚠️ GPIO 冲突风险预检

> 以下为 Kconfig default 值下的冲突快速检查，板子到货后需用实际引脚重跑。

| 检查项 | 状态 | 备注 |
|--------|------|------|
| LCD 输出引脚在安全范围 (排除 35-48) | ✅ | DC=14, RST=9, BL=8, CS=10 均为安全输出 |
| GPIO 19/20 未占用 (USB-Serial-JTAG) | ✅ | Kconfig default 未使用 19/20 |
| GPIO 26-37 未占用 (Flash/PSRAM) | ✅ | 未使用 |
| 摄像头 SD_CLK vs SD_MOSI | ⚠️ | DC=14, SCL=17, SDA=18 — 待板子确认是否冲突 |
| SD 卡 SPI 与 LCD SPI 总线复用 | ⚠️ | SD 卡 MOSI=11/MISO=13/CLK=12 与 LCD 完全重叠 — 可能是共用 SPI 总线 (不同 CS) |

---

## 建议

1. **板子到货后第一优先级**: 填写 `hardware_spec.md` 中所有 `?` 项为实际 GPIO 值
2. **DC/RST/BL**: 3 个不一致项需说明书/丝印确认后统一修正 `Kconfig.projbuild` default 值
3. **SD 卡 SPI 复用**: 确认是否与 LCD 共用 SPI 总线。如是，需在 `display_driver` 和 `sd_card` 间协调 `spi_bus_initialize` 调用
4. **OV5640 SCCB**: 确认 SCL/SDA 是否与 FT6x06 触摸 I2C 冲突 (Kconfig default 均为 SCL=17/18 vs FT6x06_SDA=17/SCL=18)
