#include "ClientManager.h"
#include "auth/AuthClient.h"
#include "common/NetworkMessage.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <sstream>
#include <chrono>

constexpr int BUFFER_SIZE = 4096;

ClientManager::ClientManager() {
    // Create a default "General" room
    chat_rooms_["General"] = std::make_shared<ChatRoom>("General");
}

ClientManager::~ClientManager() {}

void ClientManager::remove_client(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    connected_clients_.erase(
        std::remove_if(connected_clients_.begin(), connected_clients_.end(),
            [client_fd](const ClientInfo& c) { return c.fd == client_fd; }),
        connected_clients_.end()
    );
}

std::string ClientManager::get_client_name(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = std::find_if(connected_clients_.begin(), connected_clients_.end(),
        [client_fd](const ClientInfo& c) { return c.fd == client_fd; });
    if (it != connected_clients_.end()) {
        return it->name;
    }
    return "Unknown";
}

bool ClientManager::validate_token(const std::string& token) {
    auto now = std::chrono::steady_clock::now();
    
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(token_cache_mutex_);
        auto it = token_cache_.find(token);
        if (it != token_cache_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
            if (elapsed < TOKEN_CACHE_SECONDS) {
                // Token recently validated, trust cache
                return true;
            }
        }
    }
    
    // Validate with auth server
    AuthClient auth_client("localhost", 3001);
    bool valid = auth_client.validate_token(token);
    
    if (valid) {
        // Update cache
        std::lock_guard<std::mutex> lock(token_cache_mutex_);
        token_cache_[token] = now;
    }
    
    return valid;
}

std::string ClientManager::request_client_name(int client_fd, const std::string& client_ip) {
    // Client should send: JSON AUTH message
    char buffer[4096];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes <= 0) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "Client from " << client_ip << " disconnected before providing token\n";
        return "";
    }
    
    buffer[bytes] = '\0';
    std::string message = buffer;
    
    // Parse JSON message
    auto net_msg = NetworkMessage::deserialize(message);
    
    if (net_msg.body.type != "AUTH") {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "Client from " << client_ip << " sent invalid message (expected AUTH)\n";
        auto error_msg = NetworkMessage::create_error("Expected AUTH message");
        std::string error_str = error_msg.serialize();
        send(client_fd, error_str.c_str(), error_str.length(), 0);
        return "";
    }
    
    std::string token = net_msg.header.token;
    
    // Validate token with auth server
    AuthClient auth_client("localhost", 3001);
    auto user_info = auth_client.get_user_info(token);
    
    if (!user_info) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "Client from " << client_ip << " provided invalid token\n";
        const char* error_msg = "ERROR: Invalid or expired token\n";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return "";
    }
    
    // Store token in client_info for later validation
    // (caller needs to handle this)
    
    // Success - return display name
    return user_info->display_name;
}

void ClientManager::send_room_list(int client_fd) {
    std::lock_guard<std::mutex> lock(rooms_mutex_);
    std::vector<std::string> room_names;
    for (const auto& [name, room] : chat_rooms_) {
        room_names.push_back(name);
    }
    
    auto msg = NetworkMessage::create_room_list(room_names);
    std::string msg_str = msg.serialize();
    send(client_fd, msg_str.c_str(), msg_str.length(), 0);
}

void ClientManager::broadcast_room_list_to_foyer() {
    // Send room list update to all clients in foyer (not in a room)
    std::vector<int> foyer_clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& client : connected_clients_) {
            if (client.current_room.empty()) {
                foyer_clients.push_back(client.fd);
            }
        }
    }
    
    for (int fd : foyer_clients) {
        send_room_list(fd);
    }
}

void ClientManager::broadcast_member_list_to_room(const std::string& room_name) {
    std::shared_ptr<ChatRoom> room;
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        auto it = chat_rooms_.find(room_name);
        if (it == chat_rooms_.end()) {
            return;
        }
        room = it->second;
    }
    
    // Get list of members
    auto members = room->get_client_names();
    
    // Create JSON message
    auto msg = NetworkMessage::create_participant_list(members);
    std::string msg_str = msg.serialize();
    
    // Send to all clients in the room
    auto client_fds = room->get_client_fds();
    for (int fd : client_fds) {
        send(fd, msg_str.c_str(), msg_str.length(), MSG_NOSIGNAL);
    }
}

bool ClientManager::create_room(const std::string& room_name) {
    std::lock_guard<std::mutex> lock(rooms_mutex_);
    if (chat_rooms_.find(room_name) != chat_rooms_.end()) {
        return false;
    }
    chat_rooms_[room_name] = std::make_shared<ChatRoom>(room_name);
    return true;
}

bool ClientManager::join_room(int client_fd, ClientInfo& client_info, const std::string& room_name) {
    std::shared_ptr<ChatRoom> room;
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        auto it = chat_rooms_.find(room_name);
        if (it == chat_rooms_.end()) {
            return false;
        }
        room = it->second;
    }
    
    client_info.current_room = room_name;
    room->add_client(client_fd, client_info.name, client_info.ip);
    
    // Broadcast join notification
    auto join_notification = NetworkMessage::create_broadcast_message("SERVER", client_info.name + " joined the room");
    std::string join_str = join_notification.serialize();
    room->broadcast_message(join_str, client_fd);
    
    room->send_history_to_client(client_fd);
    
    // Send success response
    auto success_msg = NetworkMessage::create_room_joined(room_name);
    std::string success_str = success_msg.serialize();
    send(client_fd, success_str.c_str(), success_str.length(), 0);
    
    // Notify foyer clients of room count change
    broadcast_room_list_to_foyer();
    
    // Broadcast updated member list to all in room
    broadcast_member_list_to_room(room_name);
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << client_info.name << " (" << client_info.ip << ") joined room: " << room_name << "\n";
    }
    
    return true;
}

void ClientManager::leave_room(int client_fd, ClientInfo& client_info) {
    if (client_info.current_room.empty()) {
        return;
    }
    
    std::shared_ptr<ChatRoom> room;
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        auto it = chat_rooms_.find(client_info.current_room);
        if (it != chat_rooms_.end()) {
            room = it->second;
        }
    }
    
    if (room) {
        // Broadcast leave notification
        auto leave_notification = NetworkMessage::create_broadcast_message("SERVER", client_info.name + " left the room");
        std::string leave_str = leave_notification.serialize();
        room->broadcast_message(leave_str, client_fd);
        room->remove_client(client_fd);
        
        {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cout << client_info.name << " (" << client_info.ip << ") left room: " << client_info.current_room << "\n";
        }
    }
    
    client_info.current_room.clear();
    auto left_msg = NetworkMessage::create_error("Left room");  // Using error type with message
    left_msg.body.type = "LEFT_ROOM";
    std::string left_str = left_msg.serialize();
    send(client_fd, left_str.c_str(), left_str.length(), 0);
    
    // Notify foyer clients of room count change
    broadcast_room_list_to_foyer();
}

void ClientManager::handle_foyer(int client_fd, ClientInfo& client_info) {
    send_room_list(client_fd);
    
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            return;
        }
        
        buffer[bytes_read] = '\0';
        std::string message(buffer);
        
        // Parse JSON message
        auto net_msg = NetworkMessage::deserialize(message);
        
        // Validate token
        if (!validate_token(net_msg.header.token)) {
            auto error_msg = NetworkMessage::create_error("Invalid or expired token");
            std::string error_str = error_msg.serialize();
            send(client_fd, error_str.c_str(), error_str.length(), 0);
            return;
        }
        
        if (net_msg.body.type == "CREATE_ROOM") {
            std::string room_name = net_msg.body.data.value("room_name", "");
            if (create_room(room_name)) {
                // Auto-join the creator to the new room
                if (join_room(client_fd, client_info, room_name)) {
                    // Notify all foyer clients about the new room
                    broadcast_room_list_to_foyer();
                    
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    std::cout << client_info.name << " created and joined room: " << room_name << "\n";
                    return;
                }
            } else {
                auto error_msg = NetworkMessage::create_error("Room already exists");
                std::string error_str = error_msg.serialize();
                send(client_fd, error_str.c_str(), error_str.length(), 0);
            }
        } else if (net_msg.body.type == "JOIN_ROOM") {
            std::string room_name = net_msg.body.data.value("room_name", "");
            if (join_room(client_fd, client_info, room_name)) {
                return;
            } else {
                auto error_msg = NetworkMessage::create_error("Room not found");
                std::string error_str = error_msg.serialize();
                send(client_fd, error_str.c_str(), error_str.length(), 0);
            }
        } else if (net_msg.body.type == "REFRESH_ROOMS") {
            send_room_list(client_fd);
        } else if (net_msg.body.type == "QUIT") {
            return;
        }
    }
}

void ClientManager::handle_room_chat(int client_fd, ClientInfo& client_info) {
    std::shared_ptr<ChatRoom> room;
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        auto it = chat_rooms_.find(client_info.current_room);
        if (it != chat_rooms_.end()) {
            room = it->second;
        }
    }
    
    if (!room) {
        return;
    }
    
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            return;
        }
        
        buffer[bytes_read] = '\0';
        std::string msg_str(buffer);
        
        // Parse JSON message
        auto net_msg = NetworkMessage::deserialize(msg_str);
        
        // Validate token
        if (!validate_token(net_msg.header.token)) {
            auto error_msg = NetworkMessage::create_error("Invalid or expired token");
            std::string error_str = error_msg.serialize();
            send(client_fd, error_str.c_str(), error_str.length(), 0);
            return;
        }
        
        if (net_msg.body.type == "LEAVE") {
            leave_room(client_fd, client_info);
            return;
        } else if (net_msg.body.type == "QUIT") {
            leave_room(client_fd, client_info);
            auto quit_msg = NetworkMessage::create_error("Disconnected");
            std::string quit_str = quit_msg.serialize();
            send(client_fd, quit_str.c_str(), quit_str.length(), 0);
            return;
        } else if (net_msg.body.type == "CHAT_MESSAGE") {
            std::string message = net_msg.body.data.value("message", "");
            
            std::string display_name = client_info.name;
            {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cout << "[" << client_info.current_room << "] [" << display_name << "] " << message << "\n";
                std::cout.flush();
            }
            
            // Broadcast as JSON message
            auto broadcast_msg = NetworkMessage::create_broadcast_message(display_name, message);
            std::string broadcast_str = broadcast_msg.serialize();
            room->broadcast_message(broadcast_str, client_fd);
        }
    }
}

void ClientManager::handle_client(int client_fd, const std::string& client_ip) {
    // Receive token first
    char token_buffer[512];
    ssize_t token_bytes = recv(client_fd, token_buffer, sizeof(token_buffer) - 1, 0);
    
    if (token_bytes <= 0) {
        close(client_fd);
        return;
    }
    
    token_buffer[token_bytes] = '\0';
    std::string token_message = token_buffer;
    
    // Parse JSON message
    auto net_msg = NetworkMessage::deserialize(token_message);
    
    if (net_msg.body.type != "AUTH") {
        close(client_fd);
        return;
    }
    
    std::string token = net_msg.header.token;
    
    // Validate token
    AuthClient auth_client("localhost", 3001);
    auto user_info = auth_client.get_user_info(token);
    
    if (!user_info) {
        auto error_msg = NetworkMessage::create_error("Invalid or expired token");
        std::string error_str = error_msg.serialize();
        send(client_fd, error_str.c_str(), error_str.length(), 0);
        close(client_fd);
        return;
    }
    
    std::string client_name = user_info->display_name;
    
    ClientInfo client_info{client_fd, client_name, client_ip, "", token};
    
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        connected_clients_.push_back(client_info);
    }
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "Client connected: " << client_name << " (" << client_ip << ")\n";
    }
    
    // Main loop: alternate between foyer and room
    while (true) {
        handle_foyer(client_fd, client_info);
        
        if (client_info.current_room.empty()) {
            break;
        }
        
        handle_room_chat(client_fd, client_info);
        
        if (client_info.current_room.empty()) {
            continue;
        } else {
            break;
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << client_name << " (" << client_ip << ") disconnected\n";
    }
    
    remove_client(client_fd);
    close(client_fd);
}
