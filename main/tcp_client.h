/*
 * tcp_client.h —— 板端 TCP 数据上报（产品代码）
 *
 * 职责：把 producer 产出的最新一帧感知语义编码成 11 字段信封，经 TCP 推给 PC。
 * 只读上行，不做下行控制（见 Non-Goals）。
 *
 * 开关：CONFIG_ROBOMIND_TCP_CLIENT_ENABLED（默认 n）
 *   关闭时本文件不产生任何代码，任务不创建、栈不分配、静态段零增量。
 */
#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

/* 启动上报任务。producer 未启用时调用它没有意义（永远拿不到快照），但不会崩。 */
void tcp_client_start(void);

#endif /* TCP_CLIENT_H */
