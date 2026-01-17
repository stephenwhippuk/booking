#include "ApplicationManager.h"
#include <sstream>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

ApplicationManager::ApplicationManager(
    ThreadSafeQueue<std::string>& network_inbound,
    ThreadSafeQueue<std::string>& network_outbound,
    ThreadSafeQueue<UICommand>& ui_commands,
    ThreadSafeQueue<std::string>& input_events)
    : network_inbound_(network_inbound)
    , network_outbound_(network_outbound)
    , ui_commands_(ui_commands)
    , input_events_(input_events)
    , running_(false)
    , in_room_(false)
{
}

ApplicationManager::~ApplicationManager() {
    stop();
}

void ApplicationManager::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    app_thread_ = std::thread(&ApplicationManager::application_loop, this);
}

void ApplicationManager::stop() {
    running_ = false;
    
    if (app_thread_.joinable()) {
        app_thread_.join();
    }
}

bool ApplicationManager::is_running() const {
    return running_;
}

const ApplicationState& ApplicationManager::get_state() const {
    return state_;
}

void ApplicationManager::application_loop() {
    while (running_) {
        // Process network messages
        std::string net_msg;
        if (network_inbound_.try_pop(net_msg, 10ms)) {
            process_network_message(net_msg);
        }
        
        // Process input events
        std::string input_event;
        if (input_events_.try_pop(input_event, 10ms)) {
            process_input_event(input_event);
        }
    }
}

std::vector<RoomInfo> ApplicationManager::parse_room_list(const std::string& data) {
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

bool ApplicationManager::is_in_room() const {
    return in_room_;
}

void ApplicationManager::process_network_message(const std::string& message) {
    // DEBUG: Log all received messages
    FILE* debug = fopen("/tmp/client_messages.log", "a");
    if (debug) {
        fprintf(debug, "[CLIENT] Received message: '%s'\n", message.c_str());
        fclose(debug);
    }
    
    // Handle connection errors
    if (message == "SERVER_DISCONNECTED\n" || message == "CONNECTION_ERROR\n") {
        state_.set_connected(false);
        state_.set_screen(ApplicationState::Screen::LOGIN);
        ui_commands_.push(UICommand(UICommandType::SHOW_LOGIN));
        ui_commands_.push(UICommand(UICommandType::SHOW_ERROR, 
            ErrorData{"Connection lost"}));
        return;
    }
    
    // Process state changes BEFORE ROOM_LIST
    if (message.find("JOINED_ROOM") != std::string::npos) {
        in_room_ = true;
        
        // Parse room name if present
        size_t pos = message.find("JOINED_ROOM:");
        std::string room_name;
        if (pos != std::string::npos) {
            size_t start = pos + 12;  // Length of "JOINED_ROOM:"
            size_t end = message.find('\n', start);
            if (end != std::string::npos) {
                room_name = message.substr(start, end - start);
                state_.set_current_room(room_name);
            }
        }
        
        state_.set_screen(ApplicationState::Screen::CHATROOM);
        state_.clear_chat_messages();
        ui_commands_.push(UICommand(UICommandType::SHOW_CHATROOM, room_name));
    }
    
    if (message.find("LEFT_ROOM") != std::string::npos) {
        in_room_ = false;
        state_.set_current_room("");
        state_.clear_chat_messages();
        // Don't transition to foyer yet - wait for ROOM_LIST
    }
    
    // Handle room list
    if (message.find("ROOM_LIST") != std::string::npos) {
        auto rooms = parse_room_list(message);
        state_.set_rooms(rooms);
        
        // Only show foyer if not in a room
        if (!in_room_) {
            state_.set_screen(ApplicationState::Screen::FOYER);
            ui_commands_.push(UICommand(UICommandType::SHOW_FOYER));
            ui_commands_.push(UICommand(UICommandType::UPDATE_ROOM_LIST, 
                RoomListData{rooms}));
        }
    }
    
    // Handle chat messages
    if (message.find("CHAT:") != std::string::npos) {
        size_t pos = message.find("CHAT:");
        if (pos != std::string::npos) {
            size_t start = pos + 5;  // Length of "CHAT:"
            size_t end = message.find('\n', start);
            if (end != std::string::npos) {
                std::string chat_msg = message.substr(start, end - start);
                state_.add_chat_message(chat_msg);
                ui_commands_.push(UICommand(UICommandType::ADD_CHAT_MESSAGE, 
                    ChatMessageData{chat_msg}));
            }
        }
    }
    
    // Handle broadcast messages (from server) - find ALL occurrences
    size_t pos = 0;
    while ((pos = message.find("BROADCAST:", pos)) != std::string::npos) {
        size_t start = pos + 10;  // Length of "BROADCAST:"
        size_t end = message.find('\n', start);
        if (end != std::string::npos) {
            std::string chat_msg = message.substr(start, end - start);
            state_.add_chat_message(chat_msg);
            ui_commands_.push(UICommand(UICommandType::ADD_CHAT_MESSAGE, 
                ChatMessageData{chat_msg}));
            pos = end + 1;  // Move past this message
        } else {
            break;  // No complete message found
        }
    }
    
    // Handle member list
    if (message.find("MEMBER_LIST:") != std::string::npos) {
        size_t member_pos = message.find("MEMBER_LIST:");
        size_t start = member_pos + 12;  // Length of "MEMBER_LIST:"
        size_t end = message.find('\n', start);
        if (end != std::string::npos) {
            std::string member_data = message.substr(start, end - start);
            
            FILE* debug = fopen("/tmp/client_messages.log", "a");
            if (debug) {
                fprintf(debug, "[CLIENT] Received MEMBER_LIST, raw data: '%s'\n", member_data.c_str());
            }
            
            // Parse comma-separated list
            std::vector<std::string> participants;
            std::stringstream ss(member_data);
            std::string name;
            while (std::getline(ss, name, ',')) {
                if (!name.empty()) {
                    participants.push_back(name);
                }
            }
            
            if (debug) {
                fprintf(debug, "[CLIENT] Parsed %zu participants: ", participants.size());
                for (const auto& p : participants) {
                    fprintf(debug, "'%s' ", p.c_str());
                }
                fprintf(debug, "\n");
                fclose(debug);
            }
            
            ui_commands_.push(UICommand(UICommandType::UPDATE_PARTICIPANTS,
                ParticipantsData{participants}));
        }
    }
    
    // Handle errors
    if (message.find("ROOM_EXISTS") != std::string::npos) {
        ui_commands_.push(UICommand(UICommandType::SHOW_ERROR, 
            ErrorData{"Room already exists"}));
    }
    
    if (message.find("ROOM_NOT_FOUND") != std::string::npos) {
        ui_commands_.push(UICommand(UICommandType::SHOW_ERROR, 
            ErrorData{"Room not found"}));
    }
}

void ApplicationManager::process_input_event(const std::string& event) {
    // Parse input event format: "EVENT_TYPE:data"
    size_t colon_pos = event.find(':');
    std::string event_type = (colon_pos != std::string::npos) 
        ? event.substr(0, colon_pos) 
        : event;
    std::string event_data = (colon_pos != std::string::npos) 
        ? event.substr(colon_pos + 1) 
        : "";
    
    if (event_type == "LOGIN") {
        // LOGIN:username
        state_.set_username(event_data);
        state_.set_connected(true);
        network_outbound_.push(event_data + "\n");
        
        // Wait for server's PROVIDE_NAME response handled in network messages
        // Then wait for ROOM_LIST which will trigger SHOW_FOYER
    }
    else if (event_type == "ROOM_SELECTED") {
        // ROOM_SELECTED:room_name
        network_outbound_.push("JOIN_ROOM:" + event_data + "\n");
    }
    else if (event_type == "CREATE_ROOM") {
        // CREATE_ROOM:room_name
        network_outbound_.push("CREATE_ROOM:" + event_data + "\n");
    }
    else if (event_type == "LEAVE") {
        network_outbound_.push("/leave\n");
    }
    else if (event_type == "LOGOUT") {
        network_outbound_.push("/logout\n");
        state_.set_connected(false);
        state_.reset();
        ui_commands_.push(UICommand(UICommandType::SHOW_LOGIN));
    }
    else if (event_type == "QUIT") {
        running_ = false;
        ui_commands_.push(UICommand(UICommandType::QUIT));
    }
    else if (event_type == "CHAT_MESSAGE") {
        // CHAT_MESSAGE:message text
        // Format our own message with [You] prefix
        std::string formatted_msg = "[You] " + event_data;
        
        // Add to local chat immediately (server won't echo back to us)
        state_.add_chat_message(formatted_msg);
        ui_commands_.push(UICommand(UICommandType::ADD_CHAT_MESSAGE, 
            ChatMessageData{formatted_msg}));
        
        // Send to server
        network_outbound_.push(event_data + "\n");
    }
}
