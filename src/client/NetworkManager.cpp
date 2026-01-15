#include "NetworkManager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>

constexpr int BUFFER_SIZE = 4096;
constexpr int SELECT_TIMEOUT_MS = 50;

using namespace std::chrono_literals;

NetworkManager::NetworkManager(ThreadSafeQueue<std::string>& inbound,
                               ThreadSafeQueue<std::string>& outbound)
    : inbound_queue_(inbound)
    , outbound_queue_(outbound)
    , socket_(-1)
    , connected_(false)
    , running_(false)
{
}

NetworkManager::~NetworkManager() {
    stop();
}

bool NetworkManager::connect(const std::string& host, int port, std::string& error_msg) {
    // Create socket
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        error_msg = "Failed to create socket";
        return false;
    }
    
    // Set up server address
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        error_msg = "Invalid address";
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    // Connect to server
    if (::connect(socket_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        error_msg = "Failed to connect to server";
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    connected_ = true;
    return true;
}

void NetworkManager::start() {
    if (running_ || !connected_) {
        return;
    }
    
    running_ = true;
    network_thread_ = std::thread(&NetworkManager::network_loop, this);
}

void NetworkManager::stop() {
    running_ = false;
    connected_ = false;
    
    // Shutdown socket to unblock recv()
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_RDWR);
    }
    
    // Wait for network thread to finish
    if (network_thread_.joinable()) {
        network_thread_.join();
    }
    
    // Close socket
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
}

bool NetworkManager::is_connected() const {
    return connected_;
}

int NetworkManager::get_socket() const {
    return socket_;
}

void NetworkManager::network_loop() {
    while (running_) {
        // Use select() for non-blocking check on socket readability
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = SELECT_TIMEOUT_MS * 1000;
        
        int result = select(socket_ + 1, &read_fds, nullptr, nullptr, &tv);
        
        if (result > 0 && FD_ISSET(socket_, &read_fds)) {
            receive_data();
        }
        
        // Try to send queued outbound messages
        send_data();
    }
}

void NetworkManager::receive_data() {
    char buffer[BUFFER_SIZE];
    
    ssize_t bytes_read = recv(socket_, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        // Connection closed or error
        connected_ = false;
        running_ = false;
        
        if (bytes_read == 0) {
            inbound_queue_.push("SERVER_DISCONNECTED\n");
        } else {
            inbound_queue_.push("CONNECTION_ERROR\n");
        }
        return;
    }
    
    // Null-terminate and enqueue
    buffer[bytes_read] = '\0';
    inbound_queue_.push(std::string(buffer));
}

void NetworkManager::send_data() {
    std::string message;
    
    // Non-blocking check for outbound messages
    while (outbound_queue_.try_pop_immediate(message)) {
        if (!connected_) {
            break;  // Don't try to send if disconnected
        }
        
        ssize_t bytes_sent = send(socket_, message.c_str(), message.length(), 0);
        
        if (bytes_sent < 0) {
            // Send failed - connection likely broken
            connected_ = false;
            running_ = false;
            inbound_queue_.push("CONNECTION_ERROR\n");
            break;
        }
        
        // Note: In production, we might need to handle partial sends
        // For now, assuming messages are small enough to send atomically
    }
}
