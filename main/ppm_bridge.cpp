/*
 * ppm_bridge.cpp — 里程碑 1：在独立任务里跑 100 帧 ppm_run。
 *
 * 不挂 LVGL 循环，不改显示驱动。帧缓冲只向 PSRAM 要一块，失败就退出任务。
 * 像素是固定灰阶，不往槽里填人脸或闭眼。mock 不读像素，所以这里仍要
 * 交一帧「全部缺省」的脚本，否则模块会把 ppm_run 打成失败。
 */

#include "ppm_bridge.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "lvgl.h"

#include "ppm.h"
#include "ppm_internal.h"
#include "ppm_core.h"
#include "ppm_mock.h"
#include "ppm_profile_generated.h"

/* 只在 lv_timer_handler() 里递增。创建放在 ppm_bridge_start()，
 * 那时主循环还没开始，不会和 lv_timer_handler 抢链表。 */
static volatile uint32_t g_lv_fires = 0;

static void lv_probe_cb(lv_timer_t *t)
{
    (void)t;
    g_lv_fires += 1;
}

namespace {

constexpr int      kFrames       = 100;
constexpr int      kPeriodMs     = 70;   /* 7 tick @ 100 Hz。66 会截成 6 tick = 60 ms */
constexpr int      kFailStop     = 10;
constexpr size_t   kFrameBytes   = 76800;
constexpr unsigned kStackBytes   = 6144;
constexpr UBaseType_t kPriority  = 1;

struct HeapSample {
    uint32_t free_bytes;
    uint32_t largest;
    uint32_t min_free;
};

HeapSample sample_heap()
{
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    HeapSample s;
    s.free_bytes = heap_caps_get_free_size(caps);
    s.largest    = heap_caps_get_largest_free_block(caps);
    s.min_free   = heap_caps_get_minimum_free_size(caps);
    return s;
}

void print_heap(const char *tag, const HeapSample &s)
{
    std::printf("[heap] %s free=%lu  largest=%lu  min_free=%lu\n",
                tag,
                static_cast<unsigned long>(s.free_bytes),
                static_cast<unsigned long>(s.largest),
                static_cast<unsigned long>(s.min_free));
    std::fflush(stdout);
}

uint8_t ramp_byte(int x)
{
    uint32_t v = (static_cast<uint32_t>(x) * 255u) / 239u;
    if (v > 255u) {
        v = 255u;
    }
    return static_cast<uint8_t>(v);
}

void paint_ramp(uint8_t *pixels, int width, int height, int stride)
{
    for (int y = 0; y < height; ++y) {
        uint8_t *row = pixels + static_cast<size_t>(y) * static_cast<size_t>(stride);
        for (int x = 0; x < width; ++x) {
            row[x] = ramp_byte(x);
        }
    }
}

void paint_column(uint8_t *pixels, int height, int stride, int col, uint8_t value)
{
    for (int y = 0; y < height; ++y) {
        pixels[static_cast<size_t>(y) * static_cast<size_t>(stride) + static_cast<size_t>(col)] = value;
    }
}

void ppm_task(void *)
{
    std::printf("[ppm ] abi result=%u target=%u slots=%u frame=%u\n",
                static_cast<unsigned>(sizeof(ppm_result_t)),
                static_cast<unsigned>(sizeof(ppm_target_t)),
                static_cast<unsigned>(sizeof(ppm_slots_t)),
                static_cast<unsigned>(sizeof(ppm_frame_t)));
    std::fflush(stdout);

    uint8_t *pixels = static_cast<uint8_t *>(
        heap_caps_malloc(kFrameBytes, MALLOC_CAP_SPIRAM));
    if (pixels == nullptr) {
        std::printf("[ppm ] frame buffer alloc failed bytes=%u caps=SPIRAM; task exit, LVGL continues\n",
                    static_cast<unsigned>(kFrameBytes));
        std::fflush(stdout);
        vTaskDelete(nullptr);
        return;
    }

    const HeapSample before = sample_heap();
    print_heap("before_init", before);

    ppm_ctx_t *ctx = nullptr;
    const ppm_status_t init_st = ppm_init(
        reinterpret_cast<const uint8_t *>(k_profile),
        sizeof(k_profile) - 1u,
        sizeof(ppm_result_t),
        &ctx);
    if (init_st != PPM_OK || ctx == nullptr) {
        std::printf("[ppm ] init=FAIL status=%d reason=%s\n",
                    static_cast<int>(init_st), ppmi_config_last_error());
        std::fflush(stdout);
        heap_caps_free(pixels);
        vTaskDelete(nullptr);
        return;
    }

    const int width  = ctx->cfg.frame.width;
    const int height = ctx->cfg.frame.height;
    const int stride = width;
    if (width <= 0 || height <= 0 ||
        static_cast<size_t>(width) * static_cast<size_t>(height) != kFrameBytes) {
        std::printf("[ppm ] init=FAIL frame %dx%d is not %u bytes\n",
                    width, height, static_cast<unsigned>(kFrameBytes));
        std::fflush(stdout);
        ppm_destroy(ctx);
        heap_caps_free(pixels);
        vTaskDelete(nullptr);
        return;
    }

    std::printf("[ppm ] init=OK  profile=%s  modules=%u  layers=%u  cap=0x%08x\n",
                ctx->cfg.profile,
                static_cast<unsigned>(ctx->cfg.module_count),
                static_cast<unsigned>(ctx->cfg.layer_count),
                static_cast<unsigned>(ctx->cfg.capability_bits));
    std::fflush(stdout);

    const HeapSample after_init = sample_heap();
    print_heap("after_init", after_init);

    ppmi_decision_t *dec = ppmi_decision_instance();
    if (dec == nullptr || ppmi_decision_configure(dec, &ctx->cfg.decision) != 0) {
        std::printf("[ppm ] init=FAIL decision configure\n");
        std::fflush(stdout);
        ppm_destroy(ctx);
        heap_caps_free(pixels);
        vTaskDelete(nullptr);
        return;
    }

    paint_ramp(pixels, width, height, stride);

    int ok = 0;
    int fail = 0;
    int streak = 0;
    int ran = 0;
    unsigned last_valid = 0;
    unsigned last_level = 0;
    unsigned last_degrade = 0;
    int64_t cost_min = INT64_MAX;
    int64_t cost_max = 0;
    int64_t cost_sum = 0;
    int cost_n = 0;
    uint8_t seen_mark = 0;
    int last_col = 0;
    uint64_t first_ts = 0;
    uint64_t last_ts = 0;
    int64_t first_real_us = 0;
    int64_t last_real_us = 0;
    uint32_t fires_at_first = 0;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(kPeriodMs);

    for (int seq = 0; seq < kFrames; ++seq) {
        const int col = static_cast<int>((static_cast<unsigned>(seq) * 8u) % 240u);
        last_col = col;
        paint_column(pixels, height, stride, col, 255);
        seen_mark = pixels[col];

        ppm_frame_t frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.data   = pixels;
        frame.size   = kFrameBytes;
        frame.width  = static_cast<uint16_t>(width);
        frame.height = static_cast<uint16_t>(height);
        frame.stride = static_cast<uint16_t>(stride);
        frame.pixfmt = ctx->cfg.frame.pixfmt;
        const int64_t stamp_us = esp_timer_get_time();
        frame.ts_ms  = static_cast<uint64_t>(stamp_us / 1000);
        frame.seq    = static_cast<uint32_t>(seq);
        if (seq == 0) {
            first_ts = frame.ts_ms;
            first_real_us = stamp_us;
            fires_at_first = g_lv_fires;
        }
        last_ts = frame.ts_ms;
        last_real_us = stamp_us;

        /* 缺省脚本：没有脸、没有眼睛。不把像素解释成闭眼。 */
        ppmi_mock_begin_frame(nullptr);

        ppm_result_t result;
        std::memset(&result, 0, sizeof(result));

        const int64_t t0 = esp_timer_get_time();
        const ppm_status_t run_st = ppm_run(ctx, &frame, &result);
        const int64_t dt = esp_timer_get_time() - t0;

        paint_column(pixels, height, stride, col, ramp_byte(col));

        if (dt < cost_min) {
            cost_min = dt;
        }
        if (dt > cost_max) {
            cost_max = dt;
        }
        cost_sum += dt;
        cost_n += 1;

        if (run_st != PPM_OK) {
            fail += 1;
            streak += 1;
            std::printf("[ppm ] run fail seq=%d status=%d\n", seq, static_cast<int>(run_st));
            std::fflush(stdout);
            if (streak >= kFailStop) {
                break;
            }
        } else {
            ok += 1;
            streak = 0;
            uint8_t degrade_bits = 0;
            if (ppmi_decision_step(dec, &ctx->bus, frame.ts_ms, frame.seq,
                                   &result.targets[0], &degrade_bits) != 0) {
                std::printf("[ppm ] decision fail seq=%d\n", seq);
                std::fflush(stdout);
            } else {
                result.degrade = static_cast<uint8_t>(result.degrade | degrade_bits);
                ctx->prev_absent_100ms = result.targets[0].absent_100ms;
                last_valid   = result.targets[0].drowsy_valid;
                last_level   = result.targets[0].drowsy_level;
                last_degrade = result.degrade;
            }
        }
        ran += 1;

        if ((seq + 1) % 10 == 0) {
            std::printf("[ppm ] seq=%d ok=%d fail=%d dt=%lldus\n",
                        seq, ok, fail, static_cast<long long>(dt));
            std::fflush(stdout);
        }

        vTaskDelayUntil(&last_wake, period);
    }

    const HeapSample after_100 = sample_heap();
    const int64_t ts_span = static_cast<int64_t>(last_ts - first_ts);
    const int64_t real_span = (last_real_us - first_real_us) / 1000;
    const int64_t drift = ts_span - real_span;
    const uint32_t fires = g_lv_fires - fires_at_first;
    const int64_t expected = (real_span > 0) ? (real_span / 100) : 0;
    /* xtensa 的 StackType_t 是 uint8_t，返回值已经是字节，不再乘字长。 */
    const UBaseType_t hw = uxTaskGetStackHighWaterMark(nullptr);
    const unsigned peak = kStackBytes - static_cast<unsigned>(hw);

    std::printf("[ppm ] frame=%d  ok=%d  fail=%d  last_valid=%u  last_level=%u  degrade=0x%08x\n",
                ran, ok, fail, last_valid, last_level, last_degrade);
    std::printf("[ppm ] time   ts_span=%lldms  real_span=%lldms  drift=%lldms\n",
                static_cast<long long>(ts_span),
                static_cast<long long>(real_span),
                static_cast<long long>(drift));
    if (cost_n > 0) {
        std::printf("[ppm ] cost   min=%lldus  avg=%lldus  max=%lldus\n",
                    static_cast<long long>(cost_min),
                    static_cast<long long>(cost_sum / cost_n),
                    static_cast<long long>(cost_max));
    }
    std::printf("[ppm ] stack  high_water=%uB free of %uB  peak=%uB\n",
                static_cast<unsigned>(hw),
                kStackBytes,
                peak);
    print_heap("after_100", after_100);
    std::printf("[lvgl] timer_fires=%lu  expected=%lld  period_ms=100\n",
                static_cast<unsigned long>(fires),
                static_cast<long long>(expected));
    std::printf("[ppm ] mark col=%d byte_while_set=%u byte_after_restore=%u\n",
                last_col,
                static_cast<unsigned>(seen_mark),
                static_cast<unsigned>(pixels[last_col]));
    std::fflush(stdout);

    /* 测量窗口结束以后继续按同一节拍跑，用来和 WiFi/LVGL 并发共存。
     * 不再打印，避免把串口刷满。 */
    for (;;) {
        ppmi_mock_begin_frame(nullptr);
        ppm_frame_t frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.data   = pixels;
        frame.size   = kFrameBytes;
        frame.width  = static_cast<uint16_t>(width);
        frame.height = static_cast<uint16_t>(height);
        frame.stride = static_cast<uint16_t>(stride);
        frame.pixfmt = ctx->cfg.frame.pixfmt;
        frame.ts_ms  = static_cast<uint64_t>(esp_timer_get_time() / 1000);
        frame.seq    = static_cast<uint32_t>(ran);
        ppm_result_t result;
        std::memset(&result, 0, sizeof(result));
        if (ppm_run(ctx, &frame, &result) == PPM_OK) {
            uint8_t degrade_bits = 0;
            if (ppmi_decision_step(dec, &ctx->bus, frame.ts_ms, frame.seq,
                                   &result.targets[0], &degrade_bits) == 0) {
                ctx->prev_absent_100ms = result.targets[0].absent_100ms;
            }
        }
        ran += 1;
        vTaskDelayUntil(&last_wake, period);
    }
}

}  // namespace

void ppm_bridge_start()
{
    /* 必须在 RunLvglLoop 之前创建。ppm_task 里创建会和 lv_timer_handler 并发改链表。 */
    if (lv_timer_create(lv_probe_cb, 100, nullptr) == nullptr) {
        std::printf("[lvgl] timer create failed\n");
        std::fflush(stdout);
    }
    const BaseType_t ok = xTaskCreate(ppm_task, "ppm", kStackBytes, nullptr, kPriority, nullptr);
    if (ok != pdPASS) {
        std::printf("[ppm ] task create failed\n");
        std::fflush(stdout);
    }
}
