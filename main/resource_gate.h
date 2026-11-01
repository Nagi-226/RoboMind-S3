#pragma once

#include "esp_err.h"

/* 摄像头门控的两个状态。CAM_OFF 表示模块不在场，不是故障。 */
enum rg_cam_state_t {
    RG_CAM_OFF = 0,
    RG_CAM_ON  = 1
};

/* 非本态调用返回 ESP_ERR_INVALID_STATE，不碰硬件。
 * 启动路径已经把摄像头拉起来，所以初态是 CAM_ON。 */
esp_err_t rg_release_camera();
esp_err_t rg_acquire_camera();
rg_cam_state_t rg_camera_state();
