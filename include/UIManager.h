#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "ThreadSafeQueue.h"
#include "UICommand.h"
#include "RoomInfo.h"
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
    
    // ncurses windows
    WINDOW* chat_win_;
    WINDOW* input_win_;
    WINDOW* status_win_;
    
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
    void setup_chat_windows();
    void clear_windows();
    void show_error_popup(const std::string& message);

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
