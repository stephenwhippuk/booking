#pragma once

#include "AuthManager.h"
#include <string>
#include <thread>
#include <atomic>
#include <memory>

class AuthServer {
public:
    explicit AuthServer(int port = 3001, const std::string& user_db_path = "users.json");
    ~AuthServer();
    
    // Start the auth server
    void start();
    
    // Stop the auth server
    void stop();
    
    // Check if server is running
    bool is_running() const;

private:
    void server_loop();
    void handle_client(int client_fd);
    void process_request(int client_fd, const std::string& request);
    
    int port_;
    std::string user_db_path_;
    int server_fd_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> server_thread_;
    std::unique_ptr<AuthManager> auth_manager_;
};
