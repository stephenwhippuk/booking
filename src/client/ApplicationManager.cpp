#include "ApplicationManager.h"
#include "NetworkManager.h"
#include "auth/AuthClient.h"
#include "common/NetworkMessage.h"
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
    NetworkManager* network_manager,
    const std::string& auth_host,
    int auth_port,
    const std::string& chat_host,
    int chat_port)
    : network_inbound_(network_inbound)
    , network_outbound_(network_outbound)
    , ui_commands_(ui_commands)
    , input_events_(input_events)
    , network_manager_(network_manager)
    , auth_host_(auth_host)
    , auth_port_(auth_port)
    , chat_host_(chat_host)
    , chat_port_(chat_port)
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
    // Handle connection errors
    if (message == "SERVER_DISCONNECTED\n" || message == "CONNECTION_ERROR\n") {
        state_.set_connected(false);
        state_.set_screen(ApplicationState::Screen::LOGIN);
        ui_commands_.push(UICommand(UICommandType::SHOW_LOGIN));
        ui_commands_.push(UICommand(UICommandType::SHOW_ERROR, 
            ErrorData{"Connection lost"}));
        return;
    }
    
    // Parse JSON message
    auto net_msg = NetworkMessage::deserialize(message);
    
    if (net_msg.body.type == "ERROR") {
        std::string error_msg = net_msg.body.data.value("message", "Unknown error");
        ui_commands_.push(UICommand(UICommandType::SHOW_ERROR, ErrorData{error_msg}));
        return;
    }
    
    if (net_msg.body.type == "ROOM_JOINED") {
        in_room_ = true;
        std::string room_name = net_msg.body.data.value("room_name", "");
        state_.set_current_room(room_name);
        state_.set_screen(ApplicationState::Screen::CHATROOM);
        state_.clear_chat_messages();
        ui_commands_.push(UICommand(UICommandType::SHOW_CHATROOM, room_name));
    }
    else if (net_msg.body.type == "LEFT_ROOM") {
        in_room_ = false;
        state_.set_current_room("");
        state_.clear_chat_messages();
    }
    else if (net_msg.body.type == "ROOM_LIST") {
        std::vector<RoomInfo> rooms;
        if (net_msg.body.data.contains("rooms") && net_msg.body.data["rooms"].is_array()) {
            for (const auto& room_name : net_msg.body.data["rooms"]) {
                RoomInfo info;
                info.name = room_name.get<std::string>();
                info.client_count = 0;  // Server can optionally provide this
                rooms.push_back(info);
            }
        }
        state_.set_rooms(rooms);
        
        // Only show foyer if not in a room
        if (!in_room_) {
            state_.set_screen(ApplicationState::Screen::FOYER);
            ui_commands_.push(UICommand(UICommandType::SHOW_FOYER, state_.get_username()));
            ui_commands_.push(UICommand(UICommandType::UPDATE_ROOM_LIST, 
                RoomListData{rooms}));
        }
    }
    else if (net_msg.body.type == "MESSAGE") {
        std::string sender = net_msg.body.data.value("sender", "Unknown");
        std::string msg_text = net_msg.body.data.value("message", "");
        std::string formatted = "[" + sender + "] " + msg_text;
        
        state_.add_chat_message(formatted);
        ui_commands_.push(UICommand(UICommandType::ADD_CHAT_MESSAGE, 
            ChatMessageData{formatted}));
    }
    else if (net_msg.body.type == "PARTICIPANT_LIST") {
        std::vector<std::string> participants;
        if (net_msg.body.data.contains("participants") && net_msg.body.data["participants"].is_array()) {
            participants = net_msg.body.data["participants"].get<std::vector<std::string>>();
        }
        
        ui_commands_.push(UICommand(UICommandType::UPDATE_PARTICIPANTS,
            ParticipantsData{participants}));
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
        AuthClient auth_client(auth_host_, auth_port_);
        AuthResult auth_result = auth_client.authenticate(username, password);
        
        if (!auth_result.success) {
            ui_commands_.push(UICommand(UICommandType::SHOW_ERROR,
                ErrorData{"Login failed: " + auth_result.error_message}));
            return;
        }
        
        // Connect to chat server now that we have a token
        if (network_manager_) {
            std::string connect_error;
            if (!network_manager_->connect(chat_host_, chat_port_, connect_error)) {
                ui_commands_.push(UICommand(UICommandType::SHOW_ERROR,
                    ErrorData{"Failed to connect to chat server: " + connect_error}));
                return;
            }
            
            // Send token immediately (before starting network thread)
            // Server expects token as the first message
            auto auth_msg = NetworkMessage::create_auth(auth_result.token);
            std::string token_msg = auth_msg.serialize();
            
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
        auto msg = NetworkMessage::create_join_room(token, event_data);
        network_outbound_.push(msg.serialize());
    }
    else if (event_type == "CREATE_ROOM") {
        // CREATE_ROOM:room_name
        std::string token = state_.get_token();
        auto msg = NetworkMessage::create_create_room(token, event_data);
        network_outbound_.push(msg.serialize());
    }
    else if (event_type == "LEAVE") {
        std::string token = state_.get_token();
        auto msg = NetworkMessage::create_leave(token);
        network_outbound_.push(msg.serialize());
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
        
        // Send to server with token
        std::string token = state_.get_token();
        auto msg = NetworkMessage::create_chat_message(token, event_data);
        network_outbound_.push(msg.serialize());
    }
}
