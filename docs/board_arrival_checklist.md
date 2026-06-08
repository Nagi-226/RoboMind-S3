# 板子到货检查清单

> 🎯 **板子到货后，按此清单逐项执行。预计总耗时: 1-2 小时 (Day 0)**

---

## Phase 0: 开箱与资料收集 (15 min)

- [ ] **0.1** 拆箱，确认所有组件齐全:
  - [ ] 正点原子 ESP32-S3 主板
  - [ ] 2.4" TFT LCD (已组装或排线连接)
  - [ ] OV5640 摄像头模块
  - [ ] TF 卡 (可能需要自备)
  - [ ] USB-C 数据线
  - [ ] 扬声器 / 麦克风模块 (如套件包含)

- [ ] **0.2** 找到说明书/原理图，定位 **GPIO 引脚定义表** (通常在说明书第2-3页或附录)

- [ ] **0.3** 拍照记录:
  - [ ] 板子正面 (带丝印)
  - [ ] 板子背面 (带丝印)
  - [ ] 说明书引脚定义页

- [ ] **0.4** 将照片和说明书 PDF 上传至本项目 `docs/` 目录

---

## Phase 1: 填入 GPIO 引脚 (20 min)

- [ ] **1.1** 打开 `docs/hardware_spec.md`，逐个确认并填入 GPIO 引脚表:
  - [ ] LCD SPI 引脚 (MOSI, SCLK, CS, DC, RST, BL)
  - [ ] 触摸芯片型号 + 引脚
  - [ ] 摄像头 OV5640 引脚 (D0-D7, XCLK, PCLK, VSYNC, HREF, SCL, SDA)
  - [ ] TF 卡引脚 (SPI 或 SDMMC 模式确认)
  - [ ] I2S 音频引脚 (BCK, WS, DIN, DOUT)

- [ ] **1.2** 确认显示屏驱动 IC 型号
  - 方法: 看 LCD 排线上的丝印/说明书
  - 常见: ILI9341 (2.4" 240x320), ST7789 (2.4" 240x320)

- [ ] **1.3** 确认 ESP32-S3 模组具体型号 (看模组金属壳丝印)
  - 常见: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)

- [ ] **1.4** 更新 `main/Kconfig.projbuild` 中的引脚默认值

---

## Phase 2: 首次编译烧录 (20 min)

- [ ] **2.1** 确认 ESP-IDF 环境:
  ```bash
  . ~/esp-idf/export.sh
  idf.py --version  # 应显示 v5.2+
  ```

- [ ] **2.2** 配置项目 (`idf.py menuconfig`):
  - [ ] WiFi SSID + 密码
  - [ ] AI API Key
  - [ ] Display → 选正确驱动芯片 + SPI 引脚
  - [ ] Touch → 选正确驱动 + 引脚
  - [ ] Audio → 暂不启用

- [ ] **2.3** 首次编译:
  ```bash
  idf.py fullclean
  idf.py build
  ```
  确认无编译错误。

- [ ] **2.4** 烧录:
  ```bash
  idf.py -p /dev/ttyUSB0 flash monitor
  ```
  (Windows: COMx; Linux: /dev/ttyUSB0; Mac: /dev/tty.usbserial-*)

- [ ] **2.5** 观察串口输出，确认:
  - [ ] `NVS initialized OK`
  - [ ] `WiFi connected OK`
  - [ ] `Display + LVGL initialized OK`
  - [ ] `RoboMind-S3 started successfully`

---

## Phase 3: 屏幕验证 (10 min)

- [ ] **3.1** 屏幕是否点亮?
  - ✅ **点亮** → 查看启动画面是否正常 ("RoboMind AI" / "AI Chatbot Ready")
  - ❌ **不亮** → 排查:
    1. BL 背光引脚是否配置正确 (PWM 输出)
    2. RST 引脚是否配置 (或接 EN)
    3. 驱动 IC 型号是否选对
    4. SPI 引脚 MOSI/CLK/CS/DC 是否有误

- [ ] **3.2** 屏幕显示是否正常?
  - 颜色正常 / 无色偏 / 无撕裂
  - 如颜色异常 → 检查 MADCTL 寄存器配置 (RGB vs BGR)

---

## Phase 4: 触摸验证 (10 min)

- [ ] **4.1** 触摸是否响应?
  - ✅ **响应** → 点击文本输入区能输入
  - ❌ **不响应** → 排查:
    1. 触摸驱动型号是否选对 (XPT2046 vs FT6x06)
    2. 触摸 CS/IRQ 引脚是否正确
    3. 如 XPT2046: 确认 SPI 共享总线模式, MISO 是否正确

---

## Phase 5: 端到端对话测试 (10 min)

- [ ] **5.1** 连接 AI API:
  - 确认 WiFi 已连接 (串口显示 IP)
  - 在输入区输入 "你好"
  - 点击发送按钮

- [ ] **5.2** 验证 AI 回复:
  - 串口输出 `HTTP status: 200`
  - 屏幕上出现 AI 回复气泡 (流式逐字显示)
  - 状态栏 `✓ Done`

- [ ] **5.3** 如果 API 调用失败:
  - 检查 `ROBOMIND_AI_API_ENDPOINT` URL 是否正确
  - 检查 `ROBOMIND_AI_API_KEY` 是否有效
  - 检查 WiFi 是否能访问外网
  - 查看串口错误日志

---

## Phase 5 完成标准

当以下全部达成，Phase 5 即为通过:

- [x] 板子上电 -> 屏幕亮 -> WiFi 连接 -> 发送消息 -> AI 回复显示在屏幕上

---

## Phase 6+: 后续开发 (按需)

Phase 0-5 完成后，回到 `CLAUDE.md` 中的开发优先级表，继续开发:

- **P1**: `camera_driver.h/cpp` (OV5640 拍照)
- **P1**: `sd_card.h/cpp` (TF 卡存储)
- **P2**: `audio_io.cpp` 充实 (I2S 录音+播放)
- **P3**: 多模态视觉 + 全双工语音

---

## 遇到问题?

1. 先看串口日志 (每个模块有独立 TAG)
2. 检查引脚冲突 (`docs/hardware_spec.md` 底部检查表)
3. 确认 PSRAM 已开启 (`idf.py menuconfig` → Component config → ESP PSRAM)
4. 确认 `sdkconfig.defaults` 中的 TLS 证书包已启用
