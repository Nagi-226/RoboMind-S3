/*
 * wire_codec.h —— U03：MCU 侧 wire 编码器（11 字段信封 + [u32 BE 长度] 帧）
 *
 * 规格依据：u02-u03-implementation-spec-2026-11-13.md §3.1 / §3.2 / §3.3 / §3.5 / §3.7
 *
 * 帧格式（一次发送的全部字节）：
 *   offset 0..3    : uint32 大端，值 = 后续文本的字节数（不含这 4 字节）
 *   offset 4..N+3  : 文本载荷（ASCII），无结尾 NUL、无 \r\n
 *
 * 本文件**不 include 任何 PPM 头**（规格 §3.9 判据 6）：
 * 它只依赖 ppm_semantics.h（零依赖）与标准 C 库。
 * 这是"U02 与 U03 解耦"的机械判据 —— 编码器不知道 PPM 存在。
 */
#ifndef WIRE_CODEC_H
#define WIRE_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "ppm_semantics.h"

/*
 * 行缓冲上限。规格 §3.7 的预算：
 *   典型 112 B 文本 + 4 B 头 = 116 B
 *   降级饱和 108 B + 4 = 112 B
 *   理论最坏 ≈124 B + 4 = 128 B
 * ⇒ 320 B 在最坏情形下仍有 2.5 倍余量。
 * 缓冲由**调用方**提供（放任务栈上），本文件零静态存储。
 */
#define kWireLineMax 320

/*
 * 编码器：状态流（topic=ppm/state, channel=telemetry, qos=best_effort, ttl_ms=300）
 *
 * 返回值：写入的总字节数（含 4 B 长度头）。
 *         返回 0 = 失败（参数非法 / 缓冲不足 / 字段越界），**调用方必须丢弃整帧**，
 *         不允许发送半截数据（规格 §3.9 判据 4）。
 *
 * ⚠️ 与规格 §3.7 签名的**一处必要偏差**（已上报主理人，见交付报告）：
 *    规格写的是 wire_encode(s, buf, cap)，但 source（"ppm-<MAC2>-<boot4>"）必须每次
 *    启动不同、且每块板不同 ⇒ 它只能由调用方传入。若改成函数内静态缓冲，
 *    wire_codec.cpp 会引入 .bss，直接违反 U02-a 验收判据 4（.drm0.bss 逐字节不变）。
 *    因此这里多一个 `source` 参数：**零静态存储 + 可测试**，两个判据都能满足。
 *
 * source 约束（规格 §3.2）：形如 "ppm-a3f1-7b2c"，非空，含 `-` 与 hex 字符。
 * 本函数不做格式校验（那是 producer 的职责），但会对 `|` 与 `\` 做转义。
 */
size_t wire_encode(const struct ppm_semantics_t *s, const char *source,
                   uint8_t *buf, size_t cap);

/*
 * 编码器：一次性事件流（topic=ppm/gesture, channel=event, qos=at_least_once, ttl_ms=3000）
 *
 * 为什么要和状态流分开（规格 §3.6）：
 *   判重窗口按 topic 分桶，每个桶只留最近 128 条 key。
 *   若事件与状态共用 ppm/state 桶，事件会被每秒十几条的状态帧挤出窗口。
 *   qos 也必须更强：状态丢了下一帧补上，事件丢了就永久丢失。
 */
size_t wire_encode_event(const struct ppm_semantics_t *s, const char *source,
                         uint8_t *buf, size_t cap);

#endif /* WIRE_CODEC_H */
