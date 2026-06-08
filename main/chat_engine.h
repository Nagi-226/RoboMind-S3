/**
 * @file chat_engine.h
 * @brief AI 聊天引擎 — HTTP SSE 连接云端大模型
 *
 * 职责:
 *   - 维护对话历史 (system prompt + 最近 N 轮对话)
 *   - 构造 OpenAI 兼容 Chat Completions 请求
 *   - 通过 esp_http_client 发送 POST 请求 (stream=true)
 *   - 解析 SSE (Server-Sent Events) 流式响应
 *   - 通过回调将增量文本推送至 UI
 */

#ifndef ROBOMIND_CHAT_ENGINE_H
#define ROBOMIND_CHAT_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "esp_err.h"
#include "esp_http_client.h"

class ChatEngine {
public:
    enum class Status {
        kIdle,
        kThinking,
        kReceiving,
        kDone,
        kError,
    };

    struct Message {
        std::string role;     // "system", "user", "assistant"
        std::string content;
    };

    /// 收到 AI 增量文本时的回调 (role, delta_content)
    using MessageCallback = std::function<void(const std::string& role,
                                               const std::string& content)>;

    /// 引擎状态变化回调 (status, description)
    using StatusCallback = std::function<void(Status status,
                                              const std::string& info)>;

    static ChatEngine* GetInstance();

    /// 初始化引擎 (配置 API endpoint / key / model)
    bool Initialize();

    /// 发送用户消息，异步等待 AI 回复 (在独立 task 中执行)
    /// @return true if request accepted, false if busy
    bool SendMessage(const std::string& user_text);

    /// 清空聊天历史 (保留 system prompt)
    void ClearHistory();

    /// 设置系统 prompt
    void SetSystemPrompt(const std::string& prompt);

    /// 设置回调
    void SetMessageCallback(MessageCallback callback);
    void SetStatusCallback(StatusCallback callback);

    /// 获取当前历史消息数
    size_t GetHistorySize() const { return history_.size(); }

    /// 是否正在等待回复
    bool IsBusy() const { return busy_; }

private:
    ChatEngine() = default;
    ~ChatEngine();
    ChatEngine(const ChatEngine&) = delete;
    ChatEngine& operator=(const ChatEngine&) = delete;

    /// HTTP 请求 + SSE 解析 (在独立 task 中运行)
    void DoChatRequest();

    /// Network task entry for blocking HTTP/SSE requests.
    static void ChatNetworkTask(void* arg);

    /// HTTP client event callback used by esp_http_client.
    static esp_err_t HttpEventHandler(esp_http_client_event_t* evt);

    /// 构造 JSON 请求体
    std::string BuildRequestBody() const;

    /// 解析一行 SSE data
    bool ParseSSELine(const std::string& line, std::string* out_delta);

    /// Trim old chat messages while keeping the system prompt.
    void TrimHistory();

    std::deque<Message> history_;
    std::string api_endpoint_;
    std::string api_key_;
    std::string model_name_;
    std::string system_prompt_;
    std::string last_user_message_;

    MessageCallback message_callback_;
    StatusCallback status_callback_;

    bool busy_{false};
    int max_history_{20};

    std::string pending_prefix_;  // 累积的 SSE 前缀 (data: 拼接)
    std::string sse_buffer_;
};

#endif  // ROBOMIND_CHAT_ENGINE_H
