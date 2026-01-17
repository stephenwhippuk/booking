#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <chrono>

using json = nlohmann::json;

/**
 * NetworkMessage - JSON-based message format for client-server communication
 * 
 * Format:
 * {
 *   "header": {
 *     "timestamp": "2026-01-17T12:34:56Z",
 *     "token": "abc123..." (optional, empty for server responses)
 *   },
 *   "body": {
 *     "type": "MESSAGE|JOIN_ROOM|CREATE_ROOM|LEAVE|...",
 *     "data": {...}  (type-specific data)
 *   }
 * }
 */
struct NetworkMessage {
    struct Header {
        std::string timestamp;
        std::string token;
        
        json to_json() const {
            return json{
                {"timestamp", timestamp},
                {"token", token}
            };
        }
        
        static Header from_json(const json& j) {
            Header h;
            h.timestamp = j.value("timestamp", "");
            h.token = j.value("token", "");
            return h;
        }
    };
    
    struct Body {
        std::string type;
        json data;
        
        json to_json() const {
            return json{
                {"type", type},
                {"data", data}
            };
        }
        
        static Body from_json(const json& j) {
            Body b;
            b.type = j.value("type", "");
            b.data = j.value("data", json::object());
            return b;
        }
    };
    
    Header header;
    Body body;
    
    // Serialize to JSON string
    std::string serialize() const {
        json j;
        j["header"] = header.to_json();
        j["body"] = body.to_json();
        return j.dump() + "\n";
    }
    
    // Deserialize from JSON string
    static NetworkMessage deserialize(const std::string& str) {
        NetworkMessage msg;
        try {
            json j = json::parse(str);
            msg.header = Header::from_json(j["header"]);
            msg.body = Body::from_json(j["body"]);
        } catch (const json::exception& e) {
            // Return empty message on parse error
        }
        return msg;
    }
    
    // Helper to get current timestamp in ISO 8601 format
    static std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        
        char buffer[30];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return std::string(buffer);
    }
    
    // Factory methods for common message types
    static NetworkMessage create_auth(const std::string& token) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = token;
        msg.body.type = "AUTH";
        msg.body.data = json::object();
        return msg;
    }
    
    static NetworkMessage create_join_room(const std::string& token, const std::string& room_name) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = token;
        msg.body.type = "JOIN_ROOM";
        msg.body.data = {{"room_name", room_name}};
        return msg;
    }
    
    static NetworkMessage create_create_room(const std::string& token, const std::string& room_name) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = token;
        msg.body.type = "CREATE_ROOM";
        msg.body.data = {{"room_name", room_name}};
        return msg;
    }
    
    static NetworkMessage create_leave(const std::string& token) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = token;
        msg.body.type = "LEAVE";
        msg.body.data = json::object();
        return msg;
    }
    
    static NetworkMessage create_chat_message(const std::string& token, const std::string& message) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = token;
        msg.body.type = "CHAT_MESSAGE";
        msg.body.data = {{"message", message}};
        return msg;
    }
    
    static NetworkMessage create_quit(const std::string& token) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = token;
        msg.body.type = "QUIT";
        msg.body.data = json::object();
        return msg;
    }
    
    // Server response messages (no token)
    static NetworkMessage create_error(const std::string& error_message) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = "";
        msg.body.type = "ERROR";
        msg.body.data = {{"message", error_message}};
        return msg;
    }
    
    static NetworkMessage create_room_joined(const std::string& room_name) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = "";
        msg.body.type = "ROOM_JOINED";
        msg.body.data = {{"room_name", room_name}};
        return msg;
    }
    
    static NetworkMessage create_room_list(const std::vector<std::string>& rooms) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = "";
        msg.body.type = "ROOM_LIST";
        msg.body.data = {{"rooms", rooms}};
        return msg;
    }
    
    static NetworkMessage create_participant_list(const std::vector<std::string>& participants) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = "";
        msg.body.type = "PARTICIPANT_LIST";
        msg.body.data = {{"participants", participants}};
        return msg;
    }
    
    static NetworkMessage create_broadcast_message(const std::string& sender, const std::string& message) {
        NetworkMessage msg;
        msg.header.timestamp = get_timestamp();
        msg.header.token = "";
        msg.body.type = "MESSAGE";
        msg.body.data = {
            {"sender", sender},
            {"message", message}
        };
        return msg;
    }
};
