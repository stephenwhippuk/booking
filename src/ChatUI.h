#pragma once

#include <ncurses.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

class ServerConnection;

class ChatUI {
private:
    WINDOW* chat_win_;
    WINDOW* input_win_;
    std::vector<std::string> chat_lines_;
    int chat_height_;
    int chat_width_;
    std::mutex screen_mutex_;
    std::atomic<bool> running_{true};

    void split_and_add_lines(const std::string& line);
    void refresh_chat_window();

public:
    ChatUI();
    ~ChatUI();

    void initialize();
    bool show_login_screen(ServerConnection& connection);
    void setup_chat_windows();
    void add_chat_line(const std::string& line);
    void run_input_loop(ServerConnection& connection);
    void stop();
    void cleanup();
};
