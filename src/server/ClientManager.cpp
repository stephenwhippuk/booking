#include "ClientManager.h"
#include "auth/AuthClient.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <sstream>

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

std::string ClientManager::request_client_name(int client_fd, const std::string& client_ip) {
    // Client should send: TOKEN:<token>
    char buffer[512];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes <= 0) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "Client from " << client_ip << " disconnected before providing token\n";
        return "";
    }
    
    buffer[bytes] = '\0';
    std::string message = buffer;
    if (!message.empty() && message.back() == '\n') {
        message.pop_back();
    }
    
    // Parse TOKEN:token format
    if (message.find("TOKEN:") != 0) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "Client from " << client_ip << " sent invalid message (expected TOKEN:...)\n";
        const char* error_msg = "ERROR: Expected TOKEN:<token>\n";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return "";
    }
    
    std::string token = message.substr(6);  // Skip "TOKEN:"
    
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
    
    // Success - return display name
    return user_info->display_name;
}

void ClientManager::send_room_list(int client_fd) {
    std::lock_guard<std::mutex> lock(rooms_mutex_);
    std::stringstream ss;
    ss << "ROOM_LIST\n";
    for (const auto& [name, room] : chat_rooms_) {
        ss << name << "|" << room->get_client_count() << "\n";
    }
    ss << "END_ROOM_LIST\n";
    std::string list = ss.str();
    send(client_fd, list.c_str(), list.length(), 0);
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
    
    // Build MEMBER_LIST message
    std::string member_list_msg = "MEMBER_LIST:";
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > 0) member_list_msg += ",";
        member_list_msg += members[i];
    }
    member_list_msg += "\n";
    
    // DEBUG: Log what we're broadcasting
    FILE* debug = fopen("/tmp/server_member_list.log", "a");
    if (debug) {
        fprintf(debug, "[SERVER] Broadcasting to room '%s': %s", room_name.c_str(), member_list_msg.c_str());
        fprintf(debug, "[SERVER] Found %zu members\n", members.size());
    }
    
    // Send to all clients in the room - get FDs directly from room
    auto client_fds = room->get_client_fds();
    for (int fd : client_fds) {
        ssize_t sent = send(fd, member_list_msg.c_str(), member_list_msg.length(), MSG_NOSIGNAL);
        if (debug) {
            fprintf(debug, "[SERVER] Sent to client fd=%d, result=%zd\n", fd, sent);
        }
    }
    
    if (debug) {
        fclose(debug);
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
    
    std::string join_msg = "[SERVER] " + client_info.name + " joined the room\n";
    room->broadcast_message(join_msg, client_fd);
    
    room->send_history_to_client(client_fd);
    
    std::string success = "JOINED_ROOM:" + room_name + "\n";
    send(client_fd, success.c_str(), success.length(), 0);
    
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
        std::string leave_msg = "[SERVER] " + client_info.name + " left the room\n";
        room->broadcast_message(leave_msg, client_fd);
        room->remove_client(client_fd);
        
        {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cout << client_info.name << " (" << client_info.ip << ") left room: " << client_info.current_room << "\n";
        }
    }
    
    client_info.current_room.clear();
    const char* msg = "LEFT_ROOM\n";
    send(client_fd, msg, strlen(msg), 0);
    
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
        std::string command(buffer);
        
        if (!command.empty() && command.back() == '\n') {
            command.pop_back();
        }
        
        if (command.rfind("CREATE_ROOM:", 0) == 0) {
            std::string room_name = command.substr(12);
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
                const char* error = "ROOM_EXISTS\n";
                send(client_fd, error, strlen(error), 0);
            }
        } else if (command.rfind("JOIN_ROOM:", 0) == 0) {
            std::string room_name = command.substr(10);
            if (join_room(client_fd, client_info, room_name)) {
                return;
            } else {
                const char* error = "ROOM_NOT_FOUND\n";
                send(client_fd, error, strlen(error), 0);
            }
        } else if (command == "REFRESH_ROOMS") {
            send_room_list(client_fd);
        } else if (command == "/quit") {
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
        std::string message(buffer);
        
        if (message == "/leave\n") {
            leave_room(client_fd, client_info);
            return;
        } else if (message == "/quit\n") {
            leave_room(client_fd, client_info);
            const char* quit_msg = "QUIT\n";
            send(client_fd, quit_msg, strlen(quit_msg), 0);
            return;
        }
        
        std::string display_name = client_info.name + " (" + client_info.ip + ")";
        {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cout << "[" << client_info.current_room << "] [" << display_name << "] " << message;
            std::cout.flush();
        }
        
        std::string chat_message = "[" + display_name + "] " + message;
        room->broadcast_message(chat_message, client_fd);
    }
}

void ClientManager::handle_client(int client_fd, const std::string& client_ip) {
    std::string client_name = request_client_name(client_fd, client_ip);
    
    if (client_name.empty()) {
        close(client_fd);
        return;
    }
    
    ClientInfo client_info{client_fd, client_name, client_ip, ""};
    
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
