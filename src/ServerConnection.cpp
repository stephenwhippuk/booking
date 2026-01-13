#include "ServerConnection.h"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

ServerConnection::ServerConnection() : sock_(-1) {}

ServerConnection::~ServerConnection() {
    disconnect();
}

void ServerConnection::receive_loop() {
    char buffer[BUFFER_SIZE];
    while (connected_) {
        ssize_t bytes_read = recv(sock_, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0 && connected_) {
                if (message_callback_) {
                    message_callback_("Server disconnected");
                }
            }
            connected_ = false;
            if (disconnect_callback_) {
                disconnect_callback_();
            }
            break;
        }
        
        buffer[bytes_read] = '\0';
        if (message_callback_) {
            message_callback_(std::string(buffer));
        }
    }
}

bool ServerConnection::connect_to_server(const std::string& ip, int port, std::string& error_msg) {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) {
        error_msg = "Failed to create socket";
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        error_msg = "Invalid address";
        close(sock_);
        return false;
    }

    if (::connect(sock_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        error_msg = "Failed to connect to server";
        close(sock_);
        return false;
    }

    connected_ = true;
    return true;
}

bool ServerConnection::receive_protocol_message(std::string& message, std::string& error_msg) {
    char buffer[256];
    ssize_t bytes_read = recv(sock_, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        error_msg = "Failed to receive protocol message";
        return false;
    }
    buffer[bytes_read] = '\0';
    message = std::string(buffer);
    return true;
}

bool ServerConnection::send_message(const std::string& message) {
    ssize_t bytes_sent = send(sock_, message.c_str(), message.length(), 0);
    return bytes_sent >= 0;
}

void ServerConnection::start_receiving(std::function<void(const std::string&)> on_message,
                    std::function<void()> on_disconnect) {
    message_callback_ = on_message;
    disconnect_callback_ = on_disconnect;
    receive_thread_ = std::thread(&ServerConnection::receive_loop, this);
}

void ServerConnection::disconnect() {
    if (connected_) {
        connected_ = false;
        shutdown(sock_, SHUT_RDWR);
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
        close(sock_);
    }
}

bool ServerConnection::is_connected() const {
    return connected_;
}
