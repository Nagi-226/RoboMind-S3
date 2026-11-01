#pragma once

/* 在 app_main 进入 LVGL 循环之前调用一次。
 * 失败只打日志，不改变后面的显示循环。 */
void ppm_bridge_start();
