/**
 * @file audio_io.h
 * @brief 预留: I2S 音频 IO — 语音交互模块
 *
 * 计划阶段 2 实现:
 *   - I2S MEMS 麦克风 (INMP441) 输入 → 16kHz PCM 录音
 *   - 语音识别 API 对接 (Whisper / 云端 ASR)
 *   - TTS 合成 (云端 TTS API) → I2S 放大器 (MAX98357) 输出
 *
 * 当前状态: 接口预留，编译为空实现
 */

#ifndef ROBOMIND_AUDIO_IO_H
#define ROBOMIND_AUDIO_IO_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

class AudioIO {
public:
    enum class State {
        kIdle,
        kRecording,
        kPlaying,
        kError,
    };

    static AudioIO* GetInstance();

    /// 初始化 I2S 外设 (硬件引脚来自 Kconfig)
    bool Initialize();

    /// 开始录音 (异步，结果通过回调传递)
    bool StartRecording();

    /// 停止录音
    void StopRecording();

    /// 播放 PCM 音频 (TTS 合成结果)
    bool PlayPcm(const int16_t* samples, size_t num_samples, int sample_rate = 16000);

    /// 录音完成回调 (PCM 数据, 采样率, 采样数)
    using RecordCallback = std::function<void(const int16_t* data, size_t num_samples,
                                              int sample_rate)>;
    void SetRecordCallback(RecordCallback callback);

    State GetState() const { return state_; }

private:
    AudioIO() = default;
    ~AudioIO();
    AudioIO(const AudioIO&) = delete;
    AudioIO& operator=(const AudioIO&) = delete;

    static void CaptureTask(void* arg);
    void CaptureLoop();
    bool InitTxChannel(int sample_rate);
    bool AllocateCaptureBuffers();
    void ReleaseAudioResources();

    State state_{State::kIdle};
    RecordCallback record_callback_;
};

#endif  // ROBOMIND_AUDIO_IO_H
