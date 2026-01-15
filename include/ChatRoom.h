#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <deque>

constexpr size_t MAX_HISTORY_SIZE = 100;

struct RoomClient {
    int fd;
    std::string name;
    std::string ip;
};

class ChatRoom {
private:
    std::string name_;
    std::vector<RoomClient> clients_;
    std::deque<std::string> chat_history_;
    mutable std::mutex room_mutex_;
    
    void add_message_internal(const std::string& message);

public:
    explicit ChatRoom(const std::string& name);
    
    std::string get_name() const;
    size_t get_client_count() const;
    
    void add_client(int fd, const std::string& name, const std::string& ip);
    void remove_client(int fd);
    bool has_client(int fd) const;
    std::string get_client_display_name(int fd) const;
    
    void add_message(const std::string& message);
    void broadcast_message(const std::string& message, int sender_fd);
    void send_history_to_client(int client_fd);
    
    std::vector<std::string> get_client_names() const;
};
