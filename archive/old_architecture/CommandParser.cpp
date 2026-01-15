#include "CommandParser.h"

CommandParser::CommandParser(EventBus& event_bus) : event_bus_(event_bus) {
}

void CommandParser::parse_and_execute(const std::string& input) {
    if (input.empty()) {
        return;
    }
    
    // Check if it's a command (starts with /)
    if (input[0] == '/') {
        if (input == "/quit") {
            Event event(EventType::APP_KILLED);
            event_bus_.publish(event);
        } else if (input == "/logout") {
            Event event(EventType::LOGOUT_REQUESTED);
            event_bus_.publish(event);
        } else if (input == "/leave") {
            Event event(EventType::LEAVE_REQUESTED);
            event_bus_.publish(event);
        } else {
            Event event(EventType::COMMAND_NOT_RECOGNISED);
            event.set("command", input);
            event_bus_.publish(event);
        }
    } else {
        // It's a chat message
        Event event(EventType::CHAT_LINE_SUBMITTED);
        event.set("message", input);
        event_bus_.publish(event);
    }
}
