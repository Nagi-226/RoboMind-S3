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

/*
 * 布局常量（UI_DESIGN.md §2.1，坐标即像素）
 *
 * 原来的尺寸是散落的魔数（32 / 60 / 22），且 msg_list 用
 * `LV_VER_RES - 32 - 60 - 22` 现算 —— 改一处标题栏高度就会漏改别处。
 * 收成常量 + 一条 static_assert，让"四段必须刚好填满 240"在编译期就成立。
 */
constexpr lv_coord_t kTitleH     = 28;   /* 原 32：单行 16px 字 + 上下 6px 内边距足够 */
constexpr lv_coord_t kInputH     = 56;   /* 原 60：40px 文本框 + 上下 8px */
constexpr lv_coord_t kStatusH    = 22;
/*
 * 屏幕高用常量而不是 LV_VER_RES：LVGL 8.x 里 LV_VER_RES 展开成
 * `lv_disp_get_vertical_resolution(NULL)` —— 是**运行期**调用，不能用于 static_assert
 * （第一版我直接写了 LV_VER_RES，编译期报 non-constant condition）。
 * 这里显式写 240（与 display_driver.cpp 的 320×240 一致），从而把
 * "四段必须刚好填满屏幕"变成编译期契约；运行期再打一条日志兜底核对。
 */
constexpr lv_coord_t kScreenH    = 240;
constexpr lv_coord_t kMsgH       = kScreenH - kTitleH - kInputH - kStatusH;  /* = 134 */
constexpr lv_coord_t kEdgePad    = 4;    /* 左右内边距 */
constexpr lv_coord_t kBubbleMaxW = 243;  /* 内宽 312 的 78%，留 69px 让"谁说的"可辨 */

/* 布局契约：四段高度必须刚好等于屏幕高。改任何一个常量，这里会红。 */
static_assert(kTitleH + kMsgH + kInputH + kStatusH == kScreenH,
              "layout segments must sum to screen height (240)");

static const char* TAG = "chat_ui";

/*
 * ─────────────────────────────────────────────────────────────────────────────
 * 主题（UI_DESIGN.md §4.2 色板）
 *
 * 为什么要收成一个结构体：原来 9 个散落的 `static const lv_color_t COLOR_*`
 * 无法整体替换，也没法表达"这是一套配色"。收成结构体后，换主题只改一行。
 *
 * 零静态存储：`const` 结构体落 .rodata（flash），不进 .dram0.bss。
 * 主题切换目前是**编译期**选择（改下面 kTheme 指向），不做运行时切换 ——
 * UI_DESIGN.md §13 明确"暗色/亮色主题切换未做"。运行时可切换需要可变指针，
 * 那会引入 .bss，违反 BRANCHING.md D2（UI 不得引入静态缓冲）。
 * ─────────────────────────────────────────────────────────────────────────────
 */
struct UiTheme {
    lv_color_t bg;             /* #12141C 屏幕底色 */
    lv_color_t surface;        /* #1B1F2A 标题栏 / 状态栏 / 输入区 */
    lv_color_t surface2;       /* #232838 输入框 / 气泡底 / 胶囊底 */
    lv_color_t primary;        /* #4C8DFF 主色（发送键底、用户气泡竖边） */
    lv_color_t accent;         /* #FFB020 强调（疲劳 1 级） */
    lv_color_t success;        /* #35C46A 正常 */
    lv_color_t warning;        /* #FFB020 警告 */
    lv_color_t danger;         /* #FF5C5C 错误 / 疲劳 ≥2 */
    lv_color_t text_primary;   /* #F2F4F8 正文 */
    lv_color_t text_secondary; /* #A7B0C0 辅助 */
    lv_color_t text_disabled;  /* #6B7484 未知态（仅用于图形点，不用于文字，见下） */
    lv_color_t border;         /* #2E3444 1px 描边（替代阴影） */
};

static const UiTheme kDarkTheme = {
    .bg             = lv_color_hex(0x12141C),
    .surface        = lv_color_hex(0x1B1F2A),
    .surface2       = lv_color_hex(0x232838),
    .primary        = lv_color_hex(0x4C8DFF),
    .accent         = lv_color_hex(0xFFB020),
    .success        = lv_color_hex(0x35C46A),
    .warning        = lv_color_hex(0xFFB020),
    .danger         = lv_color_hex(0xFF5C5C),
    .text_primary   = lv_color_hex(0xF2F4F8),
    .text_secondary = lv_color_hex(0xA7B0C0),
    .text_disabled  = lv_color_hex(0x6B7484),
    .border         = lv_color_hex(0x2E3444),
};

/* 切换主题只改这一行的指向（编译期常量，无运行时状态、无 .bss） */
static const UiTheme* const kTheme = &kDarkTheme;

/*
 * 引用别名。`static const T&` 是编译期别名，不占存储。
 */
static const UiTheme& th = *kTheme;

/*
 * 对比度实测（WCAG 相对亮度公式，本文件所有前景/背景对均已复算，见 docs/smoke/）：
 *
 *   text_primary   on bg        16.69  OK
 *   text_primary   on surface   14.95  OK
 *   text_primary   on surface2  13.32  OK
 *   text_secondary on surface    7.53  OK   ← 修掉旧 AI 气泡次级字的 4.13
 *   text_secondary on surface2   6.71  OK   ← 气泡内角色标签用这个
 *   bg             on primary    5.74  OK   ← 发送键：深字蓝底（修掉旧的 3.30）
 *   text_primary   on primary    2.91  ✗    ← 白字蓝底，**禁止使用**
 *   text_disabled  on surface2   3.11  △    ← 低于文字 AA 4.5，但 ≥3.0
 *                                             ⇒ **只可用于图形点，不可用于文字**
 *
 * 最后一条是本次实现发现的：UI_DESIGN.md 的色板表没有核算 text_disabled on surface2。
 * 处置：胶囊"未知"态的**文字**改用 text_secondary（6.71），**圆点**保留 text_disabled
 * （图形元素门槛 3.0，3.11 通过）—— 既守 AA，又保住"灰=不知道"的语义。
 */

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
    lv_obj_set_style_bg_color(screen_, th.bg, LV_PART_MAIN);

    /* 运行期兜底核对：布局常量按 240 高推算，若实际屏幕不是 240 会裁切，必须能看见 */
    const lv_coord_t real_h = lv_disp_get_ver_res(lv_disp_get_default());
    if (real_h != kScreenH) {
        ESP_LOGW(TAG, "screen height %d != layout constant %d; layout may be clipped",
                 static_cast<int>(real_h), static_cast<int>(kScreenH));
    }

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
    lv_obj_set_style_bg_color(splash, th.surface, LV_PART_MAIN);
    lv_obj_set_style_border_width(splash, 0, LV_PART_MAIN);
    lv_obj_center(splash);

    lv_obj_t* title_label = lv_label_create(splash);
    lv_label_set_text(title_label, title.c_str());
    lv_obj_set_style_text_color(title_label, th.text_primary, LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* subtitle_label = lv_label_create(splash);
    lv_label_set_text(subtitle_label, subtitle.c_str());
    lv_obj_set_style_text_color(subtitle_label, th.text_secondary, LV_PART_MAIN);
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
    lv_obj_set_size(title_bar_, LV_HOR_RES, kTitleH);
    lv_obj_set_style_bg_color(title_bar_, th.surface, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_bar_, 0, LV_PART_MAIN);
    lv_obj_align(title_bar_, LV_ALIGN_TOP_MID, 0, 0);

    /* 标题 16px（§3.2：标题只在标题栏一行、固定短文本，16 与 14 的层级差肉眼可辨） */
    lv_obj_t* label = lv_label_create(title_bar_);
    lv_label_set_text(label, "RoboMind");
    lv_obj_set_style_text_color(label, th.text_primary, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    /* 左对齐 + 左内边距 8（§2.2 坐标表），右侧留给感知胶囊 */
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
}

void ChatUI::CreateStatusBar() {
    status_bar_ = lv_obj_create(screen_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, kStatusH);
    lv_obj_set_style_bg_color(status_bar_, th.surface, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(status_bar_, 0, LV_PART_MAIN);
    lv_obj_align(status_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_label_set_text(status_label_, "Ready");
    lv_obj_set_style_text_color(status_label_, th.text_secondary, LV_PART_MAIN);
    /* 状态栏 12px（§3.2：10px 在 2.4" 上笔画易糊，宁可少放信息也要 12px） */
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(status_label_, LV_ALIGN_LEFT_MID, 8, 0);
}

void ChatUI::CreateMessageList() {
    /* 消息列表在标题栏和输入区之间；高度由布局常量推出（134），不再手算 */
    msg_list_ = lv_obj_create(screen_);
    lv_obj_set_size(msg_list_, LV_HOR_RES, kMsgH);
    lv_obj_set_style_bg_color(msg_list_, th.bg, LV_PART_MAIN);
    lv_obj_set_style_border_width(msg_list_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(msg_list_, kEdgePad, LV_PART_MAIN);
    lv_obj_set_style_pad_row(msg_list_, 6, LV_PART_MAIN);   /* 气泡间距 6（§2.2） */
    lv_obj_align(msg_list_, LV_ALIGN_TOP_MID, 0, kTitleH);
    lv_obj_set_flex_flow(msg_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(msg_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // 启用滚动
    lv_obj_set_scrollbar_mode(msg_list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_scroll_dir(msg_list_, LV_DIR_VER);
}

void ChatUI::CreateInputArea() {
    // 输入区域容器
    input_area_ = lv_obj_create(screen_);
    lv_obj_set_size(input_area_, LV_HOR_RES, kInputH);
    lv_obj_set_style_bg_color(input_area_, th.surface, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_area_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(input_area_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(input_area_, 0, LV_PART_MAIN);
    lv_obj_align(input_area_, LV_ALIGN_BOTTOM_MID, 0, -kStatusH);

    // 文本输入：250×40，圆角 8，1px 描边（§5.1 —— 用描边而非阴影，零混合成本）
    text_area_ = lv_textarea_create(input_area_);
    lv_obj_set_size(text_area_, 250, 40);
    lv_obj_align(text_area_, LV_ALIGN_LEFT_MID, kEdgePad, 0);
    lv_textarea_set_placeholder_text(text_area_, "Type message...");
    lv_textarea_set_max_length(text_area_, 256);
    lv_obj_set_style_bg_color(text_area_, th.surface2, LV_PART_MAIN);
    lv_obj_set_style_text_color(text_area_, th.text_primary, LV_PART_MAIN);
    lv_obj_set_style_border_width(text_area_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(text_area_, th.border, LV_PART_MAIN);
    lv_obj_set_style_radius(text_area_, 8, LV_PART_MAIN);

    // 单行模式: 回车发送
    lv_textarea_set_one_line(text_area_, true);
    lv_obj_add_event_cb(text_area_, OnTextareaReady, LV_EVENT_READY, this);

    // 发送按钮 56×40，圆角 10（§5.1）
    send_btn_ = lv_btn_create(input_area_);
    lv_obj_set_size(send_btn_, 56, 40);
    lv_obj_align(send_btn_, LV_ALIGN_RIGHT_MID, -kEdgePad, 0);
    lv_obj_set_style_bg_color(send_btn_, th.primary, LV_PART_MAIN);
    lv_obj_set_style_radius(send_btn_, 10, LV_PART_MAIN);

    lv_obj_t* btn_label = lv_label_create(send_btn_);
    lv_label_set_text(btn_label, LV_SYMBOL_OK);
    /*
     * 关键对比度修复（UI_DESIGN.md §4.4）：
     *   白字 on primary(#4C8DFF) = 2.91 ✗ 不合格
     *   深字(bg #12141C) on primary = 5.74 ✓ 合格
     * 发送键是要按的按钮，比正文更需要清晰 —— 只改文字颜色，不换主色调。
     */
    lv_obj_set_style_text_color(btn_label, th.bg, LV_PART_MAIN);
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
            /*
             * 流式更新时重算气泡高度。原来是写死 `content_h + 24`，依赖旧的内边距 (6+6)
             * 与角色行高；批 2 把内边距改成 8+8 之后那个常数就不准了。
             * 这里改成按实际子对象量：content_h + 间距 4 + 上下内边距 16 + 角色行高。
             */
            lv_obj_t* role_lbl = lv_obj_get_child(pending_bubble_, 0);
            const lv_coord_t role_hh = (role_lbl != nullptr) ? lv_obj_get_height(role_lbl) : 0;
            lv_obj_set_height(pending_bubble_,
                              lv_obj_get_height(pending_content_label_) + 4 + 16 + role_hh);
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

    constexpr lv_coord_t kPadX   = 10;  /* §5.1 左右 10 */
    constexpr lv_coord_t kPadY   = 8;   /* §5.1 上下 8  */
    constexpr lv_coord_t kRadius = 10;  /* §5.1 圆角 10 */

    lv_obj_t* container = lv_obj_create(msg_list_);
    /* 宽度内容自适应，上限 243（§2.2：留 69px 让"谁说的"一眼可辨） */
    lv_obj_set_width(container, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(container, kBubbleMaxW, LV_PART_MAIN);

    const bool is_user = (role == "user");

    /*
     * 两个气泡同底色（§4.5）。旧方案用两种饱和色区分，但 16bit RGB565 只有 5-6-5 位，
     * 深紫明显失真；两块大饱和色在 2.4" 上也很吵。
     * 改用"同底 + 用户侧 3px 主色竖边"：省像素、不失真、颜色只出现在需要的地方。
     */
    lv_obj_set_style_bg_color(container, th.surface2, LV_PART_MAIN);
    lv_obj_set_style_radius(container, kRadius, LV_PART_MAIN);
    lv_obj_set_style_pad_left(container, kPadX, LV_PART_MAIN);
    lv_obj_set_style_pad_right(container, kPadX, LV_PART_MAIN);
    lv_obj_set_style_pad_top(container, kPadY, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(container, kPadY, LV_PART_MAIN);

    if (is_user) {
        /* 用户气泡：左侧 3px 主色竖边（描边成本远低于阴影） */
        lv_obj_set_style_border_width(container, 3, LV_PART_MAIN);
        lv_obj_set_style_border_side(container, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
        lv_obj_set_style_border_color(container, th.primary, LV_PART_MAIN);
    } else {
        lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    }

    /* 角色标签 12px（§3.1 辅助字号）；修掉旧的 #888888 on #533483 = 4.13，现 6.71 ✓ */
    lv_obj_t* role_label = lv_label_create(container);
    lv_label_set_text(role_label, is_user ? "You" : "AI");
    lv_obj_set_style_text_color(role_label, th.text_secondary, LV_PART_MAIN);
    lv_obj_set_style_text_font(role_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(role_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 正文 14px（§3.2：不上调，16px 会让每行汉字 17→15，净损失可读性） */
    lv_obj_t* content_label = lv_label_create(container);
    lv_label_set_text(content_label, content.c_str());
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(content_label, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(content_label, kBubbleMaxW - 2 * kPadX - 4, LV_PART_MAIN);
    lv_obj_set_style_text_color(content_label, th.text_primary, LV_PART_MAIN);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_update_layout(role_label);
    const lv_coord_t role_h = lv_obj_get_height(role_label);
    lv_obj_align(content_label, LV_ALIGN_TOP_LEFT, 0, role_h + 4);

    lv_obj_update_layout(content_label);
    const lv_coord_t content_h = lv_obj_get_height(content_label);
    lv_obj_set_height(container, role_h + content_h + 4 + 2 * kPadY);

    /*
     * 用户气泡右对齐：flex 列不支持逐子项 cross-align，用 translate_x 平移。
     * 不额外包一层 row 容器 —— 那会让每条消息多一个 lv_obj（每条 ~100-160 B 池占用）。
     */
    if (is_user) {
        lv_obj_update_layout(container);
        const lv_coord_t inner_w = LV_HOR_RES - 2 * kEdgePad;
        lv_obj_set_style_translate_x(container, inner_w - lv_obj_get_width(container), 0);
    }

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
                           ? th.danger
                           : th.text_secondary;
    lv_obj_set_style_text_color(status_label_, color, LV_PART_MAIN);
}
