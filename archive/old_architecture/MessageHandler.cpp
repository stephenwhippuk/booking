#include "MessageHandler.h"
#include <sstream>
#include <sys/socket.h>
#include <iostream>

MessageHandler::MessageHandler(ServerConnection& connection, EventBus& event_bus)
    : connection_(connection), event_bus_(event_bus), running_(false), in_room_(false) {
}

MessageHandler::~MessageHandler() {
    stop_listening();
}

std::vector<RoomInfo> MessageHandler::parse_room_list(const std::string& data) {
    std::vector<RoomInfo> rooms;
    std::istringstream stream(data);
    std::string line;
    bool started = false;
    
    while (std::getline(stream, line)) {
        if (line == "ROOM_LIST") {
            started = true;
            continue;
        }
        if (line == "END_ROOM_LIST") {
            // Stop after first END_ROOM_LIST to avoid parsing duplicate lists
            break;
        }
        if (line.empty()) {
            continue;
        }
        
        if (started) {
            size_t pipe_pos = line.find('|');
            if (pipe_pos != std::string::npos) {
                RoomInfo room;
                room.name = line.substr(0, pipe_pos);
                room.client_count = std::stoi(line.substr(pipe_pos + 1));
                rooms.push_back(room);
            }
        }
    }
    
    return rooms;
}

ServerResponse MessageHandler::parse_response(const std::string& data) {
    ServerResponse response;
    response.success = false;
    
    // Parse various response types
    if (data.find("ROOM_LIST") != std::string::npos) {
        response.success = true;
        response.rooms = parse_room_list(data);
    } else if (data.find("ROOM_CREATED") != std::string::npos) {
        response.success = true;
        response.message = "Room created successfully";
    } else if (data.find("ROOM_EXISTS") != std::string::npos) {
        response.success = false;
        response.message = "Room already exists";
    } else if (data.find("JOINED_ROOM") != std::string::npos) {
        response.success = true;
        // Parse room data, participants, history if present
    } else if (data.find("LEFT_ROOM") != std::string::npos) {
        response.success = true;
    }
    
    return response;
}

void MessageHandler::handle_login_submitted(const Event& event) {
    std::string username = event.get<std::string>("username");
    
    // Connect to server
    std::string error_msg;
    if (!connection_.connect_to_server("127.0.0.1", 3000, error_msg)) {
        Event kicked_event(EventType::KICKED);
        kicked_event.set("reason", error_msg);
        event_bus_.publish(kicked_event);
        return;
    }
    
    // Receive protocol message
    std::string protocol_msg;
    if (!connection_.receive_protocol_message(protocol_msg, error_msg)) {
        Event kicked_event(EventType::KICKED);
        kicked_event.set("reason", error_msg);
        event_bus_.publish(kicked_event);
        return;
    }
    
    if (protocol_msg == "PROVIDE_NAME\n") {
        connection_.send_message(username + '\n');
        
        // Login successful
        Event logged_in_event(EventType::LOGGED_IN);
        logged_in_event.set("username", username);
        event_bus_.publish(logged_in_event);
    }
}

void MessageHandler::handle_logged_in(const Event& event) {
    // Send join_foyer request - in this protocol, just wait for ROOM_LIST
    // The server automatically sends ROOM_LIST after login
}

void MessageHandler::handle_room_selected(const Event& event) {
    std::string room_name = event.get<std::string>("room_name");
    connection_.send_message("JOIN_ROOM:" + room_name + "\n");
}

void MessageHandler::handle_room_requested(const Event& event) {
    std::string room_name = event.get<std::string>("room_name");
    connection_.send_message("CREATE_ROOM:" + room_name + "\n");
}

void MessageHandler::handle_leave_requested(const Event& event) {
    connection_.send_message("/leave\n");
    // Don't raise LOGGED_IN immediately - let server's LEFT_ROOM + ROOM_LIST response handle it
}

void MessageHandler::handle_logout_requested(const Event& event) {
    connection_.send_message("/logout\n");
    
    // Assuming success for now
    Event logged_out_event(EventType::LOGGED_OUT);
    event_bus_.publish(logged_out_event);
}

void MessageHandler::handle_chat_line_submitted(const Event& event) {
    std::string message = event.get<std::string>("message");
    connection_.send_message(message + "\n");
}

void MessageHandler::process_server_message(const std::string& message) {
    // Process different server message types
    // Note: Multiple messages may arrive together, check all conditions
    // IMPORTANT: Process state changes (JOINED_ROOM, LEFT_ROOM) before ROOM_LIST
    //            so that in_room_ state is correct when ROOM_LIST is processed
    
    if (message.find("JOINED_ROOM") != std::string::npos) {
        in_room_ = true;
        Event room_joined_event(EventType::ROOM_JOINED);
        // Parse room details if available
        event_bus_.publish(room_joined_event);
    }
    
    if (message.find("LEFT_ROOM") != std::string::npos) {
        in_room_ = false;
        // Room left, ROOM_LIST will be processed and show foyer
    }
    
    if (message.find("ROOM_LIST") != std::string::npos) {
        auto rooms = parse_room_list(message);
        
        // Only transition to foyer if we're NOT in a room
        // If we're in a room, this is just a room count update
        if (!in_room_) {
            Event foyer_event(EventType::FOYER_JOINED);
            foyer_event.set("rooms", rooms);
            event_bus_.publish(foyer_event);
        }
    }
    
    if (message.find("BROADCAST:") != std::string::npos) {
        // Extract chat message
        size_t pos = message.find("BROADCAST:");
        std::string chat_line = message.substr(pos + 10);
        
        Event chat_event(EventType::CHAT_RECEIVED);
        chat_event.set("message", chat_line);
        event_bus_.publish(chat_event);
    }
    
    if (message.find("ROOM_CREATED") != std::string::npos && 
        message.find("JOINED_ROOM") != std::string::npos) {
        // Room created and joined
        Event room_joined_event(EventType::ROOM_JOINED);
        event_bus_.publish(room_joined_event);
    }
    
    if (message.find("ROOM_EXISTS") != std::string::npos) {
        // Failed to create room, stay in foyer
        // Don't raise an event, just let the user try again
    }
}

void MessageHandler::start_listening() {
    running_ = true;
    
    auto on_message = [this](const std::string& message) {
        process_server_message(message);
    };
    
    auto on_disconnect = [this]() {
        running_ = false;
        Event kicked_event(EventType::KICKED);
        kicked_event.set("reason", "Disconnected from server");
        event_bus_.publish(kicked_event);
    };
    
    connection_.start_receiving(on_message, on_disconnect);
}

void MessageHandler::stop_listening() {
    if (running_) {
        running_ = false;
        connection_.stop_receiving();
    }
}
