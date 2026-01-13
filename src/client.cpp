#include <iostream>
#include "ServerConnection.h"
#include "ChatUI.h"


int main() {
    ServerConnection connection;
    ChatUI ui;
    
    ui.initialize();
    
    if (!ui.show_login_screen(connection)) {
        ui.cleanup();
        std::cout << "Failed to connect to server.\n";
        return 1;
    }
    
    ui.setup_chat_windows();
    ui.add_chat_line("Connected to server. Type your messages below.");
    
    // Start receiving messages with callbacks
    connection.start_receiving(
        [&ui](const std::string& message) {
            ui.add_chat_line(message);
        },
        [&ui]() {
            ui.stop();
        }
    );
    
    ui.run_input_loop(connection);
    
    connection.disconnect();
    ui.cleanup();
    
    std::cout << "Disconnected from server.\n";
    return 0;
}
