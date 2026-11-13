/*
 * tcp_client.cpp —— 板端 TCP 数据上报（产品代码）
 *
 * 与探针（main/tcp_probe.cpp）的区别 —— **结构不同，不是拷贝**：
 *   探针：连上 → 打印调试文本 → 断开 → 重连        （验证连通性）
 *   产品：连上 → 每 70ms 取最新快照 → 编码 → 发送 → 断开则退避重连
 *
 * 硬参数全部来自真机实测（task #15 探针数据）：栈 4096、优先级 3、
 * 端口 9000、峰值 1,932 B。不在此重新论证。
 *
 * 帧格式：[u32 大端 长度] + 11 字段文本（长度不含头 4 字节）。
 *         必须能被 PC 端 VVOS MessageCodec::Decode 解开 —— 与 M3 的核心契约。
 */
#include "tcp_client.h"

#include "sdkconfig.h"

#if CONFIG_ROBOMIND_TCP_CLIENT_ENABLED

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "ppm_producer.h"   /* ppm_producer_snapshot() */
#include "wire_codec.h"     /* wire_encode() / kWireLineMax */

namespace {

constexpr int         kPort      = CONFIG_ROBOMIND_TCP_CLIENT_PORT;
constexpr const char *kHost      = CONFIG_ROBOMIND_TCP_CLIENT_HOST;
constexpr unsigned    kStackSize = 4096;   /* 实测峰值 1,932 B，余量 53% */
constexpr UBaseType_t kPriority  = 3;      /* 低于摄像头/LVGL，不抢主链路 */
constexpr int         kPeriodMs  = 70;     /* 与 producer 同节拍 */

/* 指数退避：链路长期不可用时不空转 */
constexpr int kBackoffMs[] = {5000, 10000, 20000, 30000};
constexpr int kBackoffN    = static_cast<int>(sizeof(kBackoffMs) / sizeof(kBackoffMs[0]));

constexpr int kSockTimeoutS = 2;   /* 避免 send/recv 永久阻塞本任务 */

/*
 * 帧缓冲：**在任务栈上**（不引入静态缓冲）。
 * 尺寸 = kWireLineMax(320) + 4 字节长度头。
 *
 * 说明：派工里写"栈上 64 B"，那是**探针**文本（`probe seq=N ts=...`）的长度；
 * 产品要发 11 字段信封，最坏约 320 B，所以缓冲必须按 kWireLineMax 取。
 * 这不是放宽要求，是目标载荷变了 —— 已在报告中标注。
 */
constexpr size_t kFrameCap = kWireLineMax + 4;

/* 日志限流：状态跃变才打印，避免断连时刷屏 */
bool g_connected = false;

void back_to_listening(const char *why, int err)
{
    if (g_connected) {
        std::printf("[tcp] disconnected (%s, errno=%d) -> backoff\n", why, err);
        std::fflush(stdout);
        g_connected = false;
    }
}

}  // namespace

void tcp_client_start(void)
{
    static const char *TAG_LOCAL = nullptr;
    (void)TAG_LOCAL;

    const unsigned before = static_cast<unsigned>(
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    TaskHandle_t handle = nullptr;
    const BaseType_t ok =
        xTaskCreate([](void *) {
            ppm_producer_snapshot_t snap;
            std::memset(&snap, 0, sizeof(snap));
            uint32_t last_gen = 0;
            int      backoff_idx = 0;
            int      sock = -1;

            /*
             * 主循环：连接管理与发送在同一任务里，但**连接失败走退避、
             * 发送失败立即回到连接管理** —— 不在这里做重试风暴。
             */
            for (;;) {
                /* ---- 建立连接 ---- */
                sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sock < 0) {
                    back_to_listening("socket", errno);
                    vTaskDelay(pdMS_TO_TICKS(kBackoffMs[backoff_idx]));
                    if (backoff_idx < kBackoffN - 1) ++backoff_idx;
                    continue;
                }

                struct timeval tv;
                tv.tv_sec = kSockTimeoutS;
                tv.tv_usec = 0;
                (void)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
                (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                struct sockaddr_in dest;
                std::memset(&dest, 0, sizeof(dest));
                dest.sin_family = AF_INET;
                dest.sin_port = htons(static_cast<uint16_t>(kPort));
                (void)inet_pton(AF_INET, kHost, &dest.sin_addr);

                if (connect(sock, reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest)) != 0) {
                    const int e = errno;
                    close(sock);
                    back_to_listening("connect", e);
                    vTaskDelay(pdMS_TO_TICKS(kBackoffMs[backoff_idx]));
                    if (backoff_idx < kBackoffN - 1) ++backoff_idx;
                    continue;
                }

                /* ---- 已连接 ---- */
                backoff_idx = 0;   /* 连上即重置退避 */
                g_connected = true;
                std::printf("[tcp] connected to %s:%d  stack_hwm=%u B free\n", kHost, kPort,
                            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
                std::fflush(stdout);

                uint8_t frame[kFrameCap];
                bool    link_lost = false;

                while (!link_lost) {
                    /* 取最新快照；gen 未变说明 producer 没产出新帧，跳过（不重发旧帧） */
                    if (!ppm_producer_snapshot(&snap)) {
                        vTaskDelay(pdMS_TO_TICKS(kPeriodMs));
                        continue;
                    }
                    if (snap.gen == last_gen) {
                        vTaskDelay(pdMS_TO_TICKS(kPeriodMs));
                        continue;
                    }

                    /*
                     * wire_encode() **自己写 4 字节大端长度头**并返回**含头总长**
                     * （wire_codec.cpp: encode_stream 里 `text_cap = cap - 4u`，
                     *  说明它预留了头部空间）。
                     *
                     * ⚠️ 第一版我按"返回纯文本长度"的假设又手写了一次头，
                     *    结果发出 [108][104][文本] 双层包头，PC 端按 108 取正文
                     *    会把下一帧的头吃进来 —— MessageCodec::Decode 必然失败。
                     *    实测证据：首帧字节 00 00 00 6c 00 00 00 68 31 7c ...
                     */
                    const size_t total =
                        wire_encode(&snap.sem, snap.source, frame, kFrameCap);
                    if (total == 0u) {
                        /* 编码失败：丢弃本帧，不重试（重试会发同样的坏帧） */
                        last_gen = snap.gen;
                        vTaskDelay(pdMS_TO_TICKS(kPeriodMs));
                        continue;
                    }

                    const int sent = send(sock, frame, static_cast<int>(total), 0);
                    if (sent != static_cast<int>(total)) {
                        /* errno 按宏名分支（数值随平台变，不写字面量） */
                        const int e = errno;
                        if (e == EWOULDBLOCK || e == EAGAIN) {
                            /* 发送缓冲满：丢本帧，不阻塞采集节拍 */
                            last_gen = snap.gen;
                        } else {
                            back_to_listening(
                                (e == ECONNRESET) ? "ECONNRESET"
                                : (e == ECONNABORTED) ? "ECONNABORTED"
                                : "send", e);
                            link_lost = true;
                        }
                        vTaskDelay(pdMS_TO_TICKS(kPeriodMs));
                        continue;
                    }
                    last_gen = snap.gen;
                    vTaskDelay(pdMS_TO_TICKS(kPeriodMs));
                }

                /* ---- 收尾：先 shutdown 再 close（规避 TIME_WAIT 边界，成本 0） ---- */
                (void)shutdown(sock, SHUT_RDWR);
                close(sock);
                vTaskDelay(pdMS_TO_TICKS(kBackoffMs[backoff_idx]));
                if (backoff_idx < kBackoffN - 1) ++backoff_idx;
            }
        },
                          "tcp_client", kStackSize, nullptr, kPriority, &handle);

    if (ok != pdPASS) {
        std::printf("[tcp] task create FAILED (pdFAIL)\n");
    } else {
        std::printf("[tcp] task started  stack=%u prio=%u  largest_before=%u B\n",
                    kStackSize, static_cast<unsigned>(kPriority), before);
        std::printf("[tcp] largest_after=%u B\n",
                    static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    }
    std::fflush(stdout);
}

#endif  // CONFIG_ROBOMIND_TCP_CLIENT_ENABLED
