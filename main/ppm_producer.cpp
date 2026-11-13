/*
 * ppm_producer.cpp —— U02：PPM 语义生产者
 *
 * 规格：u02-u03-implementation-spec-2026-11-13.md §2.5 / §2.6 / §2.8
 *
 * 两个阶段：
 *   U02-a（已完成）：接线 —— 任务能被创建、能进主循环、能打印标识
 *   U02-b（本文件现状）：接真管线 —— ppm_init / ppm_run / 栈水位 / wire_encode / wire_send
 *
 * 退役说明：本文件替代 main/ppm_bridge.cpp。
 *   原 ppm_bridge 里有 4 处重复的 ppmi_decision_step / degrade 或运算 /
 *   prev_absent_100ms 回写，全部删除 —— 那些 ppm_run 内部已经做过了
 *   （ppm_api.cpp:99/106/113），重复调用只是因为对同一 ts 命中 is_repeat 而无副作用。
 *   所以删掉它们**不改变输出**，是去耦合，不是修 bug。
 */
#include "ppm_producer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"

#include "ppm.h"                    /* 公共 C ABI */
#include "ppm_mock.h"               /* mock 是当前生效后端，必需 */
#include "ppm_profile_generated.h"  /* profile 注入（构建期生成），必需 */

#include "wire_codec.h"             /* U03 的出口 */
#include "ppm_semantics.h"          /* U02/U03 的唯一接缝 */

/*
 * wire_send：U04 才提供真实 TCP 发送。本单先接一个"打到串口 hex"的桩，
 * 让 U03 的字节可以在板上被逐字节核对（规格 §4.1.3）。
 *
 * 注意这里**不加** CONFIG_ 条件编译：producer_task 无条件编译并引用它，
 * 若把 wire_send 包进 #if，开关关闭时就会变成"未定义符号"而编译失败。
 * 开关只包住 main.cpp 里的**调用点**，不包住源文件（与规格 §4.1.2 一致）。
 */
namespace {

bool wire_send(const uint8_t *buf, size_t n)
{
    std::printf("[wire] len=%u hex=", static_cast<unsigned>(n));
    /* 分块打印，避免为整帧 hex 申请大栈缓冲 */
    char chunk[16 * 2 + 1];
    for (size_t i = 0; i < n; i += 16) {
        const size_t cnt = (n - i < 16) ? (n - i) : 16;
        for (size_t k = 0; k < cnt; ++k) {
            static const char *kHex = "0123456789abcdef";
            chunk[k * 2]     = kHex[(buf[i + k] >> 4) & 0x0F];
            chunk[k * 2 + 1] = kHex[buf[i + k] & 0x0F];
        }
        chunk[cnt * 2] = '\0';
        std::printf("%s", chunk);
    }
    std::printf("\n");
    return true;
}

}  // namespace

namespace {

/* 与规格 §2.5 一致：6,144 B。升级阈值 hw < 1,024 ⇒ 下一单改 8,192 */
constexpr unsigned kStackBytes = 6144;
constexpr UBaseType_t kPriority = 5;

/* 帧周期 70 ms（与 R3g / ppm_bridge 同节拍，保持 [mem] 可比） */
constexpr uint32_t kPeriodMs = 70;

/*
 * 链接锚点（**不要删**，理由见下）
 *
 * 为什么需要它：libppm.a 的成员是**按需拉入**的 —— 没有任何符号引用时，
 * 链接器不会把 libppm.a 的任何 .obj 放进镜像。ppm_bridge.cpp 曾是全仓库
 * 唯一引用 libppm.a 的文件，U02 删掉它之后，如果这里不留引用，
 * PPM 的 46,911 B 静态池（g_pool 30,688 + g_instance 10,704 + s_doc 5,136）
 * 会**从镜像里静默消失**。
 *
 * 后果为什么危险：.dram0.bss 会突然"变小很好看"，内存账本看起来改善了，
 * 但那是假的 —— 池子被裁掉以后，ppm_init 一跑就会踩到不存在的内存。
 * 而且这个失效**在运行时也看不出**，直到真正调用 PPM 才炸，中间可能隔一个开发周期。
 *
 * 用 volatile 是为了在 --gc-sections 下也不被优化掉；
 * 只取地址，**永不解引用、永不调用**。
 */
void *const volatile g_ppm_link_anchor[] = {
    reinterpret_cast<void *>(&ppm_init),
    reinterpret_cast<void *>(&ppm_run),
    reinterpret_cast<void *>(&ppm_reset),
    reinterpret_cast<void *>(&ppm_destroy),
    reinterpret_cast<void *>(&ppm_capabilities),
    reinterpret_cast<void *>(&ppm_version),
    reinterpret_cast<void *>(&ppmi_mock_begin_frame),
};

/*
 * 生成 source = "ppm-<MAC 末2字节>-<boot 4hex>"（规格 §3.2）
 *
 * boot 标识取法（规格定案，不要改成毫秒）：
 *   (uint16_t)(esp_timer_get_time() ^ (int64_t)esp_random())
 * 微秒相位是主承重（不依赖熵/WiFi/插入点）；esp_random 提供去相关。
 * 用毫秒的话，低 16 位回绕 65.5 s，而启动到这里的耗时稳定在几十毫秒 ⇒ 两次启动系统性接近。
 */
void make_source(char *out, size_t cap)
{
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    (void)esp_read_mac(mac, ESP_MAC_WIFI_STA);

    const uint16_t boot_id =
        static_cast<uint16_t>(esp_timer_get_time() ^ static_cast<int64_t>(esp_random()));

    std::snprintf(out, cap, "ppm-%02x%02x-%04x",
                  static_cast<unsigned>(mac[4]),
                  static_cast<unsigned>(mac[5]),
                  static_cast<unsigned>(boot_id));
}

/*
 * 把 ppm_result_t 的一个目标映射成 ppm_semantics_t（规格 §2.6）。
 * validity != OK 时状态位填 PPM_SEM_STATE_UNKNOWN(-1)，wire 上编码成 '-'。
 */
void fill_semantics(const ppm_target_t *t, const ppm_result_t *r,
                    uint32_t seq, uint64_t ts_ms, ppm_semantics_t *s)
{
    s->drowsy_valid = t->drowsy_valid;
    s->drowsy_level = t->drowsy_level;

    s->eye_valid = t->eye_state_valid;
    s->eye_state = (t->eye_state_valid == PPM_VALID_OK)
                       ? static_cast<int8_t>(t->eye_state)
                       : static_cast<int8_t>(PPM_SEM_STATE_UNKNOWN);

    s->mouth_valid = t->mouth_state_valid;
    s->mouth_state = (t->mouth_state_valid == PPM_VALID_OK)
                         ? static_cast<int8_t>(t->mouth_state)
                         : static_cast<int8_t>(PPM_SEM_STATE_UNKNOWN);

    s->absent_100ms = t->absent_100ms;
    s->degrade      = r->degrade;
    s->seq          = seq;
    s->ts_ms        = ts_ms;
}

#if CONFIG_ROBOMIND_PPM_MOCK_FIXTURE
/*
 * 发布态门禁：夹具**绝不允许**进入发布构建。
 * 合成语义进产品路径是数据正确性风险，比崩溃更隐蔽 —— 不会报错，只是数据是假的。
 */
#if defined(NDEBUG)
#error "CONFIG_ROBOMIND_PPM_MOCK_FIXTURE 不得用于发布构建（NDEBUG 已定义）。请置 n。"
#endif

/*
 * 构造一帧 mock 夹具语义。
 *
 * 🔴 **合成值，不是真实感知。**
 *    存在理由：让 target_count>0，从而走通"有目标"那条编码分支。
 *    （无目标分支不需要夹具 —— 默认 nullptr 就会走那条路，且那条路现在也会发帧。）
 *
 * 🔴 **在夹具存在期间，任何 wire 数据的语义都不可用于产品决策。**
 *    本夹具下产出的 dl（疲劳等级）、es（睁闭眼）都是**脚本生成的**，
 *    不是从摄像头像素推出来的。可以用它验证链路、验证编码、验证决策状态机在跑，
 *    **但不能用它判断"这个人累不累"**。
 *
 * 取值设计（都为了"让下游真的有东西可看"，不是为了伪造感知精度）：
 *   - has_face=1：产生一个目标
 *   - eye 每 50 帧（3.5 s）在 睁/闭 间切换：既覆盖 es=0 与 es=1 两种编码，
 *     也让 drowsy 决策层真的累积时间窗（否则永远 level=0，看不出状态机在跑）
 *   - mouth=0 恒定：本单不验张嘴
 *   - quality=1.0：不触发降级
 *
 * 真后端接入（P2）时整个函数与 ppmi_mock_begin_frame 一起删除。
 */
void build_mock_frame(uint32_t seq, ppmi_mock_frame_t *mf)
{
    std::memset(mf, 0, sizeof(*mf));
    mf->has_face  = 1u;
    mf->has_eye   = 1u;
    mf->eye       = static_cast<uint8_t>(((seq / 50u) % 2u) == 0u ? 0u : 1u);
    mf->has_mouth = 1u;
    mf->mouth     = 0u;
    mf->quality   = 1.0f;
}
#endif /* CONFIG_ROBOMIND_PPM_MOCK_FIXTURE */

void producer_task(void *arg)
{
    (void)arg;

    /*
     * source 在**任务自己的栈**上算 —— 刻意不用 static。
     * static 会进 .bss，而 U02-a 验收要求"新增 TU 的 .bss/.data 贡献 = 0"。
     */
    char source[20];
    make_source(source, sizeof(source));

    std::printf("[producer] started seq=0 src=%s\n", source);
    std::fflush(stdout);

    /* ---------- 1. 初始化 PPM ---------- */
    ppm_ctx_t *ctx = nullptr;
    const ppm_status_t init_st =
        ppm_init(reinterpret_cast<const uint8_t *>(k_profile),
                 sizeof(k_profile) - 1u,
                 sizeof(ppm_result_t),
                 &ctx);
    if (init_st != PPM_OK || ctx == nullptr) {
        /* 只有公开状态码可用（不再有 ppmi_config_last_error 的字符串，属于可接受的信息损失） */
        std::printf("[producer] ppm_init FAIL status=%d\n", static_cast<int>(init_st));
        std::fflush(stdout);
        vTaskDelete(nullptr);
        return;
    }

    /* ABI 自检：ppm_version 提供的信息比原来只打 sizeof 更多 */
    uint16_t abi_major = 0;
    uint16_t abi_minor = 0;
    size_t   result_size = 0;
    const char *ver = ppm_version(&abi_major, &abi_minor, &result_size);
    ppm_mask_t caps = {0};
    (void)ppm_capabilities(ctx, &caps);
    std::printf("[producer] init=OK ver=%s abi=%u.%u result_size=%u caps=0x%08x\n",
                (ver != nullptr) ? ver : "?",
                static_cast<unsigned>(abi_major),
                static_cast<unsigned>(abi_minor),
                static_cast<unsigned>(result_size),
                static_cast<unsigned>(caps.bits));
    std::fflush(stdout);

    /* ---------- 2. 取样主循环 ---------- */
    const TickType_t period = pdMS_TO_TICKS(kPeriodMs);
    TickType_t last_wake = xTaskGetTickCount();

    uint32_t seq = 0;
    uint32_t ok = 0;
    uint32_t run_fail = 0;
    uint32_t encode_fail = 0;
    uint32_t fb_null = 0;
    uint32_t no_target = 0;   /* 无目标帧数 —— 这些帧**会发出去**，不是被跳过 */
    uint32_t with_target = 0;

    for (;;) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == nullptr) {
            ++fb_null;
            vTaskDelayUntil(&last_wake, period);
            continue;
        }

        ppm_frame_t frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.data   = fb->buf;
        frame.size   = fb->len;
        frame.width  = static_cast<uint16_t>(fb->width);
        frame.height = static_cast<uint16_t>(fb->height);
        frame.stride = static_cast<uint16_t>(fb->width);
        frame.pixfmt = PPM_FMT_GRAY8;
        frame.ts_ms  = static_cast<uint64_t>(esp_timer_get_time() / 1000);
        frame.seq    = seq;

        /*
         * 必须调用：mock 是当前生效后端。未调用 begin_frame 时 frame_ready()
         * 返回 -1（ppm_mock.cpp:125-128），9 个模块全失败 ⇒ ppm_run 折叠成 PPM_E_INTERNAL。
         * 「从不调用」≠「调用时传 NULL」—— 传 NULL 等价于全部缺省，合法。
         *
         * 默认（不注入夹具）传 nullptr ⇒ has_face=0 ⇒ target_count=0。
         * 这正是**真实场景里"镜头前没人"的那条路径**，必须真的发出帧（见下方无目标分支）。
         *
         * 可选夹具（CONFIG_ROBOMIND_PPM_MOCK_FIXTURE，默认 n）用于让 target_count>0，
         * 走通"有目标"那条编码分支。
         */
#if CONFIG_ROBOMIND_PPM_MOCK_FIXTURE
        ppmi_mock_frame_t mf;
        build_mock_frame(seq, &mf);
        ppmi_mock_begin_frame(&mf);
#else
        /* 默认构建：不注入任何合成语义，走"全部缺省"（has_face=0 ⇒ 无目标） */
        ppmi_mock_begin_frame(nullptr);
#endif

        ppm_result_t result;
        std::memset(&result, 0, sizeof(result));
        const ppm_status_t st = ppm_run(ctx, &frame, &result);
        esp_camera_fb_return(fb);

        if (st != PPM_OK) {
            ++run_fail;
        } else {
            ppm_semantics_t s;
            std::memset(&s, 0, sizeof(s));   /* 全 0 = 三态全 NONE、状态位 -1 的来源见下 */

            if (result.target_count > 0) {
                /* targets[0] 只在 target_count>0 时有效 —— 唯一一个"省了就 UB"的判空 */
                fill_semantics(&result.targets[0], &result, seq, frame.ts_ms, &s);
                ++with_target;
            } else {
                /*
                 * 🔴 无目标帧**也必须发**（产品缺陷修复，2026-11-13）
                 *
                 * 旧行为：target_count==0 就 ++no_target 然后跳过编码 ⇒ 一帧都不发。
                 * 后果不是"验不了"，是**功能错误**：真实场景里镜头前没人时，
                 * 接收侧永远收不到"当前无人"的信号，absent_100ms 在接收侧永远停在陈旧值，
                 * 人走了看不出来。而且 ppm_run 返回 PPM_OK、run_fail=0，
                 * **没有任何指标会报异常** —— 静默断流。
                 *
                 * 正确语义（这正是"禁止用 0 表示未计算"那条红线存在的理由）：
                 *   dv=N ev=N mv=N  ⇒ 本帧这三项**未计算**（不是"算出来是 0"）
                 *   es=- ms=-       ⇒ valid≠OK 时状态位必须写 '-'
                 *   dl=0            ⇒ dl 的语法是 D := "0"|"1"|"2"|"3"，**没有 '-' 选项**；
                 *                      读者靠 dv=N 就知道 dl 无意义，不需要 dl 也带 '-'。
                 *   dg              ⇒ 取自 result.degrade，正常情况下含 PPM_DEGRADE_NO_FACE
                 *
                 * ⚠️ 已知 ABI 限制：absent_100ms 在公开 ABI 里只存在于
                 *    ppm_target_t（每目标），target_count==0 时**根本读不到**。
                 *    所以这里只能填 0，并如实记录 —— 接收侧要靠"帧间隔 + dg 的 NO_FACE 位"
                 *    自行推算离开时长，不能依赖 ab。要修这一点需要 PPM 在 ppm_result_t
                 *    顶层暴露全局 absent，属于 PPM 侧 ABI 变更，不在本单范围。
                 */
                s.degrade      = result.degrade;
                s.seq          = seq;
                s.ts_ms        = frame.ts_ms;
                s.absent_100ms = 0;
                ++no_target;   /* 语义已改为"无目标帧数"（这些帧会真的发出去） */
            }

            /*
             * 编码缓冲放**任务栈**（规格 §3.7）：静态段零增量。
             * 320 B 对 6,144 的栈预算可忽略，已计入余量。
             */
            uint8_t line[kWireLineMax];
            const size_t n = wire_encode(&s, source, line, sizeof(line));
            if (n == 0u) {
                ++encode_fail;   /* 缓冲不足/字段越界 ⇒ 丢弃整帧，绝不发半截 */
            } else {
                (void)wire_send(line, n);
                ++ok;
            }
        }

        ++seq;

        /* 每 100 帧打一次栈水位与统计（这是栈水位第一次真正可测） */
        if ((seq % 100u) == 0u) {
            const UBaseType_t hw = uxTaskGetStackHighWaterMark(nullptr);
            std::printf("[producer] frames=%u ok=%u run_fail=%u encode_fail=%u "
                        "fb_null=%u no_target=%u with_target=%u\n",
                        static_cast<unsigned>(seq), static_cast<unsigned>(ok),
                        static_cast<unsigned>(run_fail), static_cast<unsigned>(encode_fail),
                        static_cast<unsigned>(fb_null), static_cast<unsigned>(no_target),
                        static_cast<unsigned>(with_target));
            std::printf("[producer] stack high_water=%uB free of %uB peak=%uB\n",
                        static_cast<unsigned>(hw), kStackBytes,
                        static_cast<unsigned>(kStackBytes - hw));
            std::fflush(stdout);
        }

        vTaskDelayUntil(&last_wake, period);
    }
}

}  // namespace

void ppm_producer_start(void)
{
    if (g_ppm_link_anchor[0] == nullptr) {
        std::printf("[producer] link anchor missing\n");
        std::fflush(stdout);
        return;
    }

    /* arg 传 nullptr：source 由任务在自己栈上生成，避免任何静态存储 */
    const BaseType_t ok =
        xTaskCreate(producer_task, "ppm_producer", kStackBytes, nullptr, kPriority, nullptr);
    if (ok != pdPASS) {
        std::printf("[producer] task create failed\n");
        std::fflush(stdout);
    }
}
