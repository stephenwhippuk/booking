#include <iostream>
#include <thread>
#include <fstream>
#include <nlohmann/json.hpp>
#include "ServerSocket.h"
#include "ClientManager.h"

struct ServerConfig {
    int port = 3000;
    std::string auth_host = "127.0.0.1";
    int auth_port = 3001;
};

ServerConfig load_config() {
    ServerConfig cfg;
    std::ifstream file("config/server_config.json");
    if (!file.is_open()) return cfg;
    try {
        nlohmann::json j;
        file >> j;
        if (j.contains("port")) cfg.port = j.value("port", cfg.port);
        if (j.contains("auth_host")) cfg.auth_host = j.value("auth_host", cfg.auth_host);
        if (j.contains("auth_port")) cfg.auth_port = j.value("auth_port", cfg.auth_port);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to parse config/server_config.json: " << ex.what() << "\n";
    }
    return cfg;
}

int main() {
    ServerConfig cfg = load_config();

    ServerSocket server_socket(cfg.port);
    ClientManager client_manager(cfg.auth_host, cfg.auth_port);
    
    std::string error_msg;
    if (!server_socket.initialize(error_msg)) {
        std::cerr << "Server initialization failed: " << error_msg << "\n";
        return 1;
    }
    
    std::cout << "Server listening on port " << cfg.port << "...\n";
    
    // Accept connections and spawn threads to handle clients
    server_socket.accept_connections([&client_manager](int client_fd, const std::string& client_ip) {
        std::thread client_thread(&ClientManager::handle_client, &client_manager, client_fd, client_ip);
        client_thread.detach();
    });
    
    return 0;
}
