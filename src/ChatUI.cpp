#include "ChatUI.h"
#include "ServerConnection.h"

constexpr int PORT = 3000;
constexpr const char* SERVER_IP = "127.0.0.1";

ChatUI::ChatUI() : chat_win_(nullptr), input_win_(nullptr), chat_height_(0), chat_width_(0) {}

ChatUI::~ChatUI() {
    cleanup();
}

void ChatUI::split_and_add_lines(const std::string& line) {
    size_t start = 0;
    size_t end = line.find('\n');
    
    while (end != std::string::npos) {
        std::string segment = line.substr(start, end - start);
        if (!segment.empty()) {
            chat_lines_.push_back(segment);
        }
        start = end + 1;
        end = line.find('\n', start);
    }
    
    if (start < line.length()) {
        std::string segment = line.substr(start);
        if (!segment.empty()) {
            chat_lines_.push_back(segment);
        }
    }
}

void ChatUI::refresh_chat_window() {
    std::lock_guard<std::mutex> lock(screen_mutex_);
    
    werase(chat_win_);
    box(chat_win_, 0, 0);
    mvwprintw(chat_win_, 0, 2, " Chat ");
    
    int start_line = chat_lines_.size() > (chat_height_ - 2) ? 
                     chat_lines_.size() - (chat_height_ - 2) : 0;
    
    int y = 1;
    for (size_t i = start_line; i < chat_lines_.size() && y < chat_height_ - 1; ++i) {
        mvwprintw(chat_win_, y++, 1, "%s", chat_lines_[i].c_str());
    }
    
    wrefresh(chat_win_);
}

void ChatUI::initialize() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
}

bool ChatUI::show_login_screen(ServerConnection& connection) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int login_height = 9;
    int login_width = 50;
    int login_y = (max_y - login_height) / 2;
    int login_x = (max_x - login_width) / 2;
    
    WINDOW* login_win = newwin(login_height, login_width, login_y, login_x);
    keypad(login_win, TRUE);
    box(login_win, 0, 0);
    mvwprintw(login_win, 0, 2, " Login ");
    mvwprintw(login_win, 2, 2, "Connecting to server...");
    wrefresh(login_win);
    
    std::string error_msg;
    if (!connection.connect_to_server(SERVER_IP, PORT, error_msg)) {
        mvwprintw(login_win, 3, 2, "%s", error_msg.c_str());
        mvwprintw(login_win, 5, 2, "Press any key to exit...");
        wrefresh(login_win);
        wgetch(login_win);
        delwin(login_win);
        return false;
    }

    std::string protocol_msg;
    if (!connection.receive_protocol_message(protocol_msg, error_msg)) {
        mvwprintw(login_win, 3, 2, "%s", error_msg.c_str());
        mvwprintw(login_win, 5, 2, "Press any key to exit...");
        wrefresh(login_win);
        wgetch(login_win);
        delwin(login_win);
        return false;
    }
    
    if (protocol_msg == "PROVIDE_NAME\n") {
        werase(login_win);
        box(login_win, 0, 0);
        mvwprintw(login_win, 0, 2, " Login ");
        mvwprintw(login_win, 2, 2, "Enter your name:");
        mvwprintw(login_win, 4, 2, "> ");
        wrefresh(login_win);
        
        echo();
        char name_buffer[100];
        mvwgetnstr(login_win, 4, 4, name_buffer, 99);
        noecho();
        
        std::string name = std::string(name_buffer) + '\n';
        connection.send_message(name);
        
        mvwprintw(login_win, 6, 2, "Logging in...");
        wrefresh(login_win);
        napms(500);
    }
    
    delwin(login_win);
    clear();
    refresh();
    return true;
}

void ChatUI::setup_chat_windows() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    chat_height_ = max_y - 3;
    chat_width_ = max_x;
    chat_win_ = newwin(chat_height_, chat_width_, 0, 0);
    scrollok(chat_win_, TRUE);
    
    input_win_ = newwin(3, max_x, max_y - 3, 0);
    keypad(input_win_, TRUE);
    
    box(chat_win_, 0, 0);
    box(input_win_, 0, 0);
    mvwprintw(chat_win_, 0, 2, " Chat ");
    mvwprintw(input_win_, 0, 2, " Input ");
    wrefresh(chat_win_);
    wrefresh(input_win_);
}

void ChatUI::add_chat_line(const std::string& line) {
    split_and_add_lines(line);
    refresh_chat_window();
}

void ChatUI::run_input_loop(ServerConnection& connection) {
    std::string input_buffer;
    int ch;
    
    while (running_ && connection.is_connected()) {
        {
            std::lock_guard<std::mutex> lock(screen_mutex_);
            wmove(input_win_, 1, 1);
            wclrtoeol(input_win_);
            mvwprintw(input_win_, 1, 1, "%s", input_buffer.c_str());
            box(input_win_, 0, 0);
            mvwprintw(input_win_, 0, 2, " Input ");
            wrefresh(input_win_);
        }
        
        ch = wgetch(input_win_);
        
        if (ch == '\n' || ch == KEY_ENTER) {
            if (!input_buffer.empty()) {
                std::string message = input_buffer + '\n';
                if (!connection.send_message(message)) {
                    add_chat_line("Failed to send message");
                    break;
                }
                add_chat_line("[You] " + input_buffer);
                input_buffer.clear();
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!input_buffer.empty()) {
                input_buffer.pop_back();
            }
        } else if (ch >= 32 && ch <= 126) {
            input_buffer += static_cast<char>(ch);
        } else if (ch == 27) {
            break;
        }
    }
}

void ChatUI::stop() {
    running_ = false;
}

void ChatUI::cleanup() {
    if (chat_win_) {
        delwin(chat_win_);
        chat_win_ = nullptr;
    }
    if (input_win_) {
        delwin(input_win_);
        input_win_ = nullptr;
    }
    endwin();
}
