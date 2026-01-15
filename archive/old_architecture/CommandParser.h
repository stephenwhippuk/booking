#pragma once

#include "Event.h"
#include <string>

// Parses user command input and raises appropriate events
class CommandParser {
private:
    EventBus& event_bus_;
    
public:
    CommandParser(EventBus& event_bus);
    
    // Parse command and raise appropriate event
    void parse_and_execute(const std::string& input);
};
