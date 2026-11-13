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

#include "ppm_semantics.h"

/*
 * 启动 PPM producer 任务。
 *
 * 必须在 RunLvglLoop() 之前调用（见 main.cpp 的插入点说明）。
 * 开关 CONFIG_ROBOMIND_PPM_PRODUCER_ENABLED 关闭时，本函数不会被调用，
 * 因此任务不创建、栈不分配、静态段零增量。
 */
void ppm_producer_start(void);

/*
 * 读取最近一帧的语义快照（供 UI 显示感知状态）。
 *
 * 返回 false = **无可用数据**，两种情形：
 *   1. producer 开关关闭（任务从未创建）
 *   2. 任务已启动但还没产出第一帧
 *
 * ⚠️ UI 必须把 false 显示成"N/A / 未知"，**不能显示成"无人"** ——
 *    "没有数据"和"确认没人"是两件事，混淆会让关闭 producer 的构建误报"没人"。
 *
 * 线程安全：内部用临界区保护 24 字节结构体的整体拷贝，避免读到撕裂的中间态。
 */
bool ppm_producer_latest(ppm_semantics_t *out);

/*
 * UI 判断"无人"用的 degrade 位。与 PPM 的 PPM_DEGRADE_NO_FACE 同值（1<<0）。
 * 在头文件里再定义一份常量而不是让 UI include PPM 头 —— UI 不该依赖 PPM 的 ABI。
 * ppm_producer.cpp 里有 static_assert 保证它与 PPM 的枚举一致。
 */
#define PPM_PRODUCER_DEGRADE_NO_FACE (1u << 0)

#endif /* PPM_PRODUCER_H */
