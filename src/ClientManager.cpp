#include "ClientManager.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>

constexpr int BUFFER_SIZE = 4096;

ClientManager::ClientManager() {}

ClientManager::~ClientManager() {}

void ClientManager::broadcast_message(const std::string& message, int sender_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (const auto& client : connected_clients_) {
        if (client.fd != sender_fd) {
            send(client.fd, message.c_str(), message.length(), MSG_NOSIGNAL);
        }
    }
}

void ClientManager::remove_client(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    connected_clients_.erase(
        std::remove_if(connected_clients_.begin(), connected_clients_.end(),
            [client_fd](const ClientInfo& c) { return c.fd == client_fd; }),
        connected_clients_.end()
    );
}

std::string ClientManager::request_client_name(int client_fd, const std::string& client_ip) {
    // Request name from client
    const char* name_request = "PROVIDE_NAME\n";
    send(client_fd, name_request, strlen(name_request), 0);
    
    // Receive the name
    char name_buffer[256];
    ssize_t name_bytes = recv(client_fd, name_buffer, sizeof(name_buffer) - 1, 0);
    
    if (name_bytes <= 0) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "Client from " << client_ip << " disconnected before providing name\n";
        return "";
    }
    
    name_buffer[name_bytes] = '\0';
    // Remove trailing newline if present
    std::string client_name = name_buffer;
    if (!client_name.empty() && client_name.back() == '\n') {
        client_name.pop_back();
    }
    
    return client_name;
}

void ClientManager::handle_client(int client_fd, const std::string& client_ip) {
    std::string client_name = request_client_name(client_fd, client_ip);
    
    if (client_name.empty()) {
        close(client_fd);
        return;
    }
    
    std::string display_name = client_name + " (" + client_ip + ")";
    
    // Add client to connected clients list
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        connected_clients_.push_back({client_fd, client_name, client_ip});
    }
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "Client connected: " << display_name << "\n";
    }
    
    // Notify all other clients
    std::string join_message = "[SERVER] " + display_name + " joined the chat\n";
    broadcast_message(join_message, client_fd);

    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cout << display_name << " disconnected\n";
            } else {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cerr << "Error reading from " << display_name << "\n";
            }
            break;
        }

        buffer[bytes_read] = '\0';
        
        // Display on server
        {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cout << "[" << display_name << "] " << buffer;
            std::cout.flush();
        }
        
        // Broadcast to all other clients
        std::string chat_message = "[" + display_name + "] " + std::string(buffer);
        broadcast_message(chat_message, client_fd);
    }
    
    // Notify others that client left
    std::string leave_message = "[SERVER] " + display_name + " left the chat\n";
    broadcast_message(leave_message, client_fd);
    
    remove_client(client_fd);
    close(client_fd);
}
