/*
 * wire_codec.cpp —— U03 编码器实现
 *
 * 规格：u02-u03-implementation-spec-2026-11-13.md §3.1–§3.7
 *
 * 三条硬性质（验收会逐条查）：
 *   1. **零堆分配**：本文件不含任何动态分配调用（规格 §3.9 判据 3 是一条机械 grep，
 *      所以这段注释也刻意不写出那三个保留字面量，免得把判据自己弄红）
 *   2. **零静态存储**：没有任何 static 缓冲/全局对象 ⇒ 不增加 .bss
 *      （U02-a 验收判据 4 要求 .dram0.bss 逐字节不变，本文件无条件编进镜像，
 *        所以这里一旦放静态缓冲就会破坏该判据）
 *   3. **不 include 任何 PPM 头**（规格 §3.9 判据 6）
 *
 * 失败语义：任何异常一律返回 0，调用方丢弃整帧。绝不部分写入。
 */
#include "wire_codec.h"

namespace {

/* 失败哨兵：用 size_t(-1) 与合法的 pos 区分（pos 永远 >= 0） */
constexpr size_t kFail = static_cast<size_t>(-1);

/* 状态流固定字段（规格 §3.6） */
constexpr const char *kStateTopic   = "ppm/state";
constexpr const char *kStateChannel = "telemetry";
constexpr const char *kStateQos     = "best_effort";
constexpr uint32_t    kStateTtlMs   = 300u;   /* = 周期 70ms × 4，容忍 3 次连续丢帧 */

/* 事件流固定字段（规格 §3.6） */
constexpr const char *kEventTopic   = "ppm/gesture";
constexpr const char *kEventChannel = "event";
constexpr const char *kEventQos     = "at_least_once";  /* 事件丢了就永久丢失 */
constexpr uint32_t    kEventTtlMs   = 3000u;

constexpr const char *kTarget        = "vvos";  /* 占位标签，但绝不写空 */
constexpr uint32_t    kSchemaVersion = 1u;      /* 不能是 0，`:202` 会拒收 */

/* ---- 基础追加原语：全部返回新 pos，越界返回 kFail ---- */

size_t append_char(char *out, size_t cap, size_t pos, char c)
{
    if (pos + 1u > cap) {
        return kFail;
    }
    out[pos] = c;
    return pos + 1u;
}

size_t append_sep(char *out, size_t cap, size_t pos)
{
    return append_char(out, cap, pos, '|');
}

/* 原样追加（用于我们自己控制的字面量：kv 的键名与 ';' '='） */
size_t append_raw(char *out, size_t cap, size_t pos, const char *s)
{
    while (*s != '\0') {
        if (pos + 1u > cap) {
            return kFail;
        }
        out[pos] = *s;
        ++pos;
        ++s;
    }
    return pos;
}

/*
 * 转义追加（规格 §3.3）：只有 '\' 与 '|' 需要前置一个反斜杠。
 * 不做 URL 编码、不做 %XX、不加引号 —— VVOS 的解码器只认反斜杠。
 */
size_t append_escaped(char *out, size_t cap, size_t pos, const char *s)
{
    while (*s != '\0') {
        const char c = *s;
        ++s;
        if (c == '\\' || c == '|') {
            if (pos + 1u > cap) {
                return kFail;
            }
            out[pos] = '\\';
            ++pos;
        }
        if (pos + 1u > cap) {
            return kFail;
        }
        out[pos] = c;
        ++pos;
    }
    return pos;
}

/* 十进制追加：不带 '+'、不带前导 0、不带空格（规格 §3.4 对 MCU 侧的要求） */
size_t append_u64(char *out, size_t cap, size_t pos, uint64_t v)
{
    char digits[21];          /* u64 最多 20 位十进制 */
    size_t n = 0;

    if (v == 0u) {
        digits[n] = '0';
        ++n;
    }
    while (v > 0u) {
        digits[n] = static_cast<char>('0' + static_cast<char>(v % 10u));
        ++n;
        v /= 10u;
    }
    /* digits 里是低位在前，倒序写出 */
    while (n > 0u) {
        --n;
        if (pos + 1u > cap) {
            return kFail;
        }
        out[pos] = digits[n];
        ++pos;
    }
    return pos;
}

/* 三态 → 字母（规格 §3.5：绝不用数字，结构性避免"0 表示未计算"） */
char valid_to_letter(uint8_t v)
{
    switch (v) {
        case PPM_SEM_VALID_NONE:    return 'N';
        case PPM_SEM_VALID_INVALID: return 'I';
        case PPM_SEM_VALID_OK:      return 'O';
        default:                    return '\0';   /* 契约外的值 ⇒ 调用方判失败 */
    }
}

/*
 * payload：8 个定序 key=value，';' 分隔（规格 §3.5）
 *   dv=V;dl=D;ev=V;es=E;mv=V;ms=M;ab=U;dg=G
 *
 * 两条设计要点：
 *   - 三态用字母，所以 es=0 永远只可能是"有效且睁眼"，读者不需要回头关联 ev。
 *   - validity != OK 时数值位强制写 '-'，把"未计算"与"值为 0"在语法上分开。
 */
size_t append_payload(char *out, size_t cap, size_t pos,
                      const struct ppm_semantics_t *s)
{
    const char dv = valid_to_letter(s->drowsy_valid);
    const char ev = valid_to_letter(s->eye_valid);
    const char mv = valid_to_letter(s->mouth_valid);
    if (dv == '\0' || ev == '\0' || mv == '\0') {
        return kFail;                       /* valid 字段越界 = 上游契约被违反 */
    }

    pos = append_raw(out, cap, pos, "dv=");
    if (pos == kFail) { return kFail; }
    pos = append_char(out, cap, pos, dv);
    if (pos == kFail) { return kFail; }

    pos = append_raw(out, cap, pos, ";dl=");
    if (pos == kFail) { return kFail; }
    if (s->drowsy_level > 3u) { return kFail; }   /* 语法只允许 0..3 */
    pos = append_char(out, cap, pos, static_cast<char>('0' + s->drowsy_level));
    if (pos == kFail) { return kFail; }

    pos = append_raw(out, cap, pos, ";ev=");
    if (pos == kFail) { return kFail; }
    pos = append_char(out, cap, pos, ev);
    if (pos == kFail) { return kFail; }

    pos = append_raw(out, cap, pos, ";es=");
    if (pos == kFail) { return kFail; }
    if (s->eye_valid == PPM_SEM_VALID_OK) {
        if (s->eye_state < 0 || s->eye_state > 2) { return kFail; }
        pos = append_char(out, cap, pos, static_cast<char>('0' + s->eye_state));
    } else {
        pos = append_char(out, cap, pos, '-');
    }
    if (pos == kFail) { return kFail; }

    pos = append_raw(out, cap, pos, ";mv=");
    if (pos == kFail) { return kFail; }
    pos = append_char(out, cap, pos, mv);
    if (pos == kFail) { return kFail; }

    pos = append_raw(out, cap, pos, ";ms=");
    if (pos == kFail) { return kFail; }
    if (s->mouth_valid == PPM_SEM_VALID_OK) {
        if (s->mouth_state < 0 || s->mouth_state > 1) { return kFail; }
        pos = append_char(out, cap, pos, static_cast<char>('0' + s->mouth_state));
    } else {
        pos = append_char(out, cap, pos, '-');
    }
    if (pos == kFail) { return kFail; }

    pos = append_raw(out, cap, pos, ";ab=");
    if (pos == kFail) { return kFail; }
    pos = append_u64(out, cap, pos, s->absent_100ms);
    if (pos == kFail) { return kFail; }

    pos = append_raw(out, cap, pos, ";dg=");
    if (pos == kFail) { return kFail; }
    pos = append_u64(out, cap, pos, s->degrade);
    return pos;
}

/*
 * 统一的流编码：11 字段，顺序照抄 VVOS Encode：
 *   schema_version|channel|qos|sequence|timestamp_ms|ttl_ms|source|target|topic|trace_id|payload
 * trace_id 恒为空（MVP 不做追踪）⇒ 写成零长度字段，即两个 '|' 相邻。
 */
size_t encode_stream(const struct ppm_semantics_t *s, const char *source,
                     const char *topic, const char *channel, const char *qos,
                     uint32_t ttl_ms, uint8_t *buf, size_t cap)
{
    if (s == nullptr || source == nullptr || buf == nullptr) {
        return 0u;
    }
    if (cap <= 4u) {
        return 0u;                       /* 连长度头都放不下 */
    }

    char *text = reinterpret_cast<char *>(buf) + 4;
    const size_t text_cap = cap - 4u;
    size_t pos = 0u;

    pos = append_u64(text, text_cap, pos, kSchemaVersion);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_escaped(text, text_cap, pos, channel);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_escaped(text, text_cap, pos, qos);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_u64(text, text_cap, pos, s->seq);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_u64(text, text_cap, pos, s->ts_ms);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_u64(text, text_cap, pos, ttl_ms);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_escaped(text, text_cap, pos, source);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_escaped(text, text_cap, pos, kTarget);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_escaped(text, text_cap, pos, topic);
    if (pos == kFail) { return 0u; }

    /* trace_id：零长度字段 —— 只写分隔符，不写内容 */
    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }

    pos = append_sep(text, text_cap, pos);
    if (pos == kFail) { return 0u; }
    pos = append_payload(text, text_cap, pos, s);
    if (pos == kFail) { return 0u; }

    /* 长度头：uint32 大端，值为文本字节数，不含头本身 */
    const uint32_t n = static_cast<uint32_t>(pos);
    buf[0] = static_cast<uint8_t>((n >> 24) & 0xFFu);
    buf[1] = static_cast<uint8_t>((n >> 16) & 0xFFu);
    buf[2] = static_cast<uint8_t>((n >> 8) & 0xFFu);
    buf[3] = static_cast<uint8_t>(n & 0xFFu);

    return static_cast<size_t>(n) + 4u;
}

}  // namespace

size_t wire_encode(const struct ppm_semantics_t *s, const char *source,
                   uint8_t *buf, size_t cap)
{
    return encode_stream(s, source, kStateTopic, kStateChannel, kStateQos,
                         kStateTtlMs, buf, cap);
}

size_t wire_encode_event(const struct ppm_semantics_t *s, const char *source,
                         uint8_t *buf, size_t cap)
{
    return encode_stream(s, source, kEventTopic, kEventChannel, kEventQos,
                         kEventTtlMs, buf, cap);
}
