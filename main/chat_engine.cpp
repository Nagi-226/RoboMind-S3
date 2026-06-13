/**
 * @file chat_engine.cpp
 * @brief AI 聊天引擎实现
 *
 * 协议: OpenAI Chat Completions API (stream=true → SSE)
 * 适配: Claude API, Ollama 等 OpenAI 兼容接口
 */

#include "chat_engine.h"

#include <cJSON.h>


#include "esp_http_client.h"

extern "C" esp_err_t esp_crt_bundle_attach(void* conf);
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "system_monitor.h"

static const char* TAG = "chat_engine";

static TaskHandle_t s_chat_task = nullptr;
static SemaphoreHandle_t s_request_sem = nullptr;
static std::string s_pending_message;


ChatEngine* ChatEngine::GetInstance() {
    static ChatEngine instance;
    return &instance;
}

ChatEngine::~ChatEngine() {
    if (history_mutex_) {
        vSemaphoreDelete(history_mutex_);
        history_mutex_ = nullptr;
    }
}

bool ChatEngine::Initialize() {
    api_endpoint_ = CONFIG_ROBOMIND_AI_API_ENDPOINT;
    api_key_ = CONFIG_ROBOMIND_AI_API_KEY;
    model_name_ = CONFIG_ROBOMIND_AI_MODEL_NAME;
    system_prompt_ = CONFIG_ROBOMIND_AI_SYSTEM_PROMPT;
    max_history_ = CONFIG_ROBOMIND_CHAT_HISTORY_MAX;

    if (!history_mutex_) {
        history_mutex_ = xSemaphoreCreateMutex();
        if (!history_mutex_) {
            ESP_LOGE(TAG, "history mutex creation failed");
            return false;
        }
    }

    if (system_prompt_.empty()) {
        system_prompt_ = "You are RoboMind, a concise and helpful robot assistant.";
    }
    history_.push_back({"system", system_prompt_});

    if (!s_request_sem) {
        s_request_sem = xSemaphoreCreateBinary();
        if (!s_request_sem) {
            ESP_LOGE(TAG, "request semaphore create failed");
            return false;
        }
    }

    if (!s_chat_task) {
        BaseType_t task_ret = xTaskCreatePinnedToCore(
            ChatNetworkTask, "chat_net", 8192, this, 4, &s_chat_task, 1);
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "chat network task create failed");
            return false;
        }
    }

    ESP_LOGI(TAG, "ChatEngine initialized: model=%s, endpoint=%s",
             model_name_.c_str(), api_endpoint_.c_str());
    return true;
}

void ChatEngine::SetSystemPrompt(const std::string& prompt) {
    system_prompt_ = prompt;
    if (history_mutex_) xSemaphoreTake(history_mutex_, portMAX_DELAY);
    if (!history_.empty() && history_.front().role == "system") {
        history_.front().content = prompt;
    } else {
        history_.push_front({"system", prompt});
    }
    if (history_mutex_) xSemaphoreGive(history_mutex_);
}

void ChatEngine::SetMessageCallback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void ChatEngine::SetStatusCallback(StatusCallback callback) {
    status_callback_ = std::move(callback);
}

void ChatEngine::ClearHistory() {
    if (history_mutex_) xSemaphoreTake(history_mutex_, portMAX_DELAY);
    history_.clear();
    if (!system_prompt_.empty()) {
        history_.push_back({"system", system_prompt_});
    }
    if (history_mutex_) xSemaphoreGive(history_mutex_);
}

bool ChatEngine::SendMessage(const std::string& user_text) {
    if (busy_) {
        ESP_LOGW(TAG, "ChatEngine busy, rejecting message");
        return false;
    }
    if (!s_request_sem) {
        ESP_LOGE(TAG, "ChatEngine not initialized");
        return false;
    }

    if (!user_text.empty() && user_text[0] == '#') {
        std::string response = HandleLocalCommand(user_text);
        if (!response.empty()) {
            if (history_mutex_) xSemaphoreTake(history_mutex_, portMAX_DELAY);
            history_.push_back({"user", user_text});
            history_.push_back({"assistant", response});
            TrimHistory();
            if (history_mutex_) xSemaphoreGive(history_mutex_);
            if (message_callback_) {
                message_callback_("assistant", response);
            }
            if (status_callback_) {
                status_callback_(Status::kDone, "");
            }
            return true;
        }
    }

    last_user_message_ = user_text;
    if (history_mutex_) xSemaphoreTake(history_mutex_, portMAX_DELAY);
    history_.push_back({"user", user_text});
    TrimHistory();
    if (history_mutex_) xSemaphoreGive(history_mutex_);

    busy_ = true;
    if (status_callback_) {
        status_callback_(Status::kThinking, "Thinking...");
    }

    s_pending_message = user_text;
    if (xSemaphoreGive(s_request_sem) != pdTRUE) {
        ESP_LOGE(TAG, "failed to signal chat network task");
        busy_ = false;
        return false;
    }

    return true;
}

void ChatEngine::ChatNetworkTask(void* arg) {
    auto* engine = static_cast<ChatEngine*>(arg);
    while (true) {
        if (xSemaphoreTake(s_request_sem, portMAX_DELAY) == pdTRUE) {
            s_pending_message.clear();
            engine->DoChatRequest();
            engine->busy_ = false;
        }
    }
}

void ChatEngine::TrimHistory() {
    while (history_.size() > static_cast<size_t>(max_history_ + 1)) {
        auto it = history_.begin();
        if (it->role == "system") ++it;
    if (it != history_.end()) history_.erase(it);
    }
}

std::string ChatEngine::HandleLocalCommand(const std::string& text) {
    if (text == "#status") {
        SystemHealth health = SystemMonitor::GetInstance()->Snapshot();
        return SystemMonitor::GetInstance()->FormatHealth(health);
    }
    if (text == "#photo") {
        return "Photo capture queued. (Will save to /sdcard/photos/ when SD available.)";
    }
    if (text == "#voice") {
        return "Voice recording started. (ASR pipeline pending Phase 3.)";
    }
    if (text == "#help") {
        return "Commands: #status #photo #voice #help #clear";
    }
    if (text == "#clear") {
        ClearHistory();
        return "Chat history cleared.";
    }
    return "";
}

std::string ChatEngine::BuildRequestBody() const {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", model_name_.c_str());
    cJSON_AddBoolToObject(root, "stream", true);
    cJSON_AddNumberToObject(root, "max_tokens", CONFIG_ROBOMIND_AI_MAX_TOKENS);
    float temperature = static_cast<float>(CONFIG_ROBOMIND_AI_TEMPERATURE) / 100.0f;
    cJSON_AddNumberToObject(root, "temperature", temperature);

    if (history_mutex_) xSemaphoreTake(history_mutex_, portMAX_DELAY);
    cJSON* messages = cJSON_AddArrayToObject(root, "messages");
    for (const auto& msg : history_) {
        cJSON* msg_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(msg_obj, "role", msg.role.c_str());
        cJSON_AddStringToObject(msg_obj, "content", msg.content.c_str());
        cJSON_AddItemToArray(messages, msg_obj);
    }
    if (history_mutex_) xSemaphoreGive(history_mutex_);

    char* body = cJSON_PrintUnformatted(root);
    std::string result(body);
    cJSON_free(body);
    cJSON_Delete(root);

    return result;
}

bool ChatEngine::ParseSSELine(const std::string& line, std::string* out_delta) {
    // SSE 标准格式: "data: <json>" 或 "data: [DONE]"
    if (line.empty() || line[0] == ':') return false;  // 注释行

    const char* prefix = "data: ";
    size_t prefix_len = 6;

    if (line.size() < prefix_len || line.compare(0, prefix_len, prefix) != 0) {
        return false;
    }

    std::string data = line.substr(prefix_len);

    // 心跳行 (data: 后面无内容) 直接跳过，避免 cJSON_Parse("") 日志噪音
    if (data.empty() || data.find_first_not_of(" \t") == std::string::npos) {
        return false;
    }

    // 流结束标记 (大小写不敏感)
    if (data == "[DONE]" || data == "[done]" || data == "[Done]") {
        *out_delta = "[DONE]";
        return true;
    }

    // 解析 JSON: {"choices":[{"delta":{"content":"..."}}]}
    cJSON* root = cJSON_Parse(data.c_str());
    if (!root) {
        ESP_LOGW(TAG, "SSE JSON parse error: %s", data.substr(0, 60).c_str());
        return false;
    }

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON* choice = cJSON_GetArrayItem(choices, 0);
        cJSON* delta = cJSON_GetObjectItem(choice, "delta");
        if (delta) {
            cJSON* content = cJSON_GetObjectItem(delta, "content");
            if (content && cJSON_IsString(content)) {
                *out_delta = content->valuestring;
            }
            // Claude API 兼容: "text" 字段
            if (out_delta->empty()) {
                cJSON* text = cJSON_GetObjectItem(delta, "text");
                if (text && cJSON_IsString(text)) {
                    *out_delta = text->valuestring;
                }
            }
        }
    }
    cJSON_Delete(root);
    return !out_delta->empty();
}
// HTTP response callback for SSE stream data.
static ChatEngine* s_engine_for_http = nullptr;

esp_err_t ChatEngine::HttpEventHandler(esp_http_client_event_t* evt) {
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA: {
        if (!s_engine_for_http) break;

        s_engine_for_http->sse_buffer_.append(
            reinterpret_cast<const char*>(evt->data), evt->data_len);

        size_t pos;
        while ((pos = s_engine_for_http->sse_buffer_.find('\n')) != std::string::npos) {
            std::string line = s_engine_for_http->sse_buffer_.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            s_engine_for_http->sse_buffer_.erase(0, pos + 1);

            if (line.empty()) continue;

            std::string delta;
            if (s_engine_for_http->ParseSSELine(line, &delta)) {
                if (delta == "[DONE]") {
                    if (s_engine_for_http->status_callback_) {
                        s_engine_for_http->status_callback_(ChatEngine::Status::kDone, "");
                    }
                } else if (s_engine_for_http->message_callback_) {
                    s_engine_for_http->message_callback_("assistant", delta);
                }
            }
        }
        break;
    }
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP request finished");
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP error");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void ChatEngine::DoChatRequest() {
    std::string body = BuildRequestBody();
    std::string full_response;
    auto orig_msg_cb = message_callback_;

    message_callback_ = [&](const std::string& role, const std::string& content) {
        full_response += content;
        if (orig_msg_cb) {
            orig_msg_cb(role, content);
        }
    };

    esp_http_client_config_t config = {};
    config.url = api_endpoint_.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = CONFIG_ROBOMIND_HTTP_TIMEOUT_MS;
    config.event_handler = ChatEngine::HttpEventHandler;
    config.buffer_size = 4096;
    config.disable_auto_redirect = false;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    std::string auth_header = "Bearer " + api_key_;
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header.c_str());
    esp_http_client_set_header(client, "Accept", "text/event-stream");
    esp_http_client_set_post_field(client, body.c_str(), body.size());

    s_engine_for_http = this;
    sse_buffer_.clear();

    if (status_callback_) {
        status_callback_(Status::kReceiving, "Waiting for response...");
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        if (status_callback_) {
            status_callback_(Status::kError,
                             std::string("HTTP failed: ") + esp_err_to_name(err));
        }
    } else {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP status: %d", status_code);

        if (status_code != 200) {
            if (status_callback_) {
                status_callback_(Status::kError,
                                 "API returned " + std::to_string(status_code));
            }
        } else {
            if (history_mutex_) xSemaphoreTake(history_mutex_, portMAX_DELAY);
            if (!full_response.empty()) {
                history_.push_back({"assistant", full_response});
            } else {
                history_.push_back({"assistant", "[empty response]"});
            }
            TrimHistory();
            if (history_mutex_) xSemaphoreGive(history_mutex_);

            // 流正常结束但可能未收到 [DONE] (某些 API 实现不发送)
            // 强制通知 UI 完成，避免状态卡在 kReceiving
            if (status_callback_) {
                status_callback_(Status::kDone, "");
            }
        }
    }

    esp_http_client_cleanup(client);
    s_engine_for_http = nullptr;
    message_callback_ = orig_msg_cb;

    ESP_LOGI(TAG, "Chat request complete, full_response=%d chars",
             static_cast<int>(full_response.size()));
}