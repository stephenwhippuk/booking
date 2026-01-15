#include "ServerSocket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

ServerSocket::ServerSocket(int port) 
    : server_fd_(-1), port_(port), listening_(false) {}

ServerSocket::~ServerSocket() {
    shutdown();
}

bool ServerSocket::initialize(std::string& error_msg) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        error_msg = "Failed to create socket";
        return false;
    }

    // Allow port reuse
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error_msg = "Failed to set socket options";
        close(server_fd_);
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (sockaddr*)&address, sizeof(address)) < 0) {
        error_msg = "Failed to bind to port";
        close(server_fd_);
        return false;
    }

    if (listen(server_fd_, 5) < 0) {
        error_msg = "Failed to listen on socket";
        close(server_fd_);
        return false;
    }

    listening_ = true;
    return true;
}

void ServerSocket::accept_connections(std::function<void(int, const std::string&)> on_client_connected) {
    while (listening_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (listening_) {
                // Only log error if we're still supposed to be listening
                continue;
            }
            break;
        }

        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        
        if (on_client_connected) {
            on_client_connected(client_fd, client_ip);
        }
    }
}

void ServerSocket::shutdown() {
    if (server_fd_ >= 0) {
        listening_ = false;
        close(server_fd_);
        server_fd_ = -1;
    }
}

bool ServerSocket::is_listening() const {
    return listening_;
}
