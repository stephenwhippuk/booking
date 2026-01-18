// Simple test client for the new architecture
// This replaces the old client.cpp and demonstrates the new queue-based design

#include "ThreadSafeQueue.h"
#include "NetworkManager.h"
#include "ApplicationManager.h"
#include "UIManager.h"
#include "UICommand.h"
#include <iostream>
#include <memory>
#include <csignal>
#include <chrono>
#include <thread>
#include <fstream>
#include <nlohmann/json.hpp>

// Global pointers for signal handling
UIManager* g_ui_manager = nullptr;
NetworkManager* g_network_manager = nullptr;
ThreadSafeQueue<std::string>* g_network_outbound = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        // Send quit command to server before exiting
        if (g_network_outbound) {
            g_network_outbound->push("/quit\n");
        }
        
        // Give a moment for the message to send
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop UI
        if (g_ui_manager) {
            g_ui_manager->stop();
        }
    }
}

struct ClientConfig {
    std::string auth_host = "127.0.0.1";
    int auth_port = 3001;
    std::string chat_host = "127.0.0.1";
    int chat_port = 3000;
};

ClientConfig load_config() {
    ClientConfig cfg;
    std::ifstream file("config/client_config.json");
    if (!file.is_open()) return cfg;
    try {
        nlohmann::json j;
        file >> j;
        if (j.contains("auth_host")) cfg.auth_host = j.value("auth_host", cfg.auth_host);
        if (j.contains("auth_port")) cfg.auth_port = j.value("auth_port", cfg.auth_port);
        if (j.contains("chat_host")) cfg.chat_host = j.value("chat_host", cfg.chat_host);
        if (j.contains("chat_port")) cfg.chat_port = j.value("chat_port", cfg.chat_port);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to parse config/client_config.json: " << ex.what() << "\n";
    }
    return cfg;
}

int main() {
    try {
        ClientConfig cfg = load_config();

        // Create all queues
        ThreadSafeQueue<std::string> network_inbound;
        ThreadSafeQueue<std::string> network_outbound;
        ThreadSafeQueue<UICommand> ui_commands;
        ThreadSafeQueue<std::string> input_events;
        
        // Create managers with queue references
        NetworkManager network(network_inbound, network_outbound);
        ApplicationManager application(network_inbound, network_outbound, 
                                      ui_commands, input_events, &network,
                                      cfg.auth_host, cfg.auth_port,
                                      cfg.chat_host, cfg.chat_port);
        UIManager ui(ui_commands, input_events);
        
        // Set up signal handler
        g_ui_manager = &ui;
        g_network_manager = &network;
        g_network_outbound = &network_outbound;
        std::signal(SIGINT, signal_handler);
        
        // DON'T connect to chat server yet - wait until after authentication
        // Connection will happen in ApplicationManager after successful login
        
        // Start application thread (network will be started after login)
        application.start();
        
        // Run UI on main thread (blocks until quit)
        ui.run();
        
        // Send quit to server if we're still connected
        if (network.is_connected()) {
            network_outbound.push("/quit\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Clean shutdown
        application.stop();
        network.stop();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
