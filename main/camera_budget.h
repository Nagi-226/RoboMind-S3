#pragma once

#include "esp_err.h"

/* 只做 esp_camera_init，不取帧。引脚与上电序列跟已验证的那一次一致，不改极性。 */
esp_err_t camera_budget_init();
