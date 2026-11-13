/*
 * 只回答「摄像头的内部堆块能不能装进已经连着 WiFi 的 RoboMind」。
 * 不调用 esp_camera_fb_get，也不把像素交给 PPM。
 */

#include "camera_budget.h"

#include "display_driver.h"

#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t camera_budget_init()
{
    auto* display = DisplayDriver::GetInstance();
    if (!display->EnsureXl9555()) {
        return ESP_ERR_INVALID_STATE;
    }

    /* PWDN 低有效，所以高电平参数是 false。RESET 低有效脉冲：先拉低，再放开。 */
    if (!display->SetCameraPins(false, false)) {
        return ESP_ERR_INVALID_STATE;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    if (!display->SetCameraPins(false, true)) {
        return ESP_ERR_INVALID_STATE;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    camera_config_t cfg = {};
    cfg.pin_pwdn = -1;
    cfg.pin_reset = -1;
    cfg.pin_xclk = -1;
    cfg.pin_sccb_sda = 39;
    cfg.pin_sccb_scl = 38;
    cfg.pin_d7 = 18;
    cfg.pin_d6 = 17;
    cfg.pin_d5 = 16;
    cfg.pin_d4 = 15;
    cfg.pin_d3 = 7;
    cfg.pin_d2 = 6;
    cfg.pin_d1 = 5;
    cfg.pin_d0 = 4;
    cfg.pin_vsync = 47;
    cfg.pin_href = 48;
    cfg.pin_pclk = 45;
    cfg.xclk_freq_hz = 24000000;
    cfg.ledc_timer = LEDC_TIMER_0;
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.pixel_format = PIXFORMAT_GRAYSCALE;
    cfg.frame_size = FRAMESIZE_QVGA;
    cfg.jpeg_quality = 12;
    cfg.fb_count = 2;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode = CAMERA_GRAB_LATEST;
    cfg.sccb_i2c_port = -1;

    return esp_camera_init(&cfg);
}
