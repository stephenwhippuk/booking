#include "ChatUI.h"
#include "CommandParser.h"
#include <iostream>
#include <chrono>
#include <thread>

ChatUI::ChatUI(EventBus& event_bus) 
    : chat_win_(nullptr), input_win_(nullptr), chat_height_(0), chat_width_(0), 
      current_room_(""), event_bus_(event_bus), current_state_(UIState::LOGIN) {}

ChatUI::~ChatUI() {
    cleanup();
}

void ChatUI::set_current_room(const std::string& room) {
    current_room_ = room;
}

std::string ChatUI::get_current_room() const {
    return current_room_;
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
    
    std::string title = " Chat ";
    if (!current_room_.empty()) {
        title = " " + current_room_ + " ";
    }
    mvwprintw(chat_win_, 0, 2, "%s", title.c_str());
    
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
    
    // Raise initialized event
    Event event(EventType::INITIALIZED);
    event_bus_.publish(event);
}

void ChatUI::handle_initialized(const Event& event) {
    current_state_ = UIState::LOGIN;
    show_login_screen();
}

void ChatUI::show_login_screen() {
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
    mvwprintw(login_win, 2, 2, "Enter your name:");
    mvwprintw(login_win, 4, 2, "> ");
    wrefresh(login_win);
    
    echo();
    char name_buffer[100];
    mvwgetnstr(login_win, 4, 4, name_buffer, 99);
    noecho();
    
    std::string username(name_buffer);
    if (!username.empty()) {
        // Raise login_submitted event
        Event event(EventType::LOGIN_SUBMITTED);
        event.set("username", username);
        event_bus_.publish(event);
    }
    
    delwin(login_win);
    clear();
    refresh();
}

void ChatUI::show_loading_screen(const std::string& message) {
    std::lock_guard<std::mutex> lock(screen_mutex_);
    
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int loading_height = 5;
    int loading_width = 40;
    int loading_y = (max_y - loading_height) / 2;
    int loading_x = (max_x - loading_width) / 2;
    
    WINDOW* loading_win = newwin(loading_height, loading_width, loading_y, loading_x);
    box(loading_win, 0, 0);
    mvwprintw(loading_win, 0, 2, " Loading ");
    mvwprintw(loading_win, 2, 2, "%s", message.c_str());
    wrefresh(loading_win);
    delwin(loading_win);
    refresh();
}

void ChatUI::handle_logged_in(const Event& event) {
    current_state_ = UIState::LOADING;
    // Don't show loading screen here - it causes re-entrancy issues
    // The state change will cause run_input_loop to exit if we're in a room
}

void ChatUI::handle_kicked(const Event& event) {
    std::string reason = event.get_or<std::string>("reason", "Connection lost");
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int msg_height = 7;
    int msg_width = 50;
    int msg_y = (max_y - msg_height) / 2;
    int msg_x = (max_x - msg_width) / 2;
    
    WINDOW* msg_win = newwin(msg_height, msg_width, msg_y, msg_x);
    box(msg_win, 0, 0);
    mvwprintw(msg_win, 0, 2, " Error ");
    mvwprintw(msg_win, 2, 2, "%s", reason.c_str());
    mvwprintw(msg_win, 4, 2, "Press any key to return to login...");
    wrefresh(msg_win);
    wgetch(msg_win);
    delwin(msg_win);
    
    current_state_ = UIState::LOGIN;
    show_login_screen();
}

void ChatUI::handle_foyer_joined(const Event& event) {
    current_rooms_ = event.get<std::vector<RoomInfo>>("rooms");
    current_state_ = UIState::FOYER;
    
    // If we're in a chat room (input loop running), just change state and let the loop exit
    // If we're NOT in a chat room, show the foyer immediately
    if (!in_input_loop_) {
        show_foyer_screen();
    }
}

void ChatUI::handle_room_joined(const Event& event) {
    current_state_ = UIState::CHATROOM;
    std::string room_name = event.get_or<std::string>("room_name", "Chat Room");
    set_current_room(room_name);
    setup_chat_windows();
    add_chat_line("[System] Joined room: " + room_name);
    
    // Run input loop in a separate thread to avoid blocking the receive thread
    std::thread input_thread([this]() {
        run_input_loop();
    });
    input_thread.detach();
}

void ChatUI::handle_chat_received(const Event& event) {
    std::string message = event.get<std::string>("message");
    add_chat_line(message);
}

void ChatUI::handle_rooms_updated(const Event& event) {
    current_rooms_ = event.get<std::vector<RoomInfo>>("rooms");
    
    // If we're in the foyer, refresh the display
    if (current_state_ == UIState::FOYER) {
        show_foyer_screen();
    }
}

void ChatUI::show_foyer_screen() {
    // Clear the screen first
    clear();
    refresh();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int foyer_height = 20;
    int foyer_width = 60;
    int foyer_y = (max_y - foyer_height) / 2;
    int foyer_x = (max_x - foyer_width) / 2;
    
    WINDOW* foyer_win = newwin(foyer_height, foyer_width, foyer_y, foyer_x);
    keypad(foyer_win, TRUE);
    wtimeout(foyer_win, 500); // 500ms timeout to allow periodic refresh
    
    int selected = 0;
    std::string action = "";
    std::vector<RoomInfo> last_rooms = current_rooms_;
    
    while (true) {
        // Check if room list changed (including user counts) and refresh
        bool rooms_changed = false;
        if (current_rooms_.size() != last_rooms.size()) {
            rooms_changed = true;
        } else {
            for (size_t i = 0; i < current_rooms_.size(); ++i) {
                if (current_rooms_[i].name != last_rooms[i].name || 
                    current_rooms_[i].client_count != last_rooms[i].client_count) {
                    rooms_changed = true;
                    break;
                }
            }
        }
        
        if (rooms_changed) {
            last_rooms = current_rooms_;
            // Reset selection if it's now out of bounds
            if (selected >= static_cast<int>(current_rooms_.size()) + 1) {
                selected = 0;
            }
        }
        
        werase(foyer_win);
        box(foyer_win, 0, 0);
        mvwprintw(foyer_win, 0, 2, " Chat Rooms - Foyer ");
        
        mvwprintw(foyer_win, 2, 2, "Available Rooms:");
        
        int y = 4;
        for (size_t i = 0; i < current_rooms_.size() && y < foyer_height - 5; ++i) {
            if (selected == static_cast<int>(i)) {
                wattron(foyer_win, A_REVERSE);
            }
            mvwprintw(foyer_win, y++, 4, "%s (%d users)", 
                     current_rooms_[i].name.c_str(), current_rooms_[i].client_count);
            if (selected == static_cast<int>(i)) {
                wattroff(foyer_win, A_REVERSE);
            }
        }
        
        if (selected == static_cast<int>(current_rooms_.size())) {
            wattron(foyer_win, A_REVERSE);
        }
        mvwprintw(foyer_win, foyer_height - 4, 4, "[Create New Room]");
        if (selected == static_cast<int>(current_rooms_.size())) {
            wattroff(foyer_win, A_REVERSE);
        }
        
        mvwprintw(foyer_win, foyer_height - 2, 2, "Use arrows, Enter to select, Q to quit");
        wrefresh(foyer_win);
        
        int ch = wgetch(foyer_win);
        
        // ERR means timeout, just continue to refresh
        if (ch == ERR) {
            continue;
        }
        
        if (ch == KEY_UP && selected > 0) {
            selected--;
        } else if (ch == KEY_DOWN && selected < static_cast<int>(current_rooms_.size())) {
            selected++;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (selected < static_cast<int>(current_rooms_.size())) {
                // Raise room_selected event
                Event event(EventType::ROOM_SELECTED);
                event.set("room_name", current_rooms_[selected].name);
                event_bus_.publish(event);
                break;
            } else {
                action = "CREATE";
                break;
            }
        } else if (ch == 'q' || ch == 'Q' || ch == 27) {
            Event event(EventType::APP_KILLED);
            event_bus_.publish(event);
            break;
        }
    }
    
    delwin(foyer_win);
    clear();
    refresh();
    
    if (action == "CREATE") {
        // Show room creation prompt in a separate window
        WINDOW* create_win = newwin(8, 50, (max_y - 8) / 2, (max_x - 50) / 2);
        box(create_win, 0, 0);
        mvwprintw(create_win, 0, 2, " Create New Room ");
        mvwprintw(create_win, 2, 2, "Enter room name:");
        mvwprintw(create_win, 4, 2, "> ");
        wrefresh(create_win);
        
        echo();
        char room_buffer[50];
        mvwgetnstr(create_win, 4, 4, room_buffer, 49);
        noecho();
        
        std::string room_name(room_buffer);
        delwin(create_win);
        clear();
        refresh();
        
        if (!room_name.empty()) {
            // Raise room_requested event
            Event event(EventType::ROOM_REQUESTED);
            event.set("room_name", room_name);
            event_bus_.publish(event);
            
            // Don't wait here - just return and let the event system handle the transition
            // If room creation succeeds, ROOM_JOINED event will be published
            // and handle_room_joined will set up the chat room
            return;
        } else {
            // Empty room name, go back to foyer
            show_foyer_screen();
        }
    }
    
    clear();
    refresh();
}

void ChatUI::setup_chat_windows() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Clear old messages and reset state
    chat_lines_.clear();
    running_ = true;
    
    // Delete old windows if they exist
    if (chat_win_) {
        delwin(chat_win_);
        chat_win_ = nullptr;
    }
    if (input_win_) {
        delwin(input_win_);
        input_win_ = nullptr;
    }
    
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
    
    // Ensure display is updated
    doupdate();
}

void ChatUI::clear_chat_windows() {
    std::cerr << "[DEBUG] clear_chat_windows called\n" << std::flush;
    std::lock_guard<std::mutex> lock(screen_mutex_);
    
    // Delete chat windows if they exist
    if (chat_win_) {
        delwin(chat_win_);
        chat_win_ = nullptr;
        std::cerr << "[DEBUG] Deleted chat_win_\n" << std::flush;
    }
    if (input_win_) {
        delwin(input_win_);
        input_win_ = nullptr;
        std::cerr << "[DEBUG] Deleted input_win_\n" << std::flush;
    }
    
    // Clear screen and force update
    clear();
    refresh();
    
    // Clear messages
    chat_lines_.clear();
}

void ChatUI::add_chat_line(const std::string& line) {
    split_and_add_lines(line);
    refresh_chat_window();
}

void ChatUI::run_input_loop() {
    CommandParser parser(event_bus_);
    std::string input_buffer;
    int ch;
    
    // Mark that we're in the input loop
    in_input_loop_ = true;
    
    // Set input window to non-blocking mode with timeout
    wtimeout(input_win_, 100); // 100ms timeout
    
    while (running_ && current_state_ == UIState::CHATROOM) {
        {
            std::lock_guard<std::mutex> lock(screen_mutex_);
            wmove(input_win_, 1, 1);
            wclrtoeol(input_win_);
            mvwprintw(input_win_, 1, 1, "%s", input_buffer.c_str());
            box(input_win_, 0, 0);
            mvwprintw(input_win_, 0, 2, " Input (/leave, /quit) ");
            wrefresh(input_win_);
        }
        
        ch = wgetch(input_win_);
        
        // ERR means timeout, just continue to check state
        if (ch == ERR) {
            continue;
        }
        
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == 10 || ch == 13) {
            if (!input_buffer.empty()) {
                // Raise command_submitted event with the text
                Event event(EventType::COMMAND_SUBMITTED);
                event.set("text", input_buffer);
                event_bus_.publish(event);
                
                // Let command parser handle it
                parser.parse_and_execute(input_buffer);
                
                // Clear input buffer
                input_buffer.clear();
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!input_buffer.empty()) {
                input_buffer.pop_back();
            }
        } else if (ch >= 32 && ch <= 126) {
            input_buffer += static_cast<char>(ch);
        } else if (ch == 27) {
            Event event(EventType::APP_KILLED);
            event_bus_.publish(event);
            break;
        }
    }
    
    // Clean up chat windows before transitioning
    clear_chat_windows();
    
    // If we exited because state changed to FOYER, show the foyer
    if (current_state_ == UIState::FOYER) {
        show_foyer_screen();
    }
    
    // Mark that we're no longer in the input loop - do this LAST to avoid race condition
    std::cerr << "[DEBUG] Cleared in_input_loop flag\n" << std::flush;
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
    if (!isendwin()) {
        endwin();
    }
    // Reset terminal to normal mode
    std::cout << "\033[?1049l"; // Exit alternate screen
    std::cout.flush();
}
