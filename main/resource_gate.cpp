/*
 * resource_gate.cpp — 在 CAM_ON 与 CAM_OFF 之间切换摄像头。
 *
 * 只做这一件事：聊天要内部堆时把摄像头让出去，用完再按已验证的上电序列拉回来。
 * 不复制 I2C 初始化。XL9555 仍走显示驱动里那一次。
 * 状态不对就拒绝，避免在 cam_obj 已经清空之后再退一次。
 */

#include "resource_gate.h"

#include "camera_budget.h"
#include "display_driver.h"

#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

rg_cam_state_t g_state = RG_CAM_ON;

}  // namespace

esp_err_t rg_release_camera()
{
    if (g_state != RG_CAM_ON) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t err = esp_camera_deinit();
    if (err == ESP_OK) {
        g_state = RG_CAM_OFF;
    }
    return err;
}

esp_err_t rg_acquire_camera()
{
    if (g_state != RG_CAM_OFF) {
        return ESP_ERR_INVALID_STATE;
    }

    auto *display = DisplayDriver::GetInstance();
    if (!display->EnsureXl9555()) {
        return ESP_FAIL;
    }
    /* PWDN 低有效，高电平参数用 false。RESET 先拉低再放开。 */
    if (!display->SetCameraPins(false, false)) {
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    if (!display->SetCameraPins(false, true)) {
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    const esp_err_t err = camera_budget_init();
    if (err == ESP_OK) {
        g_state = RG_CAM_ON;
    }
    return err;
}

rg_cam_state_t rg_camera_state()
{
    return g_state;
}
