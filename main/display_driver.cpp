/**
 * @file display_driver.cpp
 * @brief Display driver using ESP-IDF esp_lcd for ATK-DNESP32S3
 *
 * Reference: 78/xiaozhi-esp32 atk-dnesp32s3 board config
 * Uses esp_lcd_panel_st7789 (proven working on this board)
 */

#include "display_driver.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_commands.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "display";

// --- XL9555 helpers (I2C addr 0x20, GPIO 41=SDA 42=SCL) ---
static constexpr uint8_t kXl9555OutputPort0 = 0x02;
static constexpr uint8_t kXl9555OutputPort1 = 0x03;
static constexpr uint8_t kXl9555ConfigPort0 = 0x03;
static constexpr uint8_t kXl9555ConfigPort1 = 0xF0;
static bool g_xl9555_ok = false;
static uint8_t g_xl9555_output_latch[2] = {0xFF, 0xFF};

static bool Xl9555WriteReg(uint8_t reg, uint8_t val) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x20 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

static bool Xl9555ReadReg(uint8_t reg, uint8_t* val) {
    if (!val) return false;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x20 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x20 << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

static bool SetXl9555Output(uint8_t pin, bool level) {
    if (pin > 15) return false;

    const uint8_t port = pin / 8;
    const uint8_t bit = pin % 8;
    const uint8_t reg = (port == 0) ? kXl9555OutputPort0 : kXl9555OutputPort1;

    uint8_t value = g_xl9555_output_latch[port];
    uint8_t read_value = 0;
    if (Xl9555ReadReg(reg, &read_value)) {
        value = read_value;
    }

    if (level) {
        value |= static_cast<uint8_t>(1U << bit);
    } else {
        value &= static_cast<uint8_t>(~(1U << bit));
    }

    if (!Xl9555WriteReg(reg, value)) {
        ESP_LOGE(TAG, "XL9555 set pin %u failed", static_cast<unsigned>(pin));
        return false;
    }
    g_xl9555_output_latch[port] = value;
    return true;
}

static bool CheckEsp(esp_err_t ret, const char* what) {
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: %s", what, esp_err_to_name(ret));
        return false;
    }
    return true;
}

static esp_lcd_spi_bus_handle_t ToLcdSpiBusHandle(spi_host_device_t host) {
    // IDF 5.2's legacy esp_lcd SPI API stores the host enum in a void* handle.
    return reinterpret_cast<esp_lcd_spi_bus_handle_t>(static_cast<intptr_t>(host));
}

static uint16_t Rgb565ToPanelEndian(uint16_t color) {
    return static_cast<uint16_t>((color << 8) | (color >> 8));
}

// --- Touch stubs (kept for compatibility, not fatal) ---
namespace {
spi_device_handle_t s_xpt2046_device = nullptr;
bool s_xpt2046_initialized = false;

static bool InitXpt2046() {
    if (s_xpt2046_initialized) return true;
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.mode = 0;
    dev_cfg.clock_speed_hz = 2 * 1000 * 1000;
    dev_cfg.spics_io_num = CONFIG_ROBOMIND_TOUCH_PIN_CS;
    dev_cfg.queue_size = 1;
    esp_err_t ret = spi_bus_add_device(
        static_cast<spi_host_device_t>(CONFIG_ROBOMIND_TOUCH_SPI_HOST),
        &dev_cfg, &s_xpt2046_device);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "XPT2046 SPI device add failed: %s (touch optional)", esp_err_to_name(ret));
        return false;
    }
    s_xpt2046_initialized = true;
    return true;
}

static bool ReadXpt2046Touch(int16_t* x, int16_t* y) {
    if (!s_xpt2046_initialized) return false;
    // ... stub returns false ...
    return false;
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
    }
    if (panel_) esp_lcd_panel_del(panel_);
    if (panel_io_) esp_lcd_panel_io_del(panel_io_);
    if (buf1_) heap_caps_free(buf1_);
    if (buf2_) heap_caps_free(buf2_);
}

bool DisplayDriver::InitXl9555() {
    if (g_xl9555_ok) return true;

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_41;
    conf.scl_io_num = GPIO_NUM_42;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;

    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "XL9555 I2C param: %s", esp_err_to_name(ret)); return false; }
    ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "XL9555 I2C driver: %s", esp_err_to_name(ret)); return false;
    }

    // Match the ATK-DNESP32S3 reference: pins 2..7 and 8..11 are outputs.
    if (!Xl9555WriteReg(0x06, kXl9555ConfigPort0) ||
        !Xl9555WriteReg(0x07, kXl9555ConfigPort1)) {
        ESP_LOGE(TAG, "XL9555 GPIO direction config failed");
        return false;
    }

    ESP_LOGI(TAG, "XL9555 OK: config P0=0x%02X P1=0x%02X",
             kXl9555ConfigPort0, kXl9555ConfigPort1);
    g_xl9555_ok = true;
    return true;
}

bool DisplayDriver::InitSpiBus() {
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = CONFIG_ROBOMIND_DISPLAY_PIN_MOSI;
    bus_cfg.miso_io_num = (CONFIG_ROBOMIND_DISPLAY_PIN_MISO >= 0)
                              ? CONFIG_ROBOMIND_DISPLAY_PIN_MISO : -1;
    bus_cfg.sclk_io_num = CONFIG_ROBOMIND_DISPLAY_PIN_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = display_width_ * display_height_ * 2 + 32;

    esp_err_t ret = spi_bus_initialize(
        static_cast<spi_host_device_t>(CONFIG_ROBOMIND_DISPLAY_SPI_HOST),
        &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "SPI bus OK: host=%d clk=%dHz", CONFIG_ROBOMIND_DISPLAY_SPI_HOST, kSpiClockHz);
    return true;
}

bool DisplayDriver::InitLcdPanel() {
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = CONFIG_ROBOMIND_DISPLAY_PIN_CS;
    io_config.dc_gpio_num = CONFIG_ROBOMIND_DISPLAY_PIN_DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = kSpiClockHz;
    io_config.trans_queue_depth = 7;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;

    const auto spi_host = static_cast<spi_host_device_t>(CONFIG_ROBOMIND_DISPLAY_SPI_HOST);
    esp_err_t ret = esp_lcd_new_panel_io_spi(
        ToLcdSpiBusHandle(spi_host), &io_config, &panel_io_);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Panel IO: %s", esp_err_to_name(ret)); return false; }

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;  // via XL9555
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
    panel_config.bits_per_pixel = 16;

    ret = esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Panel ST7789: %s", esp_err_to_name(ret)); return false; }

    if (!CheckEsp(esp_lcd_panel_reset(panel_), "Panel reset")) return false;

    if (g_xl9555_ok) {
        if (!SetXl9555Output(8, true)) return false;
        if (!SetXl9555Output(2, false)) return false;
        ESP_LOGI(TAG, "XL9555 LCD outputs set: pin8=1 pin2=0");
    }

    if (!CheckEsp(esp_lcd_panel_init(panel_), "Panel init")) return false;
    if (!CheckEsp(esp_lcd_panel_invert_color(panel_, true), "Panel invert")) return false;

    // Mirror + swap per xiaozhi-esp32 reference (MADCTL = MV|MX; RGB order).
    if (!CheckEsp(esp_lcd_panel_swap_xy(panel_, true), "Panel swap_xy")) return false;
    if (!CheckEsp(esp_lcd_panel_mirror(panel_, true, false), "Panel mirror")) return false;

    if (!CheckEsp(esp_lcd_panel_disp_on_off(panel_, true), "Panel display on")) return false;

    ESP_LOGI(TAG, "LCD panel OK: ST7789 320x240");
    return true;
}

bool DisplayDriver::RunSolidColorSelfTest() {
    static constexpr int kRowsPerChunk = 16;
    struct TestColor {
        uint16_t rgb565;
        const char* name;
    };
    static constexpr TestColor kColors[] = {
        {0xF800, "red"},
        {0x07E0, "green"},
        {0x001F, "blue"},
    };

    const size_t pixels = static_cast<size_t>(display_width_) * kRowsPerChunk;
    auto* strip = static_cast<uint16_t*>(
        heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (!strip) {
        ESP_LOGE(TAG, "Solid color test buffer alloc failed");
        return false;
    }

    for (const auto& color : kColors) {
        const uint16_t panel_color = Rgb565ToPanelEndian(color.rgb565);
        for (size_t i = 0; i < pixels; ++i) {
            strip[i] = panel_color;
        }

        for (int y = 0; y < display_height_; y += kRowsPerChunk) {
            int y_end = y + kRowsPerChunk;
            if (y_end > display_height_) y_end = display_height_;
            esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_, 0, y, display_width_, y_end, strip);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Solid color %s draw failed: %s", color.name, esp_err_to_name(ret));
                heap_caps_free(strip);
                return false;
            }
        }

        // NOP drains queued color transfers before reusing the strip for the next color.
        if (!CheckEsp(esp_lcd_panel_io_tx_param(panel_io_, LCD_CMD_NOP, nullptr, 0),
                      "Panel transfer drain")) {
            heap_caps_free(strip);
            return false;
        }
        ESP_LOGI(TAG, "Solid color test: %s", color.name);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    heap_caps_free(strip);
    ESP_LOGI(TAG, "Solid color SPI self-test OK");
    return true;
}

bool DisplayDriver::Initialize() {
    display_width_ = CONFIG_ROBOMIND_DISPLAY_WIDTH;
    display_height_ = CONFIG_ROBOMIND_DISPLAY_HEIGHT;

    ESP_LOGI(TAG, "Init display %dx%d", display_width_, display_height_);

    // 1. XL9555 backlight
    if (CONFIG_ROBOMIND_DISPLAY_PIN_BL < 0) {
        if (!InitXl9555()) ESP_LOGW(TAG, "XL9555 failed, screen may be dark");
    }

    // 2. SPI bus
    if (!InitSpiBus()) return false;

    // 3. ST7789 panel via esp_lcd
    if (!InitLcdPanel()) return false;

    // 4. Raw SPI smoke test before LVGL takes over
    if (!RunSolidColorSelfTest()) return false;

    // 5. LVGL
    lv_init();

    size_t buf_size = display_width_ * display_height_ / 4;
    size_t buf_bytes = buf_size * sizeof(lv_color_t);
    buf1_ = static_cast<lv_color_t*>(heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM));
    if (buf1_) {
        buf2_ = static_cast<lv_color_t*>(heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM));
        ESP_LOGI(TAG, "LVGL PSRAM buf1 OK: %u bytes", static_cast<unsigned>(buf_bytes));
        if (buf2_) {
            ESP_LOGI(TAG, "LVGL PSRAM buf2 OK: %u bytes", static_cast<unsigned>(buf_bytes));
        }
    } else {
        ESP_LOGW(TAG, "LVGL PSRAM buffer alloc failed, falling back to internal DMA RAM");
        buf_size = display_width_ * 20;
        buf_bytes = buf_size * sizeof(lv_color_t);
        buf1_ = static_cast<lv_color_t*>(
            heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
        buf2_ = static_cast<lv_color_t*>(
            heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    }
    if (!buf1_) { ESP_LOGE(TAG, "LVGL buf1 alloc failed"); return false; }
    if (!buf2_) { ESP_LOGW(TAG, "LVGL buf2 alloc failed, single buffer mode"); }

    lv_disp_draw_buf_init(&draw_buf_, buf1_, buf2_, buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = display_width_;
    disp_drv.ver_res = display_height_;
    disp_drv.flush_cb = LvglFlushCallback;
    disp_drv.draw_buf = &draw_buf_;
    display_ = lv_disp_drv_register(&disp_drv);
    if (!display_) { ESP_LOGE(TAG, "LVGL disp register failed"); return false; }

    // 6. Touch (optional)
#if CONFIG_ROBOMIND_TOUCH_ENABLE && CONFIG_ROBOMIND_TOUCH_XPT2046
    if (InitXpt2046()) {
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = LvglTouchCallback;
        lv_indev_drv_register(&indev_drv);
    }
#endif

    // 7. LVGL tick timer
    const esp_timer_create_args_t timer_args = {
        .callback = [](void*) { lv_tick_inc(kTickIntervalMs); },
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = false,
    };
    esp_err_t ret = esp_timer_create(&timer_args, &tick_timer_);
    if (!CheckEsp(ret, "LVGL tick timer create")) return false;
    ret = esp_timer_start_periodic(tick_timer_, kTickIntervalMs * 1000);
    if (!CheckEsp(ret, "LVGL tick timer start")) return false;

    ESP_LOGI(TAG, "Display + LVGL OK");
    return true;
}

void DisplayDriver::LvglFlushCallback(lv_disp_drv_t* disp, const lv_area_t* area,
                                       lv_color_t* color_p) {
    auto* self = GetInstance();
    esp_err_t ret = esp_lcd_panel_draw_bitmap(self->panel_, area->x1, area->y1,
                                              area->x2 + 1, area->y2 + 1, color_p);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL flush failed: %s", esp_err_to_name(ret));
    }
    lv_disp_flush_ready(disp);
}

void DisplayDriver::LvglTouchCallback(lv_indev_drv_t*, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_RELEASED;
#if CONFIG_ROBOMIND_TOUCH_ENABLE && CONFIG_ROBOMIND_TOUCH_XPT2046
    int16_t x = 0, y = 0;
    if (ReadXpt2046Touch(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    }
#endif
}

void DisplayDriver::RunLvglLoop() {
    ESP_LOGI(TAG, "LVGL loop start");
    const TickType_t period = pdMS_TO_TICKS(kTickIntervalMs);
    while (true) {
        vTaskDelay(period);
        lv_timer_handler();
    }
}

void DisplayDriver::SetBacklight(uint8_t percent) {
    // Backlight is XL9555 on/off, no PWM dimming for now
    if (percent > 100) percent = 100;
    if (g_xl9555_ok) {
        SetXl9555Output(8, percent > 0);
    }
}
