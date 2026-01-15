#pragma once

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <any>

// Event types as specified in the spec
enum class EventType {
    INITIALIZED,
    LOGIN_SUBMITTED,
    LOGGED_IN,
    LOGGED_OUT,
    KICKED,
    FOYER_JOINED,
    ROOM_SELECTED,
    ROOM_REQUESTED,
    ROOM_JOINED,
    LEAVE_REQUESTED,
    LOGOUT_REQUESTED,
    COMMAND_SUBMITTED,
    COMMAND_NOT_RECOGNISED,
    CHAT_LINE_SUBMITTED,
    CHAT_RECEIVED,
    ROOMS_UPDATED,
    APP_KILLED
};

// Event data structure
struct Event {
    EventType type;
    std::map<std::string, std::any> data;
    
    Event(EventType t) : type(t) {}
    
    // Helper methods to get/set data
    template<typename T>
    void set(const std::string& key, const T& value) {
        data[key] = value;
    }
    
    template<typename T>
    T get(const std::string& key) const {
        auto it = data.find(key);
        if (it != data.end()) {
            return std::any_cast<T>(it->second);
        }
        throw std::runtime_error("Key not found in event data: " + key);
    }
    
    template<typename T>
    T get_or(const std::string& key, const T& default_value) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return default_value;
            }
        }
        return default_value;
    }
    
    bool has(const std::string& key) const {
        return data.find(key) != data.end();
    }
};

// EventBus for publish/subscribe pattern
class EventBus {
private:
    std::map<EventType, std::vector<std::function<void(const Event&)>>> subscribers_;
    
public:
    // Subscribe to an event type
    void subscribe(EventType type, std::function<void(const Event&)> handler) {
        subscribers_[type].push_back(handler);
    }
    
    // Publish an event
    void publish(const Event& event) {
        auto it = subscribers_.find(event.type);
        if (it != subscribers_.end()) {
            for (auto& handler : it->second) {
                handler(event);
            }
        }
    }
    
    // Clear all subscribers
    void clear() {
        subscribers_.clear();
    }
};
