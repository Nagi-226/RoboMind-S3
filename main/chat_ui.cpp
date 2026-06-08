/**
 * @file chat_ui.cpp
 * @brief 聊天 UI 实现 — LVGL 对话界面
 *
 * UI 布局 (320x240 纵向, 可适配):
 *   ┌──────────────────────┐
 *   │   标题栏 (28px)       │
 *   ├──────────────────────┤
 *   │                      │
 *   │   消息列表 (滚动)     │
 *   │   - user 气泡 (右)   │
 *   │   - assistant 气泡   │
 *   │     (左)             │
 *   │                      │
 *   ├──────────────────────┤
 *   │  [文本输入区] [发送]  │
 *   ├──────────────────────┤
 *   │  状态栏 (20px)       │
 *   └──────────────────────┘
 */

#include "chat_ui.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "wifi_manager.h"

static const char* TAG = "chat_ui";

// 颜色常量
static const lv_color_t COLOR_BG           = lv_color_hex(0x1A1A2E);
static const lv_color_t COLOR_TITLE_BG     = lv_color_hex(0x16213E);
static const lv_color_t COLOR_USER_BUBBLE  = lv_color_hex(0x0F3460);
static const lv_color_t COLOR_AI_BUBBLE    = lv_color_hex(0x533483);
static const lv_color_t COLOR_INPUT_BG     = lv_color_hex(0x0F3460);
static const lv_color_t COLOR_SEND_BTN     = lv_color_hex(0xE94560);
static const lv_color_t COLOR_TEXT_WHITE   = lv_color_hex(0xEEEEEE);
static const lv_color_t COLOR_TEXT_DIM     = lv_color_hex(0xAAAAAA);
static const lv_color_t COLOR_STATUS_BG    = lv_color_hex(0x16213E);

static std::string FormatWifiSignal() {
    const int8_t rssi = WifiManager::GetInstance()->GetRssi();
    const char* bars = "[----]";
    if (rssi >= -50 && rssi != 0) {
        bars = "[||||]";
    } else if (rssi >= -65 && rssi != 0) {
        bars = "[|||_]";
    } else if (rssi >= -75 && rssi != 0) {
        bars = "[||__]";
    } else if (rssi >= -85 && rssi != 0) {
        bars = "[|___]";
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), " WiFi%s RSSI:%ddBm", bars,
                  static_cast<int>(rssi));
    return std::string(buffer);
}

ChatUI* ChatUI::GetInstance() {
    static ChatUI instance;
    return &instance;
}

bool ChatUI::Initialize() {
    // 创建主屏幕
    screen_ = lv_scr_act();
    if (!screen_) {
        ESP_LOGE(TAG, "Failed to get active screen");
        return false;
    }
    lv_obj_set_style_bg_color(screen_, COLOR_BG, LV_PART_MAIN);

    CreateTitleBar();
    CreateStatusBar();
    CreateMessageList();
    CreateInputArea();

    // 验证关键对象
    if (!msg_list_ || !text_area_ || !send_btn_ || !status_label_) {
        ESP_LOGE(TAG, "Critical UI object creation failed");
        return false;
    }

    ESP_LOGI(TAG, "ChatUI initialized");
    return true;
}

void ChatUI::ShowSplashScreen(const std::string& title, const std::string& subtitle) {
    // 简单启动画面: 在屏幕中央显示标题
    lv_obj_t* splash = lv_obj_create(screen_);
    lv_obj_set_size(splash, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(splash, COLOR_TITLE_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(splash, 0, LV_PART_MAIN);
    lv_obj_center(splash);

    lv_obj_t* title_label = lv_label_create(splash);
    lv_label_set_text(title_label, title.c_str());
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* subtitle_label = lv_label_create(splash);
    lv_label_set_text(subtitle_label, subtitle.c_str());
    lv_obj_set_style_text_color(subtitle_label, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_align(subtitle_label, LV_ALIGN_CENTER, 0, 20);

    // 2 秒后删除启动画面

    // 使用 LVGL timer 延迟删除
    lv_timer_create(
        [](lv_timer_t* t) {
            lv_obj_t* obj = static_cast<lv_obj_t*>(t->user_data);
            lv_timer_del(t);
            lv_obj_del(obj);
        },
        2000, splash);
}

void ChatUI::CreateTitleBar() {
    title_bar_ = lv_obj_create(screen_);
    lv_obj_set_size(title_bar_, LV_HOR_RES, 32);
    lv_obj_set_style_bg_color(title_bar_, COLOR_TITLE_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar_, 0, LV_PART_MAIN);
    lv_obj_align(title_bar_, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* label = lv_label_create(title_bar_);
    lv_label_set_text(label, "RoboMind AI");
    lv_obj_set_style_text_color(label, COLOR_TEXT_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(label);
}

void ChatUI::CreateStatusBar() {
    status_bar_ = lv_obj_create(screen_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 22);
    lv_obj_set_style_bg_color(status_bar_, COLOR_STATUS_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar_, 0, LV_PART_MAIN);
    lv_obj_align(status_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_label_set_text(status_label_, "Ready");
    lv_obj_set_style_text_color(status_label_, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_center(status_label_);
}

void ChatUI::CreateMessageList() {
    // 消息列表在标题栏和输入区之间
    msg_list_ = lv_obj_create(screen_);
    lv_obj_set_size(msg_list_, LV_HOR_RES, LV_VER_RES - 32 - 60 - 22);
    lv_obj_set_style_bg_color(msg_list_, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(msg_list_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(msg_list_, 4, LV_PART_MAIN);
    lv_obj_align(msg_list_, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_set_flex_flow(msg_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(msg_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // 启用滚动
    lv_obj_set_scrollbar_mode(msg_list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_scroll_dir(msg_list_, LV_DIR_VER);
}

void ChatUI::CreateInputArea() {
    // 输入区域容器
    input_area_ = lv_obj_create(screen_);
    lv_obj_set_size(input_area_, LV_HOR_RES, 60);
    lv_obj_set_style_bg_color(input_area_, COLOR_INPUT_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_area_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(input_area_, 0, LV_PART_MAIN);
    lv_obj_align(input_area_, LV_ALIGN_BOTTOM_MID, 0, -22);

    // 文本输入
    text_area_ = lv_textarea_create(input_area_);
    lv_obj_set_size(text_area_, LV_HOR_RES - 70, 40);
    lv_obj_align(text_area_, LV_ALIGN_LEFT_MID, 4, 0);
    lv_textarea_set_placeholder_text(text_area_, "Type message...");
    lv_textarea_set_max_length(text_area_, 256);
    lv_obj_set_style_bg_color(text_area_, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_text_color(text_area_, COLOR_TEXT_WHITE, LV_PART_MAIN);
    lv_obj_set_style_border_width(text_area_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(text_area_, COLOR_SEND_BTN, LV_PART_MAIN);

    // 单行模式: 回车发送
    lv_textarea_set_one_line(text_area_, true);
    lv_obj_add_event_cb(text_area_, OnTextareaReady, LV_EVENT_READY, this);

    // 发送按钮
    send_btn_ = lv_btn_create(input_area_);
    lv_obj_set_size(send_btn_, 56, 40);
    lv_obj_align(send_btn_, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(send_btn_, COLOR_SEND_BTN, LV_PART_MAIN);
    lv_obj_set_style_radius(send_btn_, 6, LV_PART_MAIN);

    lv_obj_t* btn_label = lv_label_create(send_btn_);
    lv_label_set_text(btn_label, LV_SYMBOL_SEND);
    lv_obj_set_style_text_color(btn_label, COLOR_TEXT_WHITE, LV_PART_MAIN);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(send_btn_, OnSendClicked, LV_EVENT_CLICKED, this);
}

void ChatUI::OnSendClicked(lv_event_t* e) {
    auto* self = static_cast<ChatUI*>(lv_event_get_user_data(e));
    const char* text = lv_textarea_get_text(self->text_area_);
    if (text && strlen(text) > 0) {
        std::string msg(text);
        lv_textarea_set_text(self->text_area_, "");

        // 在 UI 中显示用户消息
        self->AddMessageBubble("user", msg);

        // 触发回调
        if (self->input_callback_) {
            self->input_callback_(msg);
        }
    }
}

void ChatUI::OnTextareaReady(lv_event_t* e) {
    // 模拟点击发送按钮
    auto* self = static_cast<ChatUI*>(lv_event_get_user_data(e));
    lv_event_send(self->send_btn_, LV_EVENT_CLICKED, nullptr);
}

void ChatUI::SetInputCallback(InputCallback callback) {
    input_callback_ = std::move(callback);
}

void ChatUI::InjectInput(const std::string& text) {
    if (text.empty()) return;

    AddMessageBubble("user", text);
    if (input_callback_) {
        input_callback_(text);
    }
}

void ChatUI::AppendMessage(const std::string& role, const std::string& content) {
    if (role == "assistant") {
        pending_assistant_text_ += content;
        if (pending_bubble_ && pending_content_label_) {
            lv_label_set_text(pending_content_label_, pending_assistant_text_.c_str());
            lv_obj_update_layout(pending_content_label_);
            lv_obj_set_height(pending_bubble_, lv_obj_get_height(pending_content_label_) + 24);
            lv_obj_scroll_to_view(pending_bubble_, LV_ANIM_OFF);
        } else {
            pending_bubble_ = AddMessageBubble("assistant", pending_assistant_text_);
        }
    } else {
        pending_bubble_ = nullptr;
        pending_content_label_ = nullptr;
        pending_assistant_text_.clear();
        AddMessageBubble(role, content);
    }
}

lv_obj_t* ChatUI::AddMessageBubble(const std::string& role, const std::string& content) {
    if (!msg_list_) return nullptr;

    lv_obj_t* container = lv_obj_create(msg_list_);
    lv_obj_set_width(container, LV_HOR_RES - 16);

    bool is_user = (role == "user");
    lv_color_t bg_color = is_user ? COLOR_USER_BUBBLE : COLOR_AI_BUBBLE;

    lv_obj_set_style_bg_color(container, bg_color, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 6, LV_PART_MAIN);

    lv_obj_t* role_label = lv_label_create(container);
    const char* role_text = is_user ? "You" : "AI";
    lv_label_set_text(role_label, role_text);
    lv_obj_set_style_text_color(role_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(role_label, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(role_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* content_label = lv_label_create(container);
    lv_label_set_text(content_label, content.c_str());
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(content_label, LV_HOR_RES - 32);
    lv_obj_set_style_text_color(content_label, COLOR_TEXT_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_update_layout(role_label);
    lv_coord_t role_h = lv_obj_get_height(role_label);
    lv_obj_align(content_label, LV_ALIGN_TOP_LEFT, 0, role_h + 4);

    lv_obj_update_layout(content_label);
    lv_coord_t content_h = lv_obj_get_height(content_label);
    lv_obj_set_height(container, role_h + content_h + 16);

    if (!is_user) {
        pending_content_label_ = content_label;
    }

    lv_obj_scroll_to_view(container, LV_ANIM_ON);
    return container;
}

void ChatUI::SetStatus(ChatEngine::Status status, const std::string& info) {
    if (!status_label_) return;

    if (status == ChatEngine::Status::kDone || status == ChatEngine::Status::kError) {
        pending_bubble_ = nullptr;
        pending_content_label_ = nullptr;
        pending_assistant_text_.clear();
    }

    const char* icon = "";
    switch (status) {
    case ChatEngine::Status::kIdle:      icon = LV_SYMBOL_OK " "; break;
    case ChatEngine::Status::kThinking:   icon = LV_SYMBOL_REFRESH " "; break;
    case ChatEngine::Status::kReceiving:  icon = LV_SYMBOL_DOWNLOAD " "; break;
    case ChatEngine::Status::kDone:       icon = LV_SYMBOL_OK " "; break;
    case ChatEngine::Status::kError:      icon = LV_SYMBOL_CLOSE " "; break;
    }

    std::string text = std::string(icon) + info + FormatWifiSignal();
    lv_label_set_text(status_label_, text.c_str());

    lv_color_t color = (status == ChatEngine::Status::kError)
                           ? lv_color_hex(0xE94560)
                           : COLOR_TEXT_DIM;
    lv_obj_set_style_text_color(status_label_, color, LV_PART_MAIN);
}
