#pragma once

#include <ncurses.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <map>
#include "Event.h"
#include "RoomInfo.h"

class ChatUI {
private:
    WINDOW* chat_win_;
    WINDOW* input_win_;
    std::vector<std::string> chat_lines_;
    int chat_height_;
    int chat_width_;
    std::mutex screen_mutex_;
    std::atomic<bool> running_{true};
    std::atomic<bool> in_input_loop_{false};
    std::string current_room_;
    EventBus& event_bus_;
    
    enum class UIState {
        LOGIN,
        LOADING,
        FOYER,
        CHATROOM
    };
    UIState current_state_;
    std::vector<RoomInfo> current_rooms_;

    void split_and_add_lines(const std::string& line);
    void refresh_chat_window();

public:
    ChatUI(EventBus& event_bus);
    ~ChatUI();

    void initialize();
    
    // Event handlers
    void handle_initialized(const Event& event);
    void handle_logged_in(const Event& event);
    void handle_kicked(const Event& event);
    void handle_foyer_joined(const Event& event);
    void handle_room_joined(const Event& event);
    void handle_chat_received(const Event& event);
    void handle_rooms_updated(const Event& event);
    
    // UI screens
    void show_login_screen();
    void show_loading_screen(const std::string& message);
    void show_foyer_screen();
    void setup_chat_windows();
    void clear_chat_windows();
    void add_chat_line(const std::string& line);
    void run_input_loop();
    void stop();
    void cleanup();
    void set_current_room(const std::string& room);
    std::string get_current_room() const;
};
