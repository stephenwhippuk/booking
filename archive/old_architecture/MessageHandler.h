#pragma once

#include "Event.h"
#include "ServerConnection.h"
#include "RoomInfo.h"
#include <string>
#include <vector>
#include <memory>

struct ServerResponse {
    bool success;
    std::string message;
    std::vector<RoomInfo> rooms;
    std::vector<std::string> participants;
    std::vector<std::string> chat_history;
};

// Handles all server communication and converts server messages to events
class MessageHandler {
private:
    ServerConnection& connection_;
    EventBus& event_bus_;
    bool running_;
    bool in_room_;  // Track whether client is currently in a chat room
    
    // Parse different server message types
    std::vector<RoomInfo> parse_room_list(const std::string& data);
    ServerResponse parse_response(const std::string& data);
    
public:
    MessageHandler(ServerConnection& connection, EventBus& event_bus);
    ~MessageHandler();
    
    // Send messages to server based on events
    void handle_login_submitted(const Event& event);
    void handle_logged_in(const Event& event);
    void handle_room_selected(const Event& event);
    void handle_room_requested(const Event& event);
    void handle_leave_requested(const Event& event);
    void handle_logout_requested(const Event& event);
    void handle_chat_line_submitted(const Event& event);
    
    // Process incoming server messages and raise events
    void process_server_message(const std::string& message);
    
    // Start listening for server push messages
    void start_listening();
    void stop_listening();
};
