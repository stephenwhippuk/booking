#include <iostream>
#include <atomic>
#include "ServerConnection.h"
#include "ChatUI.h"
#include "Event.h"
#include "MessageHandler.h"
#include "CommandParser.h"

int main() {
    // Create event bus
    EventBus event_bus;
    
    // Create components
    ServerConnection connection;
    ChatUI ui(event_bus);
    MessageHandler message_handler(connection, event_bus);
    
    // Setup flag to control main loop
    std::atomic<bool> app_running{true};
    
    // Wire up event handlers
    
    // UI handlers
    event_bus.subscribe(EventType::INITIALIZED, [&ui](const Event& e) {
        ui.handle_initialized(e);
    });
    
    event_bus.subscribe(EventType::LOGGED_IN, [&ui](const Event& e) {
        ui.handle_logged_in(e);
    });
    
    event_bus.subscribe(EventType::KICKED, [&ui](const Event& e) {
        ui.handle_kicked(e);
    });
    
    event_bus.subscribe(EventType::FOYER_JOINED, [&ui](const Event& e) {
        ui.handle_foyer_joined(e);
    });
    
    event_bus.subscribe(EventType::ROOM_JOINED, [&ui](const Event& e) {
        ui.handle_room_joined(e);
    });
    
    event_bus.subscribe(EventType::CHAT_RECEIVED, [&ui](const Event& e) {
        ui.handle_chat_received(e);
    });
    
    event_bus.subscribe(EventType::ROOMS_UPDATED, [&ui](const Event& e) {
        ui.handle_rooms_updated(e);
    });
    
    // Message handler subscriptions
    event_bus.subscribe(EventType::LOGIN_SUBMITTED, [&message_handler](const Event& e) {
        message_handler.handle_login_submitted(e);
    });
    
    event_bus.subscribe(EventType::LOGGED_IN, [&message_handler](const Event& e) {
        message_handler.handle_logged_in(e);
        message_handler.start_listening();
    });
    
    event_bus.subscribe(EventType::ROOM_SELECTED, [&message_handler](const Event& e) {
        message_handler.handle_room_selected(e);
    });
    
    event_bus.subscribe(EventType::ROOM_REQUESTED, [&message_handler](const Event& e) {
        message_handler.handle_room_requested(e);
    });
    
    event_bus.subscribe(EventType::LEAVE_REQUESTED, [&message_handler](const Event& e) {
        message_handler.handle_leave_requested(e);
    });
    
    event_bus.subscribe(EventType::LOGOUT_REQUESTED, [&message_handler](const Event& e) {
        message_handler.handle_logout_requested(e);
    });
    
    event_bus.subscribe(EventType::CHAT_LINE_SUBMITTED, [&message_handler](const Event& e) {
        message_handler.handle_chat_line_submitted(e);
    });
    
    // App lifecycle handlers
    event_bus.subscribe(EventType::APP_KILLED, [&app_running, &ui](const Event& e) {
        app_running = false;
        ui.stop();
    });
    
    event_bus.subscribe(EventType::LOGGED_OUT, [&ui](const Event& e) {
        ui.handle_kicked(e);
    });
    
    // Initialize the application
    ui.initialize();
    
    // Main event loop - just keep the app alive
    // All interactions are driven by events
    while (app_running && connection.is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup in proper order
    // 1. Stop the UI first to prevent new events
    ui.stop();
    
    // 2. Stop listening to prevent callbacks during cleanup
    message_handler.stop_listening();
    
    // 3. Give threads time to exit cleanly
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 4. Disconnect and cleanup
    connection.disconnect();
    ui.cleanup();
    
    std::cout << "Application closed.\n";
    return 0;
}
