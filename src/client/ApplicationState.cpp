#include "ApplicationState.h"
#include <algorithm>

ApplicationState::ApplicationState()
    : connected_(false)
    , current_screen_(Screen::LOGIN)
{
}

// Connection state
void ApplicationState::set_connected(bool connected) {
    connected_ = connected;
}

bool ApplicationState::is_connected() const {
    return connected_;
}

void ApplicationState::set_username(const std::string& username) {
    username_ = username;
}

std::string ApplicationState::get_username() const {
    return username_;
}

// Screen state
void ApplicationState::set_screen(Screen screen) {
    current_screen_ = screen;
}

ApplicationState::Screen ApplicationState::get_screen() const {
    return current_screen_;
}

// Foyer state
void ApplicationState::set_rooms(const std::vector<RoomInfo>& rooms) {
    rooms_ = rooms;
}

std::vector<RoomInfo> ApplicationState::get_rooms() const {
    return rooms_;
}

void ApplicationState::add_room(const RoomInfo& room) {
    rooms_.push_back(room);
}

void ApplicationState::clear_rooms() {
    rooms_.clear();
}

// Chatroom state
void ApplicationState::set_current_room(const std::string& room_name) {
    current_room_ = room_name;
}

std::string ApplicationState::get_current_room() const {
    return current_room_;
}

void ApplicationState::add_chat_message(const std::string& message) {
    chat_messages_.push_back(message);
}

std::vector<std::string> ApplicationState::get_chat_messages() const {
    return chat_messages_;
}

void ApplicationState::clear_chat_messages() {
    chat_messages_.clear();
}

void ApplicationState::set_participants(const std::vector<std::string>& participants) {
    participants_ = participants;
}

std::vector<std::string> ApplicationState::get_participants() const {
    return participants_;
}

void ApplicationState::add_participant(const std::string& username) {
    participants_.push_back(username);
}

void ApplicationState::remove_participant(const std::string& username) {
    auto it = std::find(participants_.begin(), participants_.end(), username);
    if (it != participants_.end()) {
        participants_.erase(it);
    }
}

void ApplicationState::reset() {
    connected_ = false;
    username_.clear();
    current_screen_ = Screen::LOGIN;
    rooms_.clear();
    current_room_.clear();
    chat_messages_.clear();
    participants_.clear();
}
