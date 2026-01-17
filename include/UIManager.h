#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "ThreadSafeQueue.h"
#include "UICommand.h"
#include "RoomInfo.h"
#include <ui/Window.h>
#include <ui/TextInput.h>
#include <ui/Menu.h>
#include <ui/Label.h>
#include <ui/ListBox.h>
#include <ncurses.h>
#include <string>
#include <vector>
#include <atomic>

/**
 * UIManager - Pure presentation layer
 * 
 * Responsibilities:
 * - Poll ui_commands queue for display updates
 * - Render screens using ncurses
 * - Handle non-blocking input
 * - Send input events to application
 * 
 * Does NOT:
 * - Parse protocols
 * - Manage application state
 * - Know about network layer
 */
class UIManager {
private:
    // Queue references
    ThreadSafeQueue<UICommand>& ui_commands_;
    ThreadSafeQueue<std::string>& input_events_;
    
    // UI state (local to UI thread)
    enum class Screen {
        LOGIN,
        FOYER,
        CHATROOM
    };
    Screen current_screen_;
    
    // UI-specific data (copies from commands)
    std::vector<RoomInfo> rooms_;
    std::vector<std::string> chat_messages_;
    std::vector<std::string> participants_;
    std::string current_room_;
    std::string username_;
    std::string status_message_;
    std::string error_message_;
    
    // Input state
    std::string input_buffer_;
    int selected_room_index_;
    
    // UI components
    ui::WindowPtr main_window_;
    ui::TextInputPtr login_input_;
    ui::MenuPtr room_menu_;
    ui::TextInputPtr chat_input_;
    ui::WindowPtr chat_display_;
    ui::ListBoxPtr member_list_box_;
    ui::LabelPtr help_label_;
    ui::LabelPtr title_label_;
    
    // Thread control
    std::atomic<bool> running_;
    bool ncurses_initialized_;
    
    // Internal methods
    void process_commands();
    void poll_input();
    void render();
    
    // Input handling per screen
    void handle_login_input(int ch);
    void handle_foyer_input(int ch);
    void handle_chatroom_input(int ch);
    
    // Rendering per screen
    void render_login();
    void render_foyer();
    void render_chatroom();
    
    // Helper methods
    void init_ncurses();
    void cleanup_ncurses();
    void setup_login_ui();
    void setup_foyer_ui();
    void setup_chatroom_ui();
    void show_error_popup(const std::string& message);
    void show_create_room_dialog();

public:
    UIManager(ThreadSafeQueue<UICommand>& ui_commands,
              ThreadSafeQueue<std::string>& input_events);
    
    ~UIManager();
    
    // Prevent copying
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    
    /**
     * Main UI loop - runs on main thread
     * Blocks until QUIT command received
     */
    void run();
    
    /**
     * Stop the UI (typically called from signal handler)
     */
    void stop();
};

#endif // UIMANAGER_H
