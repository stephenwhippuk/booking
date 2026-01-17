#include "auth/AuthServer.h"
#include <iostream>
#include <signal.h>

static AuthServer* g_server = nullptr;

void signal_handler(int signal) {
    if (g_server) {
        std::cout << "\nShutting down auth server...\n";
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 3001;  // Default auth server port
    
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    AuthServer server(port);
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
