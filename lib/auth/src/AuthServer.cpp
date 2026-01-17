#include "auth/AuthServer.h"
#include "auth/FileUserRepository.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>

AuthServer::AuthServer(int port)
    : port_(port)
    , server_fd_(-1)
    , running_(false)
{
    // Create file-based user repository
    auto user_repository = std::make_shared<FileUserRepository>("users.csv");
    auth_manager_ = std::make_unique<AuthManager>(user_repository);
}

AuthServer::~AuthServer() {
    stop();
}

void AuthServer::start() {
    if (running_) {
        return;
    }
    
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create auth server socket\n";
        return;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind auth server to port " << port_ << "\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }
    
    // Listen
    if (listen(server_fd_, 10) < 0) {
        std::cerr << "Failed to listen on auth server\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }
    
    running_ = true;
    server_thread_ = std::make_unique<std::thread>(&AuthServer::server_loop, this);
    
    std::cout << "Auth server listening on port " << port_ << "...\n";
}

void AuthServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    std::cout << "Auth server stopped\n";
}

bool AuthServer::is_running() const {
    return running_;
}

void AuthServer::server_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "Auth server accept failed\n";
            }
            continue;
        }
        
        // Handle client in separate thread (for now, simple inline handling)
        handle_client(client_fd);
        close(client_fd);
    }
}

void AuthServer::handle_client(int client_fd) {
    char buffer[4096];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        return;
    }
    
    buffer[bytes_read] = '\0';
    std::string request(buffer);
    
    process_request(client_fd, request);
}

void AuthServer::process_request(int client_fd, const std::string& request) {
    std::istringstream iss(request);
    std::string command;
    iss >> command;
    
    if (command == "AUTH") {
        // AUTH username password
        std::string username, password;
        iss >> username >> password;
        
        AuthToken token = auth_manager_->authenticate(username, password);
        
        std::string response;
        if (token.is_valid) {
            response = "OK " + token.token + " " + token.display_name + "\n";
        } else {
            response = "FAILED\n";
        }
        
        send(client_fd, response.c_str(), response.length(), 0);
        
    } else if (command == "VALIDATE") {
        // VALIDATE token
        std::string token;
        iss >> token;
        
        bool valid = auth_manager_->validate_token(token);
        std::string response = valid ? "VALID\n" : "INVALID\n";
        
        send(client_fd, response.c_str(), response.length(), 0);
        
    } else if (command == "GETUSER") {
        // GETUSER token
        std::string token;
        iss >> token;
        
        auto username = auth_manager_->get_username(token);
        auto display_name = auth_manager_->get_display_name(token);
        std::string response;
        if (username && display_name) {
            response = "USER " + *username + " " + *display_name + "\n";
        } else {
            response = "NOTFOUND\n";
        }
        
        send(client_fd, response.c_str(), response.length(), 0);
        
    } else if (command == "REGISTER") {
        // REGISTER username password display_name (rest of line)
        std::string username, password;
        iss >> username >> password;
        
        // Read rest of line as display name (may contain spaces)
        std::string display_name;
        std::getline(iss >> std::ws, display_name);
        
        // If no display name provided, use username
        if (display_name.empty()) {
            display_name = username;
        }
        
        bool success = auth_manager_->register_user(username, password, display_name);
        std::string response = success ? "REGISTERED\n" : "EXISTS\n";
        
        send(client_fd, response.c_str(), response.length(), 0);
        
    } else if (command == "REVOKE") {
        // REVOKE token
        std::string token;
        iss >> token;
        
        auth_manager_->revoke_token(token);
        std::string response = "REVOKED\n";
        
        send(client_fd, response.c_str(), response.length(), 0);
        
    } else {
        std::string response = "UNKNOWN_COMMAND\n";
        send(client_fd, response.c_str(), response.length(), 0);
    }
}
