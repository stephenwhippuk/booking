#pragma once

#include <string>
#include <vector>
#include <mutex>

struct ClientInfo {
    int fd;
    std::string name;
    std::string ip;
};

class ClientManager {
private:
    std::vector<ClientInfo> connected_clients_;
    std::mutex clients_mutex_;
    std::mutex cout_mutex_;

    void broadcast_message(const std::string& message, int sender_fd);
    void remove_client(int client_fd);
    std::string request_client_name(int client_fd, const std::string& client_ip);

public:
    ClientManager();
    ~ClientManager();

    void handle_client(int client_fd, const std::string& client_ip);
};
