#pragma once

#include <string>
#include <chrono>
#include <vector>

struct AuthToken {
    std::string token;
    std::string username;
    std::string display_name;
    std::vector<std::string> roles;
    std::chrono::system_clock::time_point issued_at;
    std::chrono::system_clock::time_point expires_at;
    bool is_valid;
    
    AuthToken() : is_valid(false) {}
    
    AuthToken(const std::string& tok, const std::string& user, const std::string& display,
              const std::vector<std::string>& user_roles = {},
              std::chrono::minutes validity = std::chrono::minutes(60))
        : token(tok)
        , username(user)
        , display_name(display)
        , roles(user_roles)
        , issued_at(std::chrono::system_clock::now())
        , expires_at(issued_at + validity)
        , is_valid(true)
    {}
    
    bool is_expired() const {
        return std::chrono::system_clock::now() > expires_at;
    }
};
