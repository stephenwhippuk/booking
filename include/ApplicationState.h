#ifndef APPLICATIONSTATE_H
#define APPLICATIONSTATE_H

#include "RoomInfo.h"
#include <string>
#include <vector>

/**
 * ApplicationState - Single source of truth for application state
 * 
 * IMPORTANT: This is NOT thread-safe by design!
 * Only the Application thread should access this state.
 * Other threads (Network, UI) communicate via queues.
 */
class ApplicationState {
public:
    enum class Screen {
        LOGIN,
        FOYER,
        CHATROOM
    };

private:
    // Connection state
    bool connected_;
    std::string username_;
    std::string token_;
    
    // Screen state
    Screen current_screen_;
    
    // Foyer state
    std::vector<RoomInfo> rooms_;
    
    // Chatroom state
    std::string current_room_;
    std::vector<std::string> chat_messages_;
    std::vector<std::string> participants_;

public:
    ApplicationState();
    
    // Connection state
    void set_connected(bool connected);
    bool is_connected() const;
    
    void set_username(const std::string& username);
    std::string get_username() const;
    
    void set_token(const std::string& token);
    std::string get_token() const;
    
    // Screen state
    void set_screen(Screen screen);
    Screen get_screen() const;
    
    // Foyer state
    void set_rooms(const std::vector<RoomInfo>& rooms);
    std::vector<RoomInfo> get_rooms() const;
    void add_room(const RoomInfo& room);
    void clear_rooms();
    
    // Chatroom state
    void set_current_room(const std::string& room_name);
    std::string get_current_room() const;
    
    void add_chat_message(const std::string& message);
    std::vector<std::string> get_chat_messages() const;
    void clear_chat_messages();
    
    void set_participants(const std::vector<std::string>& participants);
    std::vector<std::string> get_participants() const;
    void add_participant(const std::string& username);
    void remove_participant(const std::string& username);
    
    // Reset to initial state
    void reset();
};

#endif // APPLICATIONSTATE_H
