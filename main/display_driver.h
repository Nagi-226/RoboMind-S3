/**
 * @file display_driver.h
 * @brief 显示屏驱动 — LVGL 移植层 (单例)
 *
 * 职责:
 *   - 初始化 SPI 显示屏 + 背光 PWM
 *   - 提供 LVGL flush callback (像素数据推送)
 *   - 提供 LVGL touch/input callback
 *   - 提供 LVGL tick (1ms)
 *   - 运行 LVGL task loop
 *
 * 支持的驱动芯片 (通过 Kconfig 选择):
 *   - ILI9341 (SPI, 240×320)
 *   - ST7789  (SPI, 320×240, ATK-DNESP32S3 default)
 *   - ST7796  (SPI, 320×480)
 *   - GC9A01  (SPI, 240×240 Round)
 */

#ifndef ROBOMIND_DISPLAY_DRIVER_H
#define ROBOMIND_DISPLAY_DRIVER_H

#include <cstdint>

#include "esp_timer.h"
#include "lvgl.h"

class DisplayDriver {
public:
    static DisplayDriver* GetInstance();

    /// 初始化 SPI 总线 + 显示屏 + LVGL 移植
    bool Initialize();

    /// 运行 LVGL 主循环 (阻塞)
    void RunLvglLoop();

    /// 获取 LVGL display handle
    lv_disp_t* GetDisplay() const { return display_; }

    /// 设置背光亮度 0-100
    void SetBacklight(uint8_t percent);

private:
    DisplayDriver() = default;
    ~DisplayDriver();
    DisplayDriver(const DisplayDriver&) = delete;
    DisplayDriver& operator=(const DisplayDriver&) = delete;

    /// 初始化 SPI 总线
    bool InitSpiBus();

    /// 初始化 LCD 控制器 (发送命令序列)
    bool InitLcdController();

    /// LVGL flush callback — 将像素数据推送至显示屏
    static void LvglFlushCallback(lv_disp_drv_t* disp, const lv_area_t* area,
                                  lv_color_t* color_p);
    /// LVGL touch read callback — 读取触摸坐标
    static void LvglTouchCallback(lv_indev_drv_t* indev, lv_indev_data_t* data);

    /// LVGL tick — 提供给 lv_tick_inc
    static void LvglTickTask(void* arg);

    lv_disp_t* display_{nullptr};
    lv_disp_draw_buf_t draw_buf_{};
    esp_timer_handle_t tick_timer_{nullptr};
    lv_color_t* buf1_{nullptr};
    lv_color_t* buf2_{nullptr};

    int display_width_{240};
    int display_height_{320};

    static constexpr int kSpiClockHz = 40 * 1000 * 1000;  // 40MHz SPI
    static constexpr int kLvglTaskStackSize = 8192;
    static constexpr int kTickIntervalMs = 5;
};

#endif  // ROBOMIND_DISPLAY_DRIVER_H
