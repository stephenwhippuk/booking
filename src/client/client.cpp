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

// Global pointer for signal handling
UIManager* g_ui_manager = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT && g_ui_manager) {
        g_ui_manager->stop();
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
                                      ui_commands, input_events);
        UIManager ui(ui_commands, input_events);
        
        // Set up signal handler
        g_ui_manager = &ui;
        std::signal(SIGINT, signal_handler);
        
        // Connect to server
        std::string error;
        if (!network.connect("127.0.0.1", 3000, error)) {
            std::cerr << "Failed to connect: " << error << std::endl;
            return 1;
        }
        
        // Start network and application threads
        network.start();
        application.start();
        
        // Run UI on main thread (blocks until quit)
        ui.run();
        
        // Clean shutdown
        application.stop();
        network.stop();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
