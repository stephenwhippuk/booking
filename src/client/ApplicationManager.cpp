#include "ApplicationManager.h"
#include "NetworkManager.h"
#include "auth/AuthClient.h"
#include <sstream>
#include <chrono>
#include <iostream>
#include <sys/socket.h>

using namespace std::chrono_literals;

ApplicationManager::ApplicationManager(
    ThreadSafeQueue<std::string>& network_inbound,
    ThreadSafeQueue<std::string>& network_outbound,
    ThreadSafeQueue<UICommand>& ui_commands,
    ThreadSafeQueue<std::string>& input_events,
    NetworkManager* network_manager)
    : network_inbound_(network_inbound)
    , network_outbound_(network_outbound)
    , ui_commands_(ui_commands)
    , input_events_(input_events)
    , network_manager_(network_manager)
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
            ui_commands_.push(UICommand(UICommandType::SHOW_FOYER, state_.get_username()));
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
        // LOGIN:username:password
        size_t second_colon = event_data.find(':');
        if (second_colon == std::string::npos) {
            ui_commands_.push(UICommand(UICommandType::SHOW_ERROR,
                ErrorData{"Invalid login format"}));
            return;
        }
        
        std::string username = event_data.substr(0, second_colon);
        std::string password = event_data.substr(second_colon + 1);
        
        // Authenticate with auth server
        AuthClient auth_client("localhost", 3001);
        AuthResult auth_result = auth_client.authenticate(username, password);
        
        if (!auth_result.success) {
            ui_commands_.push(UICommand(UICommandType::SHOW_ERROR,
                ErrorData{"Login failed: " + auth_result.error_message}));
            return;
        }
        
        // Connect to chat server now that we have a token
        if (network_manager_) {
            std::string connect_error;
            if (!network_manager_->connect("127.0.0.1", 3000, connect_error)) {
                ui_commands_.push(UICommand(UICommandType::SHOW_ERROR,
                    ErrorData{"Failed to connect to chat server: " + connect_error}));
                return;
            }
            
            // Send token immediately (before starting network thread)
            // Server expects token as the first message
            std::string token_msg = "TOKEN:" + auth_result.token + "\n";
            
            int sock = network_manager_->get_socket();
            ssize_t sent = send(sock, token_msg.c_str(), token_msg.length(), 0);
            if (sent <= 0) {
                ui_commands_.push(UICommand(UICommandType::SHOW_ERROR,
                    ErrorData{"Failed to send authentication token"}));
                return;
            }
            
            // Start network thread after token is sent
            network_manager_->start();
        }
        
        // Store token and display name
        state_.set_token(auth_result.token);
        state_.set_username(auth_result.display_name);  // Use display name for UI
        state_.set_connected(true);
        
        // Wait for server's response handled in network messages
        // Then wait for ROOM_LIST which will trigger SHOW_FOYER
    }
    else if (event_type == "ROOM_SELECTED") {
        // ROOM_SELECTED:room_name
        std::string token = state_.get_token();
        network_outbound_.push("TOKEN:" + token + "|JOIN_ROOM:" + event_data + "\n");
    }
    else if (event_type == "CREATE_ROOM") {
        // CREATE_ROOM:room_name
        std::string token = state_.get_token();
        network_outbound_.push("TOKEN:" + token + "|CREATE_ROOM:" + event_data + "\n");
    }
    else if (event_type == "LEAVE") {
        std::string token = state_.get_token();
        network_outbound_.push("TOKEN:" + token + "|/leave\n");
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
        
        // Send to server with token: TOKEN:<token>|<message>
        std::string token = state_.get_token();
        network_outbound_.push("TOKEN:" + token + "|" + event_data + "\n");
    }
}
