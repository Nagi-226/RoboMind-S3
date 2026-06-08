/**
 * @file display_driver.cpp
 * @brief 显示屏驱动实现
 *
 * SPI 引脚映射 (来自 Kconfig，可通过 menuconfig 覆盖):
 *   MOSI, CLK, CS, DC, RST, BL (PWM)
 *
 * LVGL 移植要点:
 *   - lv_tick_inc() 由独立 timer task 每 5ms 调用
 *   - flush callback 直接将像素块通过 SPI 写入显示屏 GRAM
 *   - 使用双缓冲 (draw_buf) 减少撕裂
 */

#include "display_driver.h"

#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char* TAG = "display";

// --- Kconfig 显示配置 ---
#if defined(CONFIG_ROBOMIND_DISPLAY_ILI9341)
#define DISPLAY_DRIVER_ILI9341 1
#elif defined(CONFIG_ROBOMIND_DISPLAY_ST7789)
#define DISPLAY_DRIVER_ST7789 1
#elif defined(CONFIG_ROBOMIND_DISPLAY_ST7796)
#define DISPLAY_DRIVER_ST7796 1
#elif defined(CONFIG_ROBOMIND_DISPLAY_GC9A01)
#define DISPLAY_DRIVER_GC9A01 1
#else
#error "No display driver selected! Enable one in menuconfig: RoboMind S3 -> Display -> Display Driver"
#endif

#ifndef CONFIG_ROBOMIND_DISPLAY_SPI_HOST
#define CONFIG_ROBOMIND_DISPLAY_SPI_HOST 2  // Kconfig default: 2 (SPI3_HOST)
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_PIN_MOSI
#define CONFIG_ROBOMIND_DISPLAY_PIN_MOSI 11  // Kconfig default: 11
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_PIN_MISO
#define CONFIG_ROBOMIND_DISPLAY_PIN_MISO 13  // Kconfig default: 13
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_PIN_CLK
#define CONFIG_ROBOMIND_DISPLAY_PIN_CLK 12  // Kconfig default: 12
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_PIN_CS
#define CONFIG_ROBOMIND_DISPLAY_PIN_CS 10  // Kconfig default: 10
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_PIN_DC
#define CONFIG_ROBOMIND_DISPLAY_PIN_DC 14  // Kconfig default: 14
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_PIN_RST
#define CONFIG_ROBOMIND_DISPLAY_PIN_RST 9  // Kconfig default: 9
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_PIN_BL
#define CONFIG_ROBOMIND_DISPLAY_PIN_BL 8  // Kconfig default: 8
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_ROTATION
#define CONFIG_ROBOMIND_DISPLAY_ROTATION 0  // Kconfig default: 0
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_WIDTH
#define CONFIG_ROBOMIND_DISPLAY_WIDTH 240  // Kconfig default: 240
#endif
#ifndef CONFIG_ROBOMIND_DISPLAY_HEIGHT
#define CONFIG_ROBOMIND_DISPLAY_HEIGHT 320  // Kconfig default: 320
#endif
#ifndef CONFIG_ROBOMIND_TOUCH_SPI_HOST
#define CONFIG_ROBOMIND_TOUCH_SPI_HOST 2
#endif
#ifndef CONFIG_ROBOMIND_TOUCH_PIN_CS
#define CONFIG_ROBOMIND_TOUCH_PIN_CS 15
#endif
#ifndef CONFIG_ROBOMIND_TOUCH_PIN_IRQ
#define CONFIG_ROBOMIND_TOUCH_PIN_IRQ 13
#endif
#ifndef CONFIG_ROBOMIND_TOUCH_I2C_SDA
#define CONFIG_ROBOMIND_TOUCH_I2C_SDA 17
#endif
#ifndef CONFIG_ROBOMIND_TOUCH_I2C_SCL
#define CONFIG_ROBOMIND_TOUCH_I2C_SCL 18
#endif
#ifndef CONFIG_ROBOMIND_TOUCH_I2C_ADDR
#define CONFIG_ROBOMIND_TOUCH_I2C_ADDR 0x38
#endif

static spi_device_handle_t s_spi_device = nullptr;

namespace {
constexpr uint32_t kXpt2046ClockHz = 2 * 1000 * 1000;
constexpr uint8_t kXpt2046CmdX = 0x91;
constexpr uint8_t kXpt2046CmdY = 0xD1;
constexpr uint8_t kXpt2046CmdZ1 = 0xB1;
constexpr uint8_t kXpt2046CmdZ2 = 0xC1;
constexpr uint16_t kXpt2046AdcMax = 4095;
constexpr uint16_t kXpt2046Z1Threshold = 100;
constexpr uint16_t kXpt2046Z2Threshold = 4000;
constexpr i2c_port_t kFt6x06I2cPort = I2C_NUM_0;
constexpr uint32_t kFt6x06ClockHz = 100 * 1000;
constexpr TickType_t kTouchTimeout = pdMS_TO_TICKS(100);

spi_device_handle_t s_xpt2046_device = nullptr;
bool s_xpt2046_initialized = false;
bool s_ft6x06_initialized = false;

static int16_t ClampCoord(int value, int max_value) {
    if (value < 0) {
        return 0;
    }
    if (value >= max_value) {
        return static_cast<int16_t>(max_value - 1);
    }
    return static_cast<int16_t>(value);
}

static void ApplyTouchRotation(int16_t* x, int16_t* y) {
    const int width = CONFIG_ROBOMIND_DISPLAY_WIDTH;
    const int height = CONFIG_ROBOMIND_DISPLAY_HEIGHT;
    int tx = *x;
    int ty = *y;

#if CONFIG_ROBOMIND_DISPLAY_ROTATION == 1
    tx = width - 1 - ((*y) * width / height);
    ty = (*x) * height / width;
#elif CONFIG_ROBOMIND_DISPLAY_ROTATION == 2
    tx = width - 1 - *x;
    ty = height - 1 - *y;
#elif CONFIG_ROBOMIND_DISPLAY_ROTATION == 3
    tx = (*y) * width / height;
    ty = height - 1 - ((*x) * height / width);
#endif

    *x = ClampCoord(tx, width);
    *y = ClampCoord(ty, height);
}

static bool InitXpt2046() {
    if (s_xpt2046_initialized) {
        return true;
    }

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.mode = 0;
    dev_cfg.clock_speed_hz = kXpt2046ClockHz;
    dev_cfg.spics_io_num = CONFIG_ROBOMIND_TOUCH_PIN_CS;
    dev_cfg.queue_size = 1;

    esp_err_t ret = spi_bus_add_device(
        static_cast<spi_host_device_t>(CONFIG_ROBOMIND_TOUCH_SPI_HOST),
        &dev_cfg, &s_xpt2046_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "XPT2046 SPI device add failed: %s", esp_err_to_name(ret));
        s_xpt2046_device = nullptr;
        return false;
    }

    if (CONFIG_ROBOMIND_TOUCH_PIN_IRQ >= 0) {
        ret = gpio_set_direction(static_cast<gpio_num_t>(CONFIG_ROBOMIND_TOUCH_PIN_IRQ),
                                 GPIO_MODE_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "XPT2046 IRQ gpio direction failed: %s", esp_err_to_name(ret));
            return false;
        }
        ret = gpio_set_pull_mode(static_cast<gpio_num_t>(CONFIG_ROBOMIND_TOUCH_PIN_IRQ),
                                 GPIO_PULLUP_ONLY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "XPT2046 IRQ gpio pull-up failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    s_xpt2046_initialized = true;
    ESP_LOGI(TAG, "XPT2046 touch initialized on SPI host %d CS=%d",
             CONFIG_ROBOMIND_TOUCH_SPI_HOST, CONFIG_ROBOMIND_TOUCH_PIN_CS);
    return true;
}

static uint16_t ReadXpt2046(uint8_t cmd) {
    if (!s_xpt2046_device) {
        ESP_LOGE(TAG, "XPT2046 read called before init");
        return 0;
    }

    const uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3] = {};
    spi_transaction_t transaction = {};
    transaction.length = sizeof(tx) * 8;
    transaction.tx_buffer = tx;
    transaction.rx_buffer = rx;

    esp_err_t ret = spi_device_transmit(s_xpt2046_device, &transaction);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "XPT2046 cmd 0x%02X read failed: %s", cmd, esp_err_to_name(ret));
        return 0;
    }
    esp_rom_delay_us(1500);

    return static_cast<uint16_t>(((rx[1] << 8) | rx[2]) >> 3) & kXpt2046AdcMax;
}

static bool ReadXpt2046Touch(int16_t* x, int16_t* y) {
    if (!s_xpt2046_initialized && !InitXpt2046()) {
        return false;
    }

    if (CONFIG_ROBOMIND_TOUCH_PIN_IRQ >= 0 &&
        gpio_get_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_TOUCH_PIN_IRQ)) != 0) {
        return false;
    }

    const uint16_t z1 = ReadXpt2046(kXpt2046CmdZ1);
    const uint16_t z2 = ReadXpt2046(kXpt2046CmdZ2);
    if (z1 <= kXpt2046Z1Threshold || z2 >= kXpt2046Z2Threshold) {
        return false;
    }

    const uint16_t raw_x = ReadXpt2046(kXpt2046CmdX);
    const uint16_t raw_y = ReadXpt2046(kXpt2046CmdY);
    *x = ClampCoord(static_cast<int>(raw_x) * CONFIG_ROBOMIND_DISPLAY_WIDTH /
                        kXpt2046AdcMax,
                    CONFIG_ROBOMIND_DISPLAY_WIDTH);
    *y = ClampCoord(static_cast<int>(raw_y) * CONFIG_ROBOMIND_DISPLAY_HEIGHT /
                        kXpt2046AdcMax,
                    CONFIG_ROBOMIND_DISPLAY_HEIGHT);
    ApplyTouchRotation(x, y);
    return true;
}

static bool InitFt6x06() {
    if (s_ft6x06_initialized) {
        return true;
    }

    i2c_config_t config = {};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = static_cast<gpio_num_t>(CONFIG_ROBOMIND_TOUCH_I2C_SDA);
    config.scl_io_num = static_cast<gpio_num_t>(CONFIG_ROBOMIND_TOUCH_I2C_SCL);
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = kFt6x06ClockHz;

    esp_err_t ret = i2c_param_config(kFt6x06I2cPort, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FT6x06 I2C param config failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = i2c_driver_install(kFt6x06I2cPort, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FT6x06 I2C driver install failed: %s", esp_err_to_name(ret));
        return false;
    }

    s_ft6x06_initialized = true;
    ESP_LOGI(TAG, "FT6x06 touch initialized on I2C addr=0x%02X",
             CONFIG_ROBOMIND_TOUCH_I2C_ADDR);
    return true;
}

static bool ReadFt6x06(uint8_t reg, uint8_t* data, size_t len) {
    if (!data || len == 0) {
        ESP_LOGE(TAG, "FT6x06 read called with invalid buffer");
        return false;
    }
    if (!s_ft6x06_initialized && !InitFt6x06()) {
        return false;
    }

    esp_err_t ret = i2c_master_write_read_device(
        kFt6x06I2cPort, CONFIG_ROBOMIND_TOUCH_I2C_ADDR, &reg, 1, data, len,
        kTouchTimeout);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FT6x06 reg 0x%02X read failed: %s", reg, esp_err_to_name(ret));
        return false;
    }
    return true;
}

static bool ReadFt6x06Touch(int16_t* x, int16_t* y) {
    uint8_t status = 0;
    if (!ReadFt6x06(0x02, &status, 1)) {
        return false;
    }
    const uint8_t touches = status & 0x0F;
    if (touches == 0) {
        return false;
    }

    uint8_t point[6] = {};
    if (!ReadFt6x06(0x03, point, sizeof(point))) {
        return false;
    }

    const uint16_t raw_x = (static_cast<uint16_t>(point[0] & 0x3F) << 6) |
                           (point[1] & 0x3F);
    const uint16_t raw_y = (static_cast<uint16_t>(point[2] & 0x0F) << 8) |
                           point[3];
    *x = ClampCoord(raw_x, CONFIG_ROBOMIND_DISPLAY_WIDTH);
    *y = ClampCoord(raw_y, CONFIG_ROBOMIND_DISPLAY_HEIGHT);
    ApplyTouchRotation(x, y);
    return true;
}
}  // namespace

DisplayDriver* DisplayDriver::GetInstance() {
    static DisplayDriver instance;
    return &instance;
}

DisplayDriver::~DisplayDriver() {
    if (tick_timer_) {
        esp_timer_stop(tick_timer_);
        esp_timer_delete(tick_timer_);
        tick_timer_ = nullptr;
    }
    if (s_xpt2046_device) {
        esp_err_t ret = spi_bus_remove_device(s_xpt2046_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "XPT2046 SPI device remove failed: %s", esp_err_to_name(ret));
        }
        s_xpt2046_device = nullptr;
        s_xpt2046_initialized = false;
    }
    if (s_ft6x06_initialized) {
        esp_err_t ret = i2c_driver_delete(kFt6x06I2cPort);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "FT6x06 I2C driver delete failed: %s", esp_err_to_name(ret));
        }
        s_ft6x06_initialized = false;
    }
    if (buf1_) { heap_caps_free(buf1_); buf1_ = nullptr; }
    if (buf2_) { heap_caps_free(buf2_); buf2_ = nullptr; }
}

bool DisplayDriver::Initialize() {
    display_width_ = CONFIG_ROBOMIND_DISPLAY_WIDTH;
    display_height_ = CONFIG_ROBOMIND_DISPLAY_HEIGHT;

    ESP_LOGI(TAG, "Initializing display %dx%d", display_width_, display_height_);

    // --- 1. 背光 PWM ---
    if (CONFIG_ROBOMIND_DISPLAY_PIN_BL >= 0) {
        ledc_timer_config_t ledc_timer = {};
        ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
        ledc_timer.timer_num = LEDC_TIMER_0;
        ledc_timer.freq_hz = 5000;
        ledc_timer.clk_cfg = LEDC_AUTO_CLK;
        esp_err_t ret = ledc_timer_config(&ledc_timer);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "LEDC timer config failed: %s (backlight may not work)",
                     esp_err_to_name(ret));
        }

        ledc_channel_config_t ledc_channel = {};
        ledc_channel.gpio_num = CONFIG_ROBOMIND_DISPLAY_PIN_BL;
        ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel.channel = LEDC_CHANNEL_0;
        ledc_channel.timer_sel = LEDC_TIMER_0;
        ledc_channel.duty = 1023;  // 100% brightness
        ledc_channel.hpoint = 0;
        ret = ledc_channel_config(&ledc_channel);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        }
    }

    // --- 2. SPI 总线 ---
    if (!InitSpiBus()) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return false;
    }

    // --- 3. LCD 控制器 ---
    if (!InitLcdController()) {
        ESP_LOGE(TAG, "LCD controller init failed");
        return false;
    }

    // --- 4. LVGL 初始化 ---
    lv_init();

    // 分配 LVGL 绘制缓冲区 (PSRAM)
    const size_t buf_size = display_width_ * display_height_ / 4;  // 25% screen
    buf1_ = static_cast<lv_color_t*>(
        heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
    buf2_ = static_cast<lv_color_t*>(
        heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));

    if (!buf1_ || !buf2_) {
        ESP_LOGE(TAG, "LVGL buffer allocation failed (need PSRAM)");
        if (buf1_) { heap_caps_free(buf1_); buf1_ = nullptr; }
        if (buf2_) { heap_caps_free(buf2_); buf2_ = nullptr; }
        return false;
    }

    lv_disp_draw_buf_init(&draw_buf_, buf1_, buf2_, buf_size);

    // 注册 display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = display_width_;
    disp_drv.ver_res = display_height_;
    disp_drv.flush_cb = LvglFlushCallback;
    disp_drv.draw_buf = &draw_buf_;
    display_ = lv_disp_drv_register(&disp_drv);
    if (!display_) {
        ESP_LOGE(TAG, "LVGL display driver registration failed");
        return false;
    }

    // 注册 touch input device (如果启用)
#if CONFIG_ROBOMIND_TOUCH_ENABLE
#if CONFIG_ROBOMIND_TOUCH_XPT2046
    if (!InitXpt2046()) {
        ESP_LOGE(TAG, "XPT2046 touch init failed");
        return false;
    }
#elif CONFIG_ROBOMIND_TOUCH_FT6X06
    if (!InitFt6x06()) {
        ESP_LOGE(TAG, "FT6x06 touch init failed");
        return false;
    }
#endif
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = LvglTouchCallback;
    lv_indev_drv_register(&indev_drv);
#endif

    // --- 5. LVGL tick timer ---
    const esp_timer_create_args_t timer_args = {
        .callback = [](void*) { lv_tick_inc(kTickIntervalMs); },
        .name = "lvgl_tick",
    };
    esp_err_t ret = esp_timer_create(&timer_args, &tick_timer_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL tick timer create failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = esp_timer_start_periodic(tick_timer_, kTickIntervalMs * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL tick timer start failed: %s", esp_err_to_name(ret));
        esp_timer_delete(tick_timer_);
        tick_timer_ = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Display + LVGL initialized OK");
    return true;
}

bool DisplayDriver::InitSpiBus() {
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = CONFIG_ROBOMIND_DISPLAY_PIN_MOSI;
    bus_cfg.miso_io_num = (CONFIG_ROBOMIND_DISPLAY_PIN_MISO >= 0)
                              ? CONFIG_ROBOMIND_DISPLAY_PIN_MISO
                              : -1;
    bus_cfg.sclk_io_num = CONFIG_ROBOMIND_DISPLAY_PIN_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = display_width_ * display_height_ * 2 + 32;

    esp_err_t ret = spi_bus_initialize(
        static_cast<spi_host_device_t>(CONFIG_ROBOMIND_DISPLAY_SPI_HOST),
        &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init error: %s", esp_err_to_name(ret));
        return false;
    }

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.mode = 0;
    dev_cfg.clock_speed_hz = kSpiClockHz;
    dev_cfg.spics_io_num = CONFIG_ROBOMIND_DISPLAY_PIN_CS;
    dev_cfg.queue_size = 7;

    ret = spi_bus_add_device(
        static_cast<spi_host_device_t>(CONFIG_ROBOMIND_DISPLAY_SPI_HOST),
        &dev_cfg, &s_spi_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add error: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "SPI bus initialized: host=%d, clk=%dHz",
             CONFIG_ROBOMIND_DISPLAY_SPI_HOST, kSpiClockHz);
    return true;
}

// --- LCD command write helpers ---
static bool LcdWriteCommand(uint8_t cmd) {
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 0);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    esp_err_t ret = spi_device_transmit(s_spi_device, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write command 0x%02X failed: %s", cmd, esp_err_to_name(ret));
        return false;
    }
    return true;
}

static bool LcdWriteData(uint8_t data) {
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 1);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    esp_err_t ret = spi_device_transmit(s_spi_device, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write data 0x%02X failed: %s", data, esp_err_to_name(ret));
        return false;
    }
    return true;
}

static bool LcdWriteData16(uint16_t data) {
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 1);
    spi_transaction_t t = {};
    t.length = 16;
    t.tx_buffer = &data;
    esp_err_t ret = spi_device_transmit(s_spi_device, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write data16 0x%04X failed: %s", data, esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool DisplayDriver::InitLcdController() {
    // RST 引脚初始化
    if (CONFIG_ROBOMIND_DISPLAY_PIN_RST >= 0) {
        gpio_set_direction(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_RST),
                           GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_RST), 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_RST), 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_RST), 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    // DC 引脚
    gpio_set_direction(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC),
                       GPIO_MODE_OUTPUT);

// --- Display-specific init sequences ---
#if DISPLAY_DRIVER_ILI9341
    if (!LcdWriteCommand(0x01)) return false;  // SW reset
    vTaskDelay(pdMS_TO_TICKS(150));

    if (!LcdWriteCommand(0x11)) return false;  // Sleep out
    vTaskDelay(pdMS_TO_TICKS(120));

    if (!LcdWriteCommand(0x36)) return false;  // MADCTL
#if CONFIG_ROBOMIND_DISPLAY_ROTATION == 0
    if (!LcdWriteData(0x48)) return false;  // Portrait: MX | BGR
#elif CONFIG_ROBOMIND_DISPLAY_ROTATION == 1
    if (!LcdWriteData(0x88)) return false;  // Landscape: MV | MX | BGR
#elif CONFIG_ROBOMIND_DISPLAY_ROTATION == 2
    if (!LcdWriteData(0x28)) return false;  // Portrait inverted: MY | BGR
#else
    if (!LcdWriteData(0xE8)) return false;  // Landscape inverted: MV | MX | MY | BGR
#endif

    if (!LcdWriteCommand(0x3A)) return false;  // Pixel format
    if (!LcdWriteData(0x55)) return false;     // 16bpp

    if (!LcdWriteCommand(0x29)) return false;  // Display on
    vTaskDelay(pdMS_TO_TICKS(20));

#elif DISPLAY_DRIVER_ST7789
    if (!LcdWriteCommand(0x01)) return false;  // SW reset
    vTaskDelay(pdMS_TO_TICKS(150));

    if (!LcdWriteCommand(0x11)) return false;  // Sleep out
    vTaskDelay(pdMS_TO_TICKS(120));

    if (!LcdWriteCommand(0x36)) return false;  // MADCTL
#if CONFIG_ROBOMIND_DISPLAY_ROTATION == 0
    if (!LcdWriteData(0x48)) return false;  // Portrait: MX | BGR
#elif CONFIG_ROBOMIND_DISPLAY_ROTATION == 1
    if (!LcdWriteData(0x88)) return false;  // Landscape: MV | MX | BGR
#elif CONFIG_ROBOMIND_DISPLAY_ROTATION == 2
    if (!LcdWriteData(0x28)) return false;  // Portrait inverted: MY | BGR
#else
    if (!LcdWriteData(0xE8)) return false;  // Landscape inverted: MV | MX | MY | BGR
#endif

    if (!LcdWriteCommand(0x3A)) return false;  // Pixel format
    if (!LcdWriteData(0x55)) return false;     // 16bpp

    if (!LcdWriteCommand(0x29)) return false;  // Display on
    vTaskDelay(pdMS_TO_TICKS(20));

#elif DISPLAY_DRIVER_GC9A01
    if (!LcdWriteCommand(0x01)) return false;  // SW reset
    vTaskDelay(pdMS_TO_TICKS(150));

    if (!LcdWriteCommand(0xEF)) return false;
    if (!LcdWriteCommand(0xEB)) return false; if (!LcdWriteData(0x14)) return false;
    if (!LcdWriteCommand(0xFE)) return false;
    if (!LcdWriteCommand(0xEF)) return false;
    if (!LcdWriteCommand(0xEB)) return false; if (!LcdWriteData(0x14)) return false;
    if (!LcdWriteCommand(0x84)) return false; if (!LcdWriteData(0x40)) return false;
    if (!LcdWriteCommand(0x85)) return false; if (!LcdWriteData(0xFF)) return false;
    if (!LcdWriteCommand(0x86)) return false; if (!LcdWriteData(0xFF)) return false;
    if (!LcdWriteCommand(0x87)) return false; if (!LcdWriteData(0xFF)) return false;
    if (!LcdWriteCommand(0x88)) return false; if (!LcdWriteData(0x0A)) return false;
    if (!LcdWriteCommand(0x89)) return false; if (!LcdWriteData(0x21)) return false;
    if (!LcdWriteCommand(0x8A)) return false; if (!LcdWriteData(0x00)) return false;
    if (!LcdWriteCommand(0x8B)) return false; if (!LcdWriteData(0x80)) return false;
    if (!LcdWriteCommand(0x8C)) return false; if (!LcdWriteData(0x01)) return false;
    if (!LcdWriteCommand(0x8D)) return false; if (!LcdWriteData(0x01)) return false;
    if (!LcdWriteCommand(0x8E)) return false; if (!LcdWriteData(0xFF)) return false;
    if (!LcdWriteCommand(0x8F)) return false; if (!LcdWriteData(0xFF)) return false;
    if (!LcdWriteCommand(0xB6)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x20)) return false;
    if (!LcdWriteCommand(0x36)) return false;
#if CONFIG_ROBOMIND_DISPLAY_ROTATION == 0
    if (!LcdWriteData(0x08)) return false;
#else
    if (!LcdWriteData(0x68)) return false;
#endif
    if (!LcdWriteCommand(0x3A)) return false; if (!LcdWriteData(0x05)) return false;
    if (!LcdWriteCommand(0x90)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x08)) return false;
    if (!LcdWriteCommand(0xBD)) return false; if (!LcdWriteData(0x06)) return false;
    if (!LcdWriteCommand(0xBC)) return false; if (!LcdWriteData(0x00)) return false;
    if (!LcdWriteCommand(0xFF)) return false; if (!LcdWriteData(0x60)) return false; if (!LcdWriteData(0x01)) return false; if (!LcdWriteData(0x04)) return false;
    if (!LcdWriteCommand(0xC3)) return false; if (!LcdWriteData(0x13)) return false;
    if (!LcdWriteCommand(0xC4)) return false; if (!LcdWriteData(0x13)) return false;
    if (!LcdWriteCommand(0xC9)) return false; if (!LcdWriteData(0x22)) return false;
    if (!LcdWriteCommand(0xBE)) return false; if (!LcdWriteData(0x11)) return false;
    if (!LcdWriteCommand(0xE1)) return false; if (!LcdWriteData(0x10)) return false; if (!LcdWriteData(0x0E)) return false;
    if (!LcdWriteCommand(0xDF)) return false; if (!LcdWriteData(0x21)) return false; if (!LcdWriteData(0x0C)) return false; if (!LcdWriteData(0x02)) return false;
    if (!LcdWriteCommand(0xF0)) return false; if (!LcdWriteData(0x45)) return false; if (!LcdWriteData(0x09)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x26)) return false; if (!LcdWriteData(0x2A)) return false;
    if (!LcdWriteCommand(0xF1)) return false; if (!LcdWriteData(0x43)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x72)) return false; if (!LcdWriteData(0x36)) return false; if (!LcdWriteData(0x37)) return false; if (!LcdWriteData(0x6F)) return false;
    if (!LcdWriteCommand(0xF2)) return false; if (!LcdWriteData(0x45)) return false; if (!LcdWriteData(0x09)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x26)) return false; if (!LcdWriteData(0x2A)) return false;
    if (!LcdWriteCommand(0xF3)) return false; if (!LcdWriteData(0x43)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x72)) return false; if (!LcdWriteData(0x36)) return false; if (!LcdWriteData(0x37)) return false; if (!LcdWriteData(0x6F)) return false;
    if (!LcdWriteCommand(0xED)) return false; if (!LcdWriteData(0x1B)) return false; if (!LcdWriteData(0x0B)) return false;
    if (!LcdWriteCommand(0xAE)) return false; if (!LcdWriteData(0x77)) return false;
    if (!LcdWriteCommand(0xCD)) return false; if (!LcdWriteData(0x63)) return false;
    if (!LcdWriteCommand(0x70)) return false; if (!LcdWriteData(0x07)) return false; if (!LcdWriteData(0x07)) return false; if (!LcdWriteData(0x04)) return false; if (!LcdWriteData(0x0E)) return false; if (!LcdWriteData(0x0F)) return false; if (!LcdWriteData(0x09)) return false; if (!LcdWriteData(0x07)) return false; if (!LcdWriteData(0x08)) return false; if (!LcdWriteData(0x03)) return false;
    if (!LcdWriteCommand(0xE8)) return false; if (!LcdWriteData(0x34)) return false;
    if (!LcdWriteCommand(0x62)) return false; if (!LcdWriteData(0x18)) return false; if (!LcdWriteData(0x0D)) return false; if (!LcdWriteData(0x71)) return false; if (!LcdWriteData(0xED)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x18)) return false; if (!LcdWriteData(0x0D)) return false; if (!LcdWriteData(0x71)) return false; if (!LcdWriteData(0xEF)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x70)) return false;
    if (!LcdWriteCommand(0x63)) return false; if (!LcdWriteData(0x18)) return false; if (!LcdWriteData(0x11)) return false; if (!LcdWriteData(0x71)) return false; if (!LcdWriteData(0xF1)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x18)) return false; if (!LcdWriteData(0x11)) return false; if (!LcdWriteData(0x71)) return false; if (!LcdWriteData(0xF1)) return false; if (!LcdWriteData(0x70)) return false; if (!LcdWriteData(0x70)) return false;
    if (!LcdWriteCommand(0x64)) return false; if (!LcdWriteData(0x28)) return false; if (!LcdWriteData(0x29)) return false; if (!LcdWriteData(0xF1)) return false; if (!LcdWriteData(0x01)) return false; if (!LcdWriteData(0xF1)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x07)) return false;
    if (!LcdWriteCommand(0x66)) return false; if (!LcdWriteData(0x3C)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0xCD)) return false; if (!LcdWriteData(0x67)) return false; if (!LcdWriteData(0x45)) return false; if (!LcdWriteData(0x45)) return false; if (!LcdWriteData(0x10)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x00)) return false;
    if (!LcdWriteCommand(0x67)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x3C)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x01)) return false; if (!LcdWriteData(0x54)) return false; if (!LcdWriteData(0x10)) return false; if (!LcdWriteData(0x32)) return false; if (!LcdWriteData(0x98)) return false;
    if (!LcdWriteCommand(0x74)) return false; if (!LcdWriteData(0x10)) return false; if (!LcdWriteData(0x85)) return false; if (!LcdWriteData(0x80)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x00)) return false; if (!LcdWriteData(0x4E)) return false; if (!LcdWriteData(0x00)) return false;
    if (!LcdWriteCommand(0x98)) return false; if (!LcdWriteData(0x3E)) return false; if (!LcdWriteData(0x07)) return false;
    if (!LcdWriteCommand(0x35)) return false; if (!LcdWriteData(0x00)) return false;
    if (!LcdWriteCommand(0x11)) return false;
    vTaskDelay(pdMS_TO_TICKS(120));
    if (!LcdWriteCommand(0x29)) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
#endif

    // --- 通用设置 ---
    // Column/Page addressing: set full window
    if (!LcdWriteCommand(0x2A)) return false;  // CASET
    if (!LcdWriteData16(0)) return false;
    if (!LcdWriteData16(display_width_ - 1)) return false;

    if (!LcdWriteCommand(0x2B)) return false;  // RASET
    if (!LcdWriteData16(0)) return false;
    if (!LcdWriteData16(display_height_ - 1)) return false;

    if (!LcdWriteCommand(0x2C)) return false;  // RAMWR — ready for pixel data

    ESP_LOGI(TAG, "LCD controller initialized");
    return true;
}

void DisplayDriver::LvglFlushCallback(lv_disp_drv_t* /*disp*/,
                                      const lv_area_t* area,
                                      lv_color_t* color_p) {
    auto* self = GetInstance();

    // Set write window: CASET command followed by coordinate data.
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 0);
    uint8_t caset_cmd = 0x2A;
    spi_transaction_t t_caset_cmd = {};
    t_caset_cmd.length = 8;
    t_caset_cmd.tx_buffer = &caset_cmd;
    spi_device_transmit(s_spi_device, &t_caset_cmd);

    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 1);
    uint8_t caset_data[4] = {
        static_cast<uint8_t>((area->x1 >> 8) & 0xFF),
        static_cast<uint8_t>(area->x1 & 0xFF),
        static_cast<uint8_t>((area->x2 >> 8) & 0xFF),
        static_cast<uint8_t>(area->x2 & 0xFF),
    };
    spi_transaction_t t_caset_data = {};
    t_caset_data.length = sizeof(caset_data) * 8;
    t_caset_data.tx_buffer = caset_data;
    spi_device_transmit(s_spi_device, &t_caset_data);

    // RASET command followed by coordinate data.
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 0);
    uint8_t raset_cmd = 0x2B;
    spi_transaction_t t_raset_cmd = {};
    t_raset_cmd.length = 8;
    t_raset_cmd.tx_buffer = &raset_cmd;
    spi_device_transmit(s_spi_device, &t_raset_cmd);

    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 1);
    uint8_t raset_data[4] = {
        static_cast<uint8_t>((area->y1 >> 8) & 0xFF),
        static_cast<uint8_t>(area->y1 & 0xFF),
        static_cast<uint8_t>((area->y2 >> 8) & 0xFF),
        static_cast<uint8_t>(area->y2 & 0xFF),
    };
    spi_transaction_t t_raset_data = {};
    t_raset_data.length = sizeof(raset_data) * 8;
    t_raset_data.tx_buffer = raset_data;
    spi_device_transmit(s_spi_device, &t_raset_data);

    // RAMWR 命令
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 0);
    uint8_t ramwr = 0x2C;
    spi_transaction_t t5 = {};
    t5.length = 8; t5.tx_buffer = &ramwr;
    spi_device_transmit(s_spi_device, &t5);

    // 发送像素数据
    uint32_t pixel_count = static_cast<uint32_t>(area->x2 - area->x1 + 1) *
                           static_cast<uint32_t>(area->y2 - area->y1 + 1);

    gpio_set_level(static_cast<gpio_num_t>(CONFIG_ROBOMIND_DISPLAY_PIN_DC), 1);
    spi_transaction_t t6 = {};
    t6.length = pixel_count * 16;
    t6.tx_buffer = color_p;
    spi_device_transmit(s_spi_device, &t6);

    lv_disp_flush_ready(self->display_);
}

void DisplayDriver::LvglTouchCallback(lv_indev_drv_t* /*indev*/,
                                      lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_RELEASED;

#if CONFIG_ROBOMIND_TOUCH_ENABLE
    int16_t x = 0;
    int16_t y = 0;
    bool touched = false;

#if CONFIG_ROBOMIND_TOUCH_XPT2046
    touched = ReadXpt2046Touch(&x, &y);
#elif CONFIG_ROBOMIND_TOUCH_FT6X06
    touched = ReadFt6x06Touch(&x, &y);
#endif

    if (touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    }
#endif
}

void DisplayDriver::RunLvglLoop() {
    ESP_LOGI(TAG, "Starting LVGL loop");

    esp_task_wdt_add(nullptr);

    const TickType_t period = pdMS_TO_TICKS(kTickIntervalMs);
    while (true) {
        esp_task_wdt_reset();
        vTaskDelay(period);
        lv_timer_handler();
    }
}

void DisplayDriver::SetBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t duty = static_cast<uint32_t>(percent) * 1023 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
