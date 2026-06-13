/**
 * @file display_driver.h
 * @brief 显示屏驱动 — LVGL 移植层 (单例)
 *
 * ATK-DNESP32S3: ST7789 320x240 SPI via esp_lcd + XL9555 backlight
 */

#ifndef ROBOMIND_DISPLAY_DRIVER_H
#define ROBOMIND_DISPLAY_DRIVER_H

#include <cstdint>

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_timer.h"
#include "lvgl.h"

class DisplayDriver {
public:
    static DisplayDriver* GetInstance();

    bool Initialize();
    void RunLvglLoop();
    lv_disp_t* GetDisplay() const { return display_; }
    void SetBacklight(uint8_t percent);

private:
    DisplayDriver() = default;
    ~DisplayDriver();
    DisplayDriver(const DisplayDriver&) = delete;
    DisplayDriver& operator=(const DisplayDriver&) = delete;

    bool InitSpiBus();
    bool InitLcdPanel();
    bool InitXl9555();
    bool RunSolidColorSelfTest();

    static void LvglFlushCallback(lv_disp_drv_t* disp, const lv_area_t* area,
                                  lv_color_t* color_p);
    static void LvglTouchCallback(lv_indev_drv_t* indev, lv_indev_data_t* data);
    static void LvglTickTask(void* arg);

    esp_lcd_panel_io_handle_t panel_io_{nullptr};
    esp_lcd_panel_handle_t panel_{nullptr};
    lv_disp_t* display_{nullptr};
    lv_disp_draw_buf_t draw_buf_{};
    esp_timer_handle_t tick_timer_{nullptr};
    lv_color_t* buf1_{nullptr};
    lv_color_t* buf2_{nullptr};

    int display_width_{320};
    int display_height_{240};

    static constexpr int kSpiClockHz = 20 * 1000 * 1000;
    static constexpr int kTickIntervalMs = 15;
    static constexpr int kLvglTaskStackSize = 8192;
};

#endif  // ROBOMIND_DISPLAY_DRIVER_H
