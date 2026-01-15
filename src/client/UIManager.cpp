#include "UIManager.h"
#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;

UIManager::UIManager(ThreadSafeQueue<UICommand>& ui_commands,
                     ThreadSafeQueue<std::string>& input_events)
    : ui_commands_(ui_commands)
    , input_events_(input_events)
    , current_screen_(Screen::LOGIN)
    , selected_room_index_(0)
    , chat_win_(nullptr)
    , input_win_(nullptr)
    , status_win_(nullptr)
    , running_(false)
    , ncurses_initialized_(false)
{
}

UIManager::~UIManager() {
    cleanup_ncurses();
}

void UIManager::init_ncurses() {
    if (ncurses_initialized_) return;
    
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);  // Non-blocking input
    keypad(stdscr, TRUE);
    curs_set(0);  // Hide cursor
    
    ncurses_initialized_ = true;
}

void UIManager::cleanup_ncurses() {
    if (!ncurses_initialized_) return;
    
    clear_windows();
    endwin();
    ncurses_initialized_ = false;
}

void UIManager::clear_windows() {
    if (chat_win_) {
        delwin(chat_win_);
        chat_win_ = nullptr;
    }
    if (input_win_) {
        delwin(input_win_);
        input_win_ = nullptr;
    }
    if (status_win_) {
        delwin(status_win_);
        status_win_ = nullptr;
    }
}

void UIManager::setup_chat_windows() {
    clear_windows();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int chat_height = max_y - 4;  // Leave room for input and status
    chat_win_ = newwin(chat_height, max_x, 0, 0);
    
    input_win_ = newwin(3, max_x, chat_height, 0);
    
    status_win_ = newwin(1, max_x, max_y - 1, 0);
    
    scrollok(chat_win_, TRUE);
}

void UIManager::run() {
    init_ncurses();
    running_ = true;
    
    while (running_) {
        process_commands();
        poll_input();
        render();
        
        std::this_thread::sleep_for(100ms);  // ~10 FPS, reduces flicker
    }
    
    cleanup_ncurses();
}

void UIManager::stop() {
    running_ = false;
}

void UIManager::process_commands() {
    UICommand cmd(UICommandType::QUIT);
    
    // Process all available commands
    while (ui_commands_.try_pop(cmd, 0ms)) {
        switch (cmd.type) {
            case UICommandType::SHOW_LOGIN:
                current_screen_ = Screen::LOGIN;
                input_buffer_.clear();
                error_message_.clear();
                break;
                
            case UICommandType::SHOW_FOYER:
                current_screen_ = Screen::FOYER;
                selected_room_index_ = 0;
                input_buffer_.clear();
                break;
                
            case UICommandType::SHOW_CHATROOM:
                current_screen_ = Screen::CHATROOM;
                setup_chat_windows();
                input_buffer_.clear();
                if (cmd.has_data()) {
                    current_room_ = cmd.get<std::string>();
                }
                break;
                
            case UICommandType::UPDATE_ROOM_LIST:
                if (cmd.has_data()) {
                    rooms_ = cmd.get<RoomListData>().rooms;
                    // Keep selected index in bounds
                    if (selected_room_index_ >= static_cast<int>(rooms_.size())) {
                        selected_room_index_ = std::max(0, static_cast<int>(rooms_.size()) - 1);
                    }
                }
                break;
                
            case UICommandType::ADD_CHAT_MESSAGE:
                if (cmd.has_data()) {
                    chat_messages_.push_back(cmd.get<ChatMessageData>().message);
                }
                break;
                
            case UICommandType::UPDATE_PARTICIPANTS:
                if (cmd.has_data()) {
                    participants_ = cmd.get<ParticipantsData>().participants;
                }
                break;
                
            case UICommandType::SHOW_ERROR:
                if (cmd.has_data()) {
                    error_message_ = cmd.get<ErrorData>().message;
                }
                break;
                
            case UICommandType::SHOW_STATUS:
                if (cmd.has_data()) {
                    status_message_ = cmd.get<StatusData>().message;
                }
                break;
                
            case UICommandType::CLEAR_INPUT:
                input_buffer_.clear();
                break;
                
            case UICommandType::QUIT:
                running_ = false;
                break;
        }
    }
}

void UIManager::poll_input() {
    int ch = getch();
    if (ch == ERR) return;  // No input
    
    switch (current_screen_) {
        case Screen::LOGIN:
            handle_login_input(ch);
            break;
        case Screen::FOYER:
            handle_foyer_input(ch);
            break;
        case Screen::CHATROOM:
            handle_chatroom_input(ch);
            break;
    }
}

void UIManager::handle_login_input(int ch) {
    if (ch == '\n') {
        if (!input_buffer_.empty()) {
            input_events_.push("LOGIN:" + input_buffer_);
            username_ = input_buffer_;
            input_buffer_.clear();
        }
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        if (!input_buffer_.empty()) {
            input_buffer_.pop_back();
        }
    } else if (ch == 'q' || ch == 'Q') {
        if (input_buffer_.empty()) {  // Only quit if not typing
            input_events_.push("QUIT");
        } else {
            input_buffer_ += ch;
        }
    } else if (ch >= 32 && ch < 127) {  // Printable characters
        input_buffer_ += static_cast<char>(ch);
    }
}

void UIManager::handle_foyer_input(int ch) {
    if (ch == KEY_UP) {
        if (selected_room_index_ > 0) {
            selected_room_index_--;
        }
    } else if (ch == KEY_DOWN) {
        if (selected_room_index_ < static_cast<int>(rooms_.size()) - 1) {
            selected_room_index_++;
        }
    } else if (ch == '\n') {
        // Check if user is typing a room name first
        if (!input_buffer_.empty()) {
            // Create new room
            input_events_.push("CREATE_ROOM:" + input_buffer_);
            input_buffer_.clear();
            status_message_.clear();
        } else if (selected_room_index_ >= 0 && selected_room_index_ < static_cast<int>(rooms_.size())) {
            // Join selected room
            input_events_.push("ROOM_SELECTED:" + rooms_[selected_room_index_].name);
        }
    } else if (ch == 'c' || ch == 'C') {
        // Start creating a room (just a visual cue)
        status_message_ = "Enter room name: ";
    } else if (ch == 'q' || ch == 'Q') {
        input_events_.push("QUIT");
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        if (!input_buffer_.empty()) {
            input_buffer_.pop_back();
        } else {
            status_message_.clear();  // Clear prompt if backspace on empty
        }
    } else if (ch >= 32 && ch < 127) {
        input_buffer_ += static_cast<char>(ch);
    }
}

void UIManager::handle_chatroom_input(int ch) {
    if (ch == '\n') {
        if (!input_buffer_.empty()) {
            if (input_buffer_ == "/leave") {
                input_events_.push("LEAVE");
            } else if (input_buffer_ == "/quit") {
                input_events_.push("QUIT");
            } else {
                input_events_.push("CHAT_MESSAGE:" + input_buffer_);
            }
            input_buffer_.clear();
        }
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        if (!input_buffer_.empty()) {
            input_buffer_.pop_back();
        }
    } else if (ch >= 32 && ch < 127) {
        input_buffer_ += static_cast<char>(ch);
    }
}

void UIManager::render() {
    switch (current_screen_) {
        case Screen::LOGIN:
            render_login();
            break;
        case Screen::FOYER:
            render_foyer();
            break;
        case Screen::CHATROOM:
            render_chatroom();
            break;
    }
    
    // Show error if present
    if (!error_message_.empty()) {
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        attron(A_REVERSE);
        mvprintw(max_y - 1, 0, "ERROR: %s", error_message_.c_str());
        attroff(A_REVERSE);
        error_message_.clear();  // Clear after showing once
        wnoutrefresh(stdscr);
    }
    
    // Update physical screen once (double buffering)
    doupdate();
}

void UIManager::render_login() {
    werase(stdscr);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int y = max_y / 2 - 3;
    int x = max_x / 2 - 15;
    
    mvprintw(y, x, "+============================+");
    mvprintw(y + 1, x, "|      Chat Client Login     |");
    mvprintw(y + 2, x, "+============================+");
    
    mvprintw(y + 4, x, "Enter your name:");
    mvprintw(y + 5, x, "> %s", input_buffer_.c_str());
    
    mvprintw(y + 7, x, "Press 'q' to quit");
    
    // Show cursor at input position
    move(y + 5, x + 2 + input_buffer_.length());
    curs_set(1);
    
    wnoutrefresh(stdscr);
}

void UIManager::render_foyer() {
    werase(stdscr);
    curs_set(0);  // Hide cursor
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    mvprintw(0, 0, "+=== FOYER ===+");
    mvprintw(1, 0, "Available Rooms:");
    
    if (rooms_.empty()) {
        mvprintw(3, 2, "No rooms available. Press 'c' to create one.");
    } else {
        int y = 3;
        for (size_t i = 0; i < rooms_.size() && y < max_y - 5; ++i) {
            if (static_cast<int>(i) == selected_room_index_) {
                attron(A_REVERSE);
            }
            mvprintw(y++, 2, "%s (%d users)", 
                     rooms_[i].name.c_str(), 
                     rooms_[i].client_count);
            if (static_cast<int>(i) == selected_room_index_) {
                attroff(A_REVERSE);
            }
        }
    }
    
    int help_y = max_y - 4;
    mvprintw(help_y, 0, "----------------------------");
    mvprintw(help_y + 1, 0, "Up/Down: Navigate | Enter: Join | c: Create room");
    
    if (!input_buffer_.empty() || !status_message_.empty()) {
        mvprintw(help_y + 2, 0, "%s%s", status_message_.c_str(), input_buffer_.c_str());
    }
    
    mvprintw(help_y + 3, 0, "q: Quit");
    
    wnoutrefresh(stdscr);
}

void UIManager::render_chatroom() {
    if (!chat_win_ || !input_win_) {
        setup_chat_windows();
    }
    
    // Render chat window
    werase(chat_win_);
    box(chat_win_, 0, 0);
    
    if (!current_room_.empty()) {
        mvwprintw(chat_win_, 0, 2, " %s ", current_room_.c_str());
    }
    
    int chat_height, chat_width;
    getmaxyx(chat_win_, chat_height, chat_width);
    
    // Show last N messages that fit
    int start_idx = chat_messages_.size() > (size_t)(chat_height - 2)
        ? chat_messages_.size() - (chat_height - 2)
        : 0;
    
    int y = 1;
    for (size_t i = start_idx; i < chat_messages_.size() && y < chat_height - 1; ++i) {
        mvwprintw(chat_win_, y++, 1, "%.*s", 
                  chat_width - 2, 
                  chat_messages_[i].c_str());
    }
    
    wnoutrefresh(chat_win_);
    
    // Render input window
    werase(input_win_);
    box(input_win_, 0, 0);
    mvwprintw(input_win_, 0, 2, " Input ");
    mvwprintw(input_win_, 1, 1, "> %s", input_buffer_.c_str());
    wnoutrefresh(input_win_);
    
    // Show cursor at input position
    int input_y, input_x;
    getbegyx(input_win_, input_y, input_x);
    move(input_y + 1, input_x + 3 + input_buffer_.length());
    curs_set(1);
}

void UIManager::show_error_popup(const std::string& message) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int popup_height = 7;
    int popup_width = std::min(60, max_x - 4);
    int popup_y = (max_y - popup_height) / 2;
    int popup_x = (max_x - popup_width) / 2;
    
    WINDOW* popup = newwin(popup_height, popup_width, popup_y, popup_x);
    box(popup, 0, 0);
    mvwprintw(popup, 0, 2, " Error ");
    mvwprintw(popup, 2, 2, "%.*s", popup_width - 4, message.c_str());
    mvwprintw(popup, 4, 2, "Press any key...");
    wrefresh(popup);
    
    // Wait for keypress (blocking)
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);
    
    delwin(popup);
}
