#include "UIManager.h"
#include <chrono>
#include <algorithm>
#include <iostream>

using namespace std::chrono_literals;

UIManager::UIManager(ThreadSafeQueue<UICommand>& ui_commands,
                     ThreadSafeQueue<std::string>& input_events)
    : ui_commands_(ui_commands)
    , input_events_(input_events)
    , current_screen_(Screen::LOGIN)
    , selected_room_index_(0)
    , main_window_(nullptr)
    , login_input_(nullptr)
    , password_input_(nullptr)
    , room_menu_(nullptr)
    , chat_input_(nullptr)
    , chat_display_(nullptr)
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
    
    // Clean up UI components
    login_input_.reset();
    password_input_.reset();
    room_menu_.reset();
    chat_input_.reset();
    chat_display_.reset();
    help_label_.reset();
    title_label_.reset();
    main_window_.reset();
    
    endwin();
    ncurses_initialized_ = false;
}

void UIManager::setup_login_ui() {
    FILE* debug = fopen("/tmp/setup_debug.log", "a");
    if (debug) {
        fprintf(debug, "setup_login_ui() starting\n");
        fclose(debug);
    }
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    debug = fopen("/tmp/setup_debug.log", "a");
    if (debug) {
        fprintf(debug, "Screen size: %dx%d\n", max_x, max_y);
        fclose(debug);
    }
    
    int y = max_y / 2 - 3;
    int x = max_x / 2 - 20;
    int width = 40;
    
    debug = fopen("/tmp/setup_debug.log", "a");
    if (debug) {
        fprintf(debug, "Creating window at (%d, %d) with width %d\n", x, y, width);
        fclose(debug);
    }
    
    // Create main window
    try {
        main_window_ = std::make_shared<ui::Window>(x, y, width, 10);
        debug = fopen("/tmp/setup_debug.log", "a");
        if (debug) {
            fprintf(debug, "Window created successfully: %p\n", main_window_.get());
            fclose(debug);
        }
    } catch (const std::exception& e) {
        debug = fopen("/tmp/setup_debug.log", "a");
        if (debug) {
            fprintf(debug, "Exception creating window: %s\n", e.what());
            fclose(debug);
        }
        return;
    }
    
    main_window_->set_bordered(true);
    main_window_->set_title(" Chat Client Login ");
    
    // Create text input for username (coordinates relative to window)
    login_input_ = std::make_shared<ui::TextInput>(2, 3, width - 4);
    login_input_->set_placeholder("Enter your name...");
    login_input_->set_label("Username:");
    login_input_->set_focus(true);
    
    // Create password input (coordinates relative to window)
    password_input_ = std::make_shared<ui::TextInput>(2, 5, width - 4);
    password_input_->set_placeholder("Enter password...");
    password_input_->set_label("Password:");
    password_input_->set_password_mode(true);
    password_input_->set_focus(false);
    
    // Add help label (coordinates relative to window)
    help_label_ = std::make_shared<ui::Label>(2, 7, "Tab to switch fields | Enter to login | 'q' to quit");
    help_label_->set_attributes(A_DIM);
    
    // Handle Enter key on username field - move to password
    login_input_->set_on_submit([this](const std::string& text) {
        if (!text.empty()) {
            password_input_->set_focus(true);
            login_input_->set_focus(false);
        }
    });
    
    // Handle Enter key on password field - attempt login
    password_input_->set_on_submit([this](const std::string& password) {
        std::string username = login_input_->get_text();
        if (!username.empty() && !password.empty()) {
            input_events_.push("LOGIN:" + username + ":" + password);
            username_ = username;
            login_input_->clear();
            password_input_->clear();
        }
    });
    
    main_window_->add_child(login_input_);
    main_window_->add_child(password_input_);
    main_window_->add_child(help_label_);
}

void UIManager::setup_foyer_ui() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create main window
    main_window_ = std::make_shared<ui::Window>(0, 0, max_x, max_y);
    main_window_->set_bordered(false);
    
    // Create title label
    title_label_ = std::make_shared<ui::Label>(0, 0, "+=== FOYER ===+");
    title_label_->set_attributes(A_BOLD);
    
    // Create welcome label
    auto welcome_label = std::make_shared<ui::Label>(0, 1, "Welcome, " + username_ + "!");
    
    // Create help label
    help_label_ = std::make_shared<ui::Label>(0, max_y - 2, max_x, 2,
        "Up/Down: Navigate | Enter: Join | c: Create Room\nq: Quit");
    help_label_->set_attributes(A_DIM);
    
    // Create room menu
    room_menu_ = std::make_shared<ui::Menu>(ui::Rect{2, 3, max_x - 4, max_y - 6});
    room_menu_->set_bordered(true);
    room_menu_->set_title(" Available Rooms ");
    room_menu_->set_numbered(false);
    room_menu_->set_focus(true);
    
    // Update menu when rooms change
    std::vector<ui::MenuItem> menu_items;
    for (const auto& room : rooms_) {
        ui::MenuItem item;
        item.text = room.name + " (" + std::to_string(room.client_count) + " users)";
        menu_items.push_back(item);
    }
    room_menu_->set_items(menu_items);
    
    // Handle room activation
    room_menu_->set_on_activate([this](size_t index, const ui::MenuItem& item) {
        if (index < rooms_.size()) {
            input_events_.push("ROOM_SELECTED:" + rooms_[index].name);
        }
    });
    
    main_window_->add_child(title_label_);
    main_window_->add_child(welcome_label);
    main_window_->add_child(room_menu_);
    main_window_->add_child(help_label_);
}

void UIManager::setup_chatroom_ui() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create main window
    main_window_ = std::make_shared<ui::Window>(0, 0, max_x, max_y);
    main_window_->set_bordered(false);
    
    // Member list width (right side)
    int member_list_width = 20;
    
    // Create chat display window (left side, leaving space for member list)
    chat_display_ = std::make_shared<ui::Window>(0, 0, max_x - member_list_width - 1, max_y - 3);
    chat_display_->set_bordered(true);
    chat_display_->set_title(" " + current_room_ + " ");
    
    // Create member list box (right side)
    member_list_box_ = std::make_shared<ui::ListBox>(max_x - member_list_width, 0, member_list_width, max_y - 3);
    member_list_box_->set_bordered(true);
    member_list_box_->set_title(" Members ");
    
    // Create chat input (coordinates relative to main window, full width)
    chat_input_ = std::make_shared<ui::TextInput>(1, max_y - 3, max_x - 2);
    chat_input_->set_label(">");
    chat_input_->set_focus(true);
    
    // Handle Enter key
    chat_input_->set_on_submit([this](const std::string& text) {
        if (!text.empty()) {
            if (text == "/leave") {
                input_events_.push("LEAVE");
            } else if (text == "/quit") {
                input_events_.push("QUIT");
            } else {
                input_events_.push("CHAT_MESSAGE:" + text);
            }
            chat_input_->clear();
        }
    });
    
    main_window_->add_child(chat_display_);
    main_window_->add_child(member_list_box_);
    main_window_->add_child(chat_input_);
}

void UIManager::run() {
    FILE* debug = fopen("/tmp/ui_run_debug.log", "a");
    if (debug) {
        fprintf(debug, "UIManager::run() started\n");
        fclose(debug);
    }
    
    init_ncurses();
    
    debug = fopen("/tmp/ui_run_debug.log", "a");
    if (debug) {
        fprintf(debug, "ncurses initialized\n");
        fclose(debug);
    }
    
    running_ = true;
    
    while (running_) {
        debug = fopen("/tmp/ui_run_debug.log", "a");
        if (debug) {
            fprintf(debug, "Loop iteration: current_screen_=%d\n", static_cast<int>(current_screen_));
            fclose(debug);
        }
        
        process_commands();
        poll_input();
        render();
        
        std::this_thread::sleep_for(100ms);  // ~10 FPS, reduces flicker
    }
    
    debug = fopen("/tmp/ui_run_debug.log", "a");
    if (debug) {
        fprintf(debug, "Exited main loop, cleaning up\n");
        fclose(debug);
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
                setup_login_ui();
                break;
                
            case UICommandType::SHOW_FOYER:
                current_screen_ = Screen::FOYER;
                selected_room_index_ = 0;
                input_buffer_.clear();
                setup_foyer_ui();
                break;
                
            case UICommandType::SHOW_CHATROOM:
                current_screen_ = Screen::CHATROOM;
                input_buffer_.clear();
                if (cmd.has_data()) {
                    current_room_ = cmd.get<std::string>();
                }
                setup_chatroom_ui();
                break;
                
            case UICommandType::UPDATE_ROOM_LIST:
                if (cmd.has_data()) {
                    rooms_ = cmd.get<RoomListData>().rooms;
                    // Keep selected index in bounds
                    if (selected_room_index_ >= static_cast<int>(rooms_.size())) {
                        selected_room_index_ = std::max(0, static_cast<int>(rooms_.size()) - 1);
                    }
                    // Update menu if it exists
                    if (room_menu_) {
                        std::vector<ui::MenuItem> menu_items;
                        for (const auto& room : rooms_) {
                            ui::MenuItem item;
                            item.text = room.name + " (" + std::to_string(room.client_count) + " users)";
                            menu_items.push_back(item);
                        }
                        room_menu_->set_items(menu_items);
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
                    FILE* debug = fopen("/tmp/ui_participants.log", "a");
                    if (debug) {
                        fprintf(debug, "[UI] UPDATE_PARTICIPANTS received, count: %zu\n", participants_.size());
                        for (const auto& p : participants_) {
                            fprintf(debug, "[UI]   - '%s'\n", p.c_str());
                        }
                        fclose(debug);
                    }
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
                if (login_input_) login_input_->clear();
                if (chat_input_) chat_input_->clear();
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
    
    // Create event for components
    ui::Event event;
    event.type = ui::EventType::KEY_PRESS;
    event.key = ch;
    
    // Handle quit on any screen
    if (ch == 'q' || ch == 'Q') {
        if (current_screen_ == Screen::LOGIN && login_input_ && login_input_->get_text().empty()) {
            input_events_.push("QUIT");
            return;
        } else if (current_screen_ == Screen::FOYER) {
            input_events_.push("QUIT");
            return;
        }
    }
    
    // Handle Tab key on login screen to switch between username and password
    if (ch == '\t' && current_screen_ == Screen::LOGIN) {
        if (main_window_ && login_input_ && password_input_) {
            if (login_input_->has_focus()) {
                main_window_->focus_child(password_input_);
            } else {
                main_window_->focus_child(login_input_);
            }
        }
        return;
    }
    
    // Handle create room in foyer
    if ((ch == 'c' || ch == 'C') && current_screen_ == Screen::FOYER) {
        show_create_room_dialog();
        return;
    }
    
    // Forward to main window (which forwards to focused child)
    if (main_window_) {
        main_window_->handle_event(event);
    }
}

void UIManager::render() {
    // Clear stdscr without refreshing
    werase(stdscr);
    
    switch (current_screen_) {
        case Screen::LOGIN:
            // For login, stage stdscr first so window appears on top
            wnoutrefresh(stdscr);
            render_login();
            break;
        case Screen::FOYER:
            render_foyer();
            // For foyer, stage stdscr after children have drawn to it
            wnoutrefresh(stdscr);
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
    if (!main_window_) {
        setup_login_ui();
    }
    
    // Render the UI components
    if (main_window_) {
        WINDOW* win = main_window_->get_window();
        
        if (win && win != stdscr) {
            werase(win);
            if (main_window_->is_bordered()) {
                box(win, 0, 0);
                std::string title = main_window_->get_title();
                if (!title.empty()) {
                    mvwprintw(win, 0, 2, "%s", title.c_str());
                }
            }
            
            // Manually render children
            if (login_input_) {
                login_input_->render(win);
            }
            if (password_input_) {
                password_input_->render(win);
            }
            if (help_label_) {
                help_label_->render(win);
            }
            
            // Position cursor BEFORE wnoutrefresh if TextInput is focused
            if (login_input_ && login_input_->has_focus()) {
                int cursor_x = login_input_->get_bounds().left() + 
                              login_input_->get_label().length() + 1 +
                              login_input_->get_cursor_pos();
                int cursor_y = login_input_->get_bounds().top();
                wmove(win, cursor_y, cursor_x);
                curs_set(1);  // Show cursor
            } else if (password_input_ && password_input_->has_focus()) {
                int cursor_x = password_input_->get_bounds().left() + 
                              password_input_->get_label().length() + 1 +
                              password_input_->get_cursor_pos();
                int cursor_y = password_input_->get_bounds().top();
                wmove(win, cursor_y, cursor_x);
                curs_set(1);  // Show cursor
            } else {
                curs_set(0);  // Hide cursor
            }
            
            // CRITICAL: Mark window as changed and move it to the front
            touchwin(win);
            wnoutrefresh(win);
        }
    }
}

void UIManager::render_foyer() {
    if (!main_window_) {
        setup_foyer_ui();
    }
    
    // Render all children directly to stdscr (main_window is fullscreen, no border)
    if (main_window_) {
        for (const auto& child : main_window_->get_children()) {
            if (!child || !child->is_visible()) continue;
            
            auto label = std::dynamic_pointer_cast<ui::Label>(child);
            if (label) {
                label->render(stdscr);
                continue;
            }
            
            auto menu = std::dynamic_pointer_cast<ui::Menu>(child);
            if (menu) {
                menu->render(stdscr);
                continue;
            }
        }
    }
}

void UIManager::render_chatroom() {
    if (!main_window_) {
        setup_chatroom_ui();
    }
    
    // Debug: Always show participant info
    mvprintw(0, 0, "P:%zu", participants_.size());
    if (!participants_.empty()) {
        for (size_t i = 0; i < std::min(participants_.size(), size_t(3)); ++i) {
            mvprintw(i + 1, 0, "%s", participants_[i].c_str());
        }
    }
    
    // Render chat input to stdscr first
    if (chat_input_) {
        chat_input_->render(stdscr);
    }
    
    // Render member list box BEFORE staging stdscr
    if (member_list_box_) {
        member_list_box_->set_items(participants_);
        member_list_box_->render(stdscr);
    }
    
    // Position cursor on stdscr BEFORE staging if TextInput is focused
    if (chat_input_ && chat_input_->has_focus()) {
        int cursor_x = chat_input_->get_bounds().left() + 
                      chat_input_->get_label().length() + 1 +
                      (chat_input_->get_cursor_pos() - chat_input_->get_scroll_offset());
        int cursor_y = chat_input_->get_bounds().top();
        wmove(stdscr, cursor_y, cursor_x);
        curs_set(1);  // Show cursor
    } else {
        curs_set(0);  // Hide cursor
    }
    
    // Stage stdscr as background
    wnoutrefresh(stdscr);
    
    // Render chat messages in chat_display window (overlays stdscr)
    if (chat_display_) {
        WINDOW* win = chat_display_->get_window();
        if (win) {
            werase(win);
            
            // Draw border
            box(win, 0, 0);
            if (!current_room_.empty()) {
                mvwprintw(win, 0, 2, " %s ", current_room_.c_str());
            }
            
            int chat_height, chat_width;
            getmaxyx(win, chat_height, chat_width);
            
            // Show last N messages that fit
            int start_idx = chat_messages_.size() > (size_t)(chat_height - 2)
                ? chat_messages_.size() - (chat_height - 2)
                : 0;
            
            int y = 1;
            for (size_t i = start_idx; i < chat_messages_.size() && y < chat_height - 1; ++i) {
                mvwprintw(win, y++, 1, "%.*s", 
                          chat_width - 2, 
                          chat_messages_[i].c_str());
            }
            
            touchwin(win);
            wnoutrefresh(win);
        }
    }
    
    // Render member list box (overlays stdscr on right side)
    if (member_list_box_) {
        member_list_box_->set_items(participants_);
        member_list_box_->render(stdscr);
    }
    
    // Position cursor AFTER all windows staged, then re-stage stdscr
    if (chat_input_ && chat_input_->has_focus()) {
        int cursor_x = chat_input_->get_bounds().left() + 
                      chat_input_->get_label().length() + 1 +
                      (chat_input_->get_cursor_pos() - chat_input_->get_scroll_offset());
        int cursor_y = chat_input_->get_bounds().top();
        wmove(stdscr, cursor_y, cursor_x);
        curs_set(1);  // Show cursor
        wnoutrefresh(stdscr);  // Re-stage stdscr with correct cursor position
    } else {
        curs_set(0);  // Hide cursor
    }
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

void UIManager::show_create_room_dialog() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int popup_height = 9;
    int popup_width = std::min(50, max_x - 4);
    int popup_y = (max_y - popup_height) / 2;
    int popup_x = (max_x - popup_width) / 2;
    
    WINDOW* popup = newwin(popup_height, popup_width, popup_y, popup_x);
    
    std::string room_name;
    bool done = false;
    
    nodelay(stdscr, FALSE);  // Blocking input for dialog
    curs_set(1);  // Show cursor
    
    while (!done) {
        werase(popup);
        box(popup, 0, 0);
        mvwprintw(popup, 0, 2, " Create New Room ");
        mvwprintw(popup, 2, 2, "Room name:");
        mvwprintw(popup, 3, 2, "%.*s", popup_width - 4, room_name.c_str());
        mvwprintw(popup, 5, 2, "Enter: Create | Esc: Cancel");
        
        // Position cursor
        wmove(popup, 3, 2 + room_name.length());
        wrefresh(popup);
        
        int ch = wgetch(popup);
        
        if (ch == 27) {  // ESC
            done = true;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (!room_name.empty()) {
                input_events_.push("CREATE_ROOM:" + room_name);
                done = true;
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!room_name.empty()) {
                room_name.pop_back();
            }
        } else if (ch >= 32 && ch < 127 && room_name.length() < 30) {
            room_name += static_cast<char>(ch);
        }
    }
    
    nodelay(stdscr, TRUE);  // Non-blocking again
    curs_set(0);  // Hide cursor
    delwin(popup);
    
    // Force full redraw
    clear();
    refresh();
}
