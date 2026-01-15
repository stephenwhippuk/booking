#pragma once

#include <string>
#include <functional>

class ServerSocket {
private:
    int server_fd_;
    int port_;
    bool listening_;

public:
    ServerSocket(int port);
    ~ServerSocket();

    bool initialize(std::string& error_msg);
    void accept_connections(std::function<void(int, const std::string&)> on_client_connected);
    void shutdown();
    bool is_listening() const;
};
