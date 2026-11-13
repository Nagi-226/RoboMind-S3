/*
 * ppm_producer.h —— U02：PPM 语义生产者（替代 ppm_bridge）
 *
 * 规格：u02-u03-implementation-spec-2026-11-13.md §2.6 / §2.8 / §2.9
 *
 * 本单（U02-a）只做**接线**：让 producer 真的能被创建、能进主循环、能打印标识。
 * 取样（ppm_init / ppm_run / 栈水位 / 调 wire_encode）属于 U02-b。
 *
 * 为什么 U02-a 也要单独一步：
 *   原 ppm_bridge_start() 全仓库无调用点 ⇒ 代码存在但从未执行。
 *   如果 producer 也照此办理，就是"看起来交付了、实际零验证"。
 *   所以 U02-a 的验收核心是"开关打开后，串口真的能看到它跑起来"。
 */
#ifndef PPM_PRODUCER_H
#define PPM_PRODUCER_H

/*
 * 启动 PPM producer 任务。
 *
 * 必须在 RunLvglLoop() 之前调用（见 main.cpp 的插入点说明）。
 * 开关 CONFIG_ROBOMIND_PPM_PRODUCER_ENABLED 关闭时，本函数不会被调用，
 * 因此任务不创建、栈不分配、静态段零增量。
 */
void ppm_producer_start(void);

#endif /* PPM_PRODUCER_H */
