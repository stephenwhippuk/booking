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

int main() {
    try {
        // Create all queues
        ThreadSafeQueue<std::string> network_inbound;
        ThreadSafeQueue<std::string> network_outbound;
        ThreadSafeQueue<UICommand> ui_commands;
        ThreadSafeQueue<std::string> input_events;
        
        // Create managers with queue references
        NetworkManager network(network_inbound, network_outbound);
        ApplicationManager application(network_inbound, network_outbound, 
                                      ui_commands, input_events, &network);
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
