/*
 * ppm_semantics.h —— U02 与 U03 之间的唯一接缝（单一真源）
 *
 * 为什么单独一个文件、且零依赖：
 *   U02（ppm_producer.cpp）负责**填**这个结构；
 *   U03（wire_codec.cpp）负责**读**这个结构。
 *   如果结构体定义放在 ppm_producer.h 里，wire_codec 就得 include producer 的头，
 *   于是 codec 依赖 producer（方向错误的依赖），还会把 PPM 头传递进来。
 *   单独一个零依赖头，两边都 include 同一个定义，且 wire_codec 可以完全不碰 PPM。
 *
 * 纪律（违反会出事，且编译器不会报错）：
 *   这个结构体**只能有一份定义**。若有人在别处内联复制一份字段顺序不同的定义，
 *   两份都是合法 POD ⇒ 编译通过 ⇒ 运行期字段错位 ⇒ 症状是"值是别的字段的值"，
 *   而不是崩溃。下面的 static_assert 就是防这个的机械保证。
 *
 * 禁止事项（见规格 §2.6.1）：
 *   - 禁止 memcpy 传输本结构
 *   - 禁止用 sizeof(本结构) 做序列化长度
 *   - 禁止把本结构当协议体直接发二进制
 *   原因：偏移 9..11 那 3 字节是**填充，内容未定义**。传输一律走文本 wire（wire_codec）。
 */
#ifndef PPM_SEMANTICS_H
#define PPM_SEMANTICS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define PPM_SEM_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define PPM_SEM_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

/*
 * 三态有效性取值 —— 这是 **wire 契约**（不是 PPM 的枚举副本）。
 *
 * 规格 §3.5 要求三态在 wire 上只用字母 N/I/O，绝不用数字，
 * 免得出现"某个数字表示未计算"的歧义（PPM 红线 2）。
 * 这里定义的是**内存里**的表示，编码器负责映射到字母。
 *
 * ppm_producer.cpp 里有 static_assert 保证这几个值与 PPM 的
 * ppm_valid_t 一致；若 PPM 改了枚举，编译期就会红。
 */
enum ppm_sem_valid {
    PPM_SEM_VALID_NONE    = 0,  /* 未计算 / 无该能力 */
    PPM_SEM_VALID_INVALID = 1,  /* 算了但结果不可信 */
    PPM_SEM_VALID_OK      = 2   /* 有效 */
};

/* eye_state / mouth_state 在 valid != OK 时统一填这个哨兵值（wire 上编码为 '-'） */
#define PPM_SEM_STATE_UNKNOWN (-1)

struct ppm_semantics_t {
    uint8_t  drowsy_valid;   /* ppm_sem_valid：0=NONE 1=INVALID 2=OK */
    uint8_t  drowsy_level;   /* 0..3 */
    uint8_t  eye_valid;      /*ppm_sem_valid*/
    int8_t   eye_state;      /* 0=open 1=closed 2=uncertain；valid!=OK 时为 -1 */
    uint8_t  mouth_valid;    /* ppm_sem_valid */
    int8_t   mouth_state;    /* 0=closed 1=open；valid!=OK 时为 -1 */
    uint16_t absent_100ms;   /* PPM 自己饱和在 65535 */
    uint8_t  degrade;        /* ppm_degrade_t 位或，0..255 */
    uint32_t seq;            /* 板侧帧序号，每次启动从 0 开始 */
    uint64_t ts_ms;          /* 板侧开机毫秒（esp_timer_get_time()/1000），非墙钟 */
};

/*
 * 布局契约（精确推算，非手估）：
 *   offset  0  drowsy_valid   1
 *   offset  1  drowsy_level   1
 *   offset  2  eye_valid      1
 *   offset  3  eye_state      1
 *   offset  4  mouth_valid    1
 *   offset  5  mouth_state    1
 *   offset  6  absent_100ms   2
 *   offset  8  degrade        1
 *   offset  9..11  padding    3   （内容未定义，禁止按字节复制结构体）
 *   offset 12  seq            4
 *   offset 16  ts_ms          8
 *   ------------------------------
 *   合计                      24（对齐 8）
 *
 * 对齐不变性：即使某个 ABI 把 uint64_t 对齐从 8 降为 4，上表偏移与 sizeof 仍然不变
 * （seq 恰好结束于 16，ts_ms 按 4 或 8 对齐都落 16）。所以这组断言在
 * xtensa(ILP32) 与 x86-64(LP64) 上同时成立，可直接当跨端契约。
 */
PPM_SEM_ASSERT(sizeof(struct ppm_semantics_t) == 24,
               "ppm_semantics_t must be 24 bytes");

PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, drowsy_valid) ==  0, "layout: drowsy_valid");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, drowsy_level) ==  1, "layout: drowsy_level");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, eye_valid)    ==  2, "layout: eye_valid");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, eye_state)    ==  3, "layout: eye_state");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, mouth_valid)  ==  4, "layout: mouth_valid");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, mouth_state)  ==  5, "layout: mouth_state");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, absent_100ms) ==  6, "layout: absent_100ms");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, degrade)      ==  8, "layout: degrade");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, seq)          == 12, "layout: seq");
PPM_SEM_ASSERT(offsetof(struct ppm_semantics_t, ts_ms)        == 16, "layout: ts_ms");

#endif /* PPM_SEMANTICS_H */
