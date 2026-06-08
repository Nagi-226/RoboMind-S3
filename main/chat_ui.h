/**
 * @file chat_ui.h
 * @brief 聊天 UI 界面 — LVGL 对话屏幕 (单例)
 *
 * 职责:
 *   - 创建聊天界面 (标题栏 + 消息列表 + 输入区 + 发送按钮)
 *   - 接收 ChatEngine 回调, 渲染对话气泡
 *   - 处理触摸/串口输入
 *   - 显示状态指示 (Idle / Thinking / Error)
 */

#ifndef ROBOMIND_CHAT_UI_H
#define ROBOMIND_CHAT_UI_H

#include <functional>
#include <string>

#include "lvgl.h"
#include "chat_engine.h"

class ChatUI {
public:
    static ChatUI* GetInstance();

    /// 初始化 LVGL UI 组件
    bool Initialize();

    /// 显示启动画面 (持续 2 秒后自动切换到聊天界面)
    void ShowSplashScreen(const std::string& title, const std::string& subtitle);

    /// 添加消息气泡到对话列表
    void AppendMessage(const std::string& role, const std::string& content);

    /// 更新状态栏
    void SetStatus(ChatEngine::Status status, const std::string& info);

    /// 设置用户输入回调 (触摸发送 或 串口输入)
    using InputCallback = std::function<void(const std::string& text)>;
    void SetInputCallback(InputCallback callback);

    /// 程序化注入输入 (用于串口终端输入)
    void InjectInput(const std::string& text);

private:
    ChatUI() = default;
    ~ChatUI() = default;
    ChatUI(const ChatUI&) = delete;
    ChatUI& operator=(const ChatUI&) = delete;

    /// 创建标题栏
    void CreateTitleBar();
    /// 创建消息列表容器 (可滚动)
    void CreateMessageList();
    /// 创建输入区域
    void CreateInputArea();
    /// 创建状态指示器
    void CreateStatusBar();

    /// 添加一条消息气泡到 UI
    lv_obj_t* AddMessageBubble(const std::string& role, const std::string& content);

    /// 发送按钮事件处理
    static void OnSendClicked(lv_event_t* e);

    /// 文本域键盘就绪事件
    static void OnTextareaReady(lv_event_t* e);

    InputCallback input_callback_;

    lv_obj_t* screen_{nullptr};
    lv_obj_t* title_bar_{nullptr};
    lv_obj_t* msg_list_{nullptr};
    lv_obj_t* input_area_{nullptr};
    lv_obj_t* status_bar_{nullptr};
    lv_obj_t* text_area_{nullptr};
    lv_obj_t* send_btn_{nullptr};
    lv_obj_t* status_label_{nullptr};

    std::string pending_assistant_text_;  // 流式累积中的 assistant 消息
    lv_obj_t* pending_bubble_{nullptr};
    lv_obj_t* pending_content_label_{nullptr};
};

#endif  // ROBOMIND_CHAT_UI_H
