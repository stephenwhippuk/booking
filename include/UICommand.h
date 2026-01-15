#ifndef UICOMMAND_H
#define UICOMMAND_H

#include "RoomInfo.h"
#include <string>
#include <vector>
#include <variant>

/**
 * UICommand - Commands sent from Application thread to UI thread
 * 
 * UI thread polls the ui_commands queue and updates display accordingly.
 * Commands are simple, serializable, and contain all data needed for rendering.
 */

enum class UICommandType {
    // Screen transitions
    SHOW_LOGIN,
    SHOW_FOYER,
    SHOW_CHATROOM,
    
    // Foyer updates
    UPDATE_ROOM_LIST,
    
    // Chatroom updates
    ADD_CHAT_MESSAGE,
    UPDATE_PARTICIPANTS,
    
    // Status/error messages
    SHOW_ERROR,
    SHOW_STATUS,
    
    // Input control
    CLEAR_INPUT,
    
    // Application control
    QUIT
};

/**
 * Payload types for different commands
 */
struct RoomListData {
    std::vector<RoomInfo> rooms;
};

struct ChatMessageData {
    std::string message;
};

struct ParticipantsData {
    std::vector<std::string> participants;
};

struct ErrorData {
    std::string message;
};

struct StatusData {
    std::string message;
};

/**
 * UICommand struct - holds command type and associated data
 */
struct UICommand {
    UICommandType type;
    
    // Variant to hold different payload types
    std::variant<
        std::monostate,          // No data
        RoomListData,
        ChatMessageData,
        ParticipantsData,
        ErrorData,
        StatusData,
        std::string              // Generic string data
    > data;
    
    // Constructors for convenience
    UICommand(UICommandType t) : type(t), data(std::monostate{}) {}
    
    template<typename T>
    UICommand(UICommandType t, T&& d) : type(t), data(std::forward<T>(d)) {}
    
    // Helper to check if command has data
    bool has_data() const {
        return !std::holds_alternative<std::monostate>(data);
    }
    
    // Helper to get typed data (throws if wrong type)
    template<typename T>
    const T& get() const {
        return std::get<T>(data);
    }
};

#endif // UICOMMAND_H
