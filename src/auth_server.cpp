#include "auth/AuthServer.h"
#include <iostream>
#include <signal.h>
#include <fstream>
#include <nlohmann/json.hpp>

static AuthServer* g_server = nullptr;

void signal_handler(int signal) {
    if (g_server) {
        std::cout << "\nShutting down auth server...\n";
        g_server->stop();
    }
}

struct AuthConfig {
    int port = 3001;
    std::string user_db_path = "users.json";
};

AuthConfig load_config() {
    AuthConfig cfg;
    std::ifstream file("config/auth_config.json");
    if (!file.is_open()) {
        return cfg; // use defaults
    }
    try {
        nlohmann::json j;
        file >> j;
        if (j.contains("port")) cfg.port = j.value("port", cfg.port);
        if (j.contains("user_db_path")) cfg.user_db_path = j.value("user_db_path", cfg.user_db_path);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to parse config/auth_config.json: " << ex.what() << "\n";
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    AuthConfig cfg = load_config();

    // CLI arg (if provided) overrides config for port
    if (argc > 1) {
        cfg.port = std::atoi(argv[1]);
    }
    
    AuthServer server(cfg.port, cfg.user_db_path);
    g_server = &server;
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    server.start();
    
    if (server.is_running()) {
        // Keep main thread alive
        while (server.is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    return 0;
}
