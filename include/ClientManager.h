#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include "ChatRoom.h"

struct ClientInfo {
    int fd;
    std::string name;
    std::string ip;
    std::string current_room;
};

class ClientManager {
private:
    std::vector<ClientInfo> connected_clients_;
    std::map<std::string, std::shared_ptr<ChatRoom>> chat_rooms_;
    std::mutex clients_mutex_;
    std::mutex rooms_mutex_;
    std::mutex cout_mutex_;

    void remove_client(int client_fd);
    std::string request_client_name(int client_fd, const std::string& client_ip);
    void handle_foyer(int client_fd, ClientInfo& client_info);
    void handle_room_chat(int client_fd, ClientInfo& client_info);
    void send_room_list(int client_fd);
    void broadcast_room_list_to_foyer();
    void broadcast_member_list_to_room(const std::string& room_name);
    bool create_room(const std::string& room_name);
    bool join_room(int client_fd, ClientInfo& client_info, const std::string& room_name);
    void leave_room(int client_fd, ClientInfo& client_info);
    std::string get_client_name(int client_fd);

public:
    ClientManager();
    ~ClientManager();

    void handle_client(int client_fd, const std::string& client_ip);
};
