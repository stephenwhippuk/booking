#include "ChatRoom.h"
#include <sys/socket.h>
#include <algorithm>
#include <iostream>

ChatRoom::ChatRoom(const std::string& name) : name_(name) {}

std::string ChatRoom::get_name() const {
    return name_;
}

size_t ChatRoom::get_client_count() const {
    std::lock_guard<std::mutex> lock(room_mutex_);
    return clients_.size();
}

void ChatRoom::add_client(int fd, const std::string& name, const std::string& ip) {
    std::lock_guard<std::mutex> lock(room_mutex_);
    clients_.push_back({fd, name, ip});
}

void ChatRoom::remove_client(int fd) {
    std::lock_guard<std::mutex> lock(room_mutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [fd](const RoomClient& c) { return c.fd == fd; }),
        clients_.end()
    );
}

bool ChatRoom::has_client(int fd) const {
    std::lock_guard<std::mutex> lock(room_mutex_);
    return std::any_of(clients_.begin(), clients_.end(),
        [fd](const RoomClient& c) { return c.fd == fd; });
}

std::string ChatRoom::get_client_display_name(int fd) const {
    std::lock_guard<std::mutex> lock(room_mutex_);
    auto it = std::find_if(clients_.begin(), clients_.end(),
        [fd](const RoomClient& c) { return c.fd == fd; });
    if (it != clients_.end()) {
        return it->name + " (" + it->ip + ")";
    }
    return "Unknown";
}

void ChatRoom::add_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(room_mutex_);
    add_message_internal(message);
}

void ChatRoom::add_message_internal(const std::string& message) {
    chat_history_.push_back(message);
    if (chat_history_.size() > MAX_HISTORY_SIZE) {
        chat_history_.pop_front();
    }
}

void ChatRoom::broadcast_message(const std::string& message, int sender_fd) {
    std::lock_guard<std::mutex> lock(room_mutex_);
    add_message_internal(message);  // Use internal version that doesn't lock
    
    // Format as BROADCAST: for client event system
    std::string broadcast_msg = "BROADCAST:" + message;
    
    for (const auto& client : clients_) {
        if (client.fd != sender_fd) {
            send(client.fd, broadcast_msg.c_str(), broadcast_msg.length(), MSG_NOSIGNAL);
        }
    }
}

void ChatRoom::send_history_to_client(int client_fd) {
    std::lock_guard<std::mutex> lock(room_mutex_);
    if (!chat_history_.empty()) {
        std::string history_msg = "=== Chat History ===\n";
        send(client_fd, history_msg.c_str(), history_msg.length(), MSG_NOSIGNAL);
        
        for (const auto& msg : chat_history_) {
            send(client_fd, msg.c_str(), msg.length(), MSG_NOSIGNAL);
        }
        
        std::string end_msg = "=== End of History ===\n";
        send(client_fd, end_msg.c_str(), end_msg.length(), MSG_NOSIGNAL);
    }
}

std::vector<std::string> ChatRoom::get_client_names() const {
    std::lock_guard<std::mutex> lock(room_mutex_);
    std::vector<std::string> names;
    for (const auto& client : clients_) {
        names.push_back(client.name);
    }
    return names;
}
