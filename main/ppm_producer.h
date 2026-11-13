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

#define PPM_PRODUCER_SOURCE_MAX 20

/*
 * 生产者快照（单槽、覆盖式）。
 *
 * 为什么是单槽而不是队列：**过期的感知结果没有价值**。
 * 若用队列，网络卡顿时会积压一串陈旧帧，接收端看到的是"历史"而不是"现在"，
 * 且队列深度需要额外内存（产品态 free_internal 只剩 11 KB）。
 * 单槽的语义是"永远拿最新一帧"，正好匹配 70ms 周期的状态流。
 */
typedef struct {
    ppm_semantics_t sem;
    char            source[PPM_PRODUCER_SOURCE_MAX];
    uint32_t        gen;   /* 覆盖代数：调用方可据此判断"这帧我发过没有" */
} ppm_producer_snapshot_t;

/*
 * 取最近一帧快照。返回 false = 无可用数据（producer 未启用 / 尚未产出首帧）。
 * 内部用临界区保护整体拷贝，避免读到撕裂的中间态。
 */
bool ppm_producer_snapshot(ppm_producer_snapshot_t *out);

/*
 * 启动 PPM producer 任务。
 *
 * 必须在 RunLvglLoop() 之前调用（见 main.cpp 的插入点说明）。
 * 开关 CONFIG_ROBOMIND_PPM_PRODUCER_ENABLED 关闭时，本函数不会被调用，
 * 因此任务不创建、栈不分配、静态段零增量。
 */
void ppm_producer_start(void);

#endif /* PPM_PRODUCER_H */
