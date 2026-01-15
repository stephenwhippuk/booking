#include <iostream>
#include <thread>
#include "ServerSocket.h"
#include "ClientManager.h"

constexpr int PORT = 3000;

int main() {
    ServerSocket server_socket(PORT);
    ClientManager client_manager;
    
    std::string error_msg;
    if (!server_socket.initialize(error_msg)) {
        std::cerr << "Server initialization failed: " << error_msg << "\n";
        return 1;
    }
    
    std::cout << "Server listening on port " << PORT << "...\n";
    
    // Accept connections and spawn threads to handle clients
    server_socket.accept_connections([&client_manager](int client_fd, const std::string& client_ip) {
        std::thread client_thread(&ClientManager::handle_client, &client_manager, client_fd, client_ip);
        client_thread.detach();
    });
    
    return 0;
}
