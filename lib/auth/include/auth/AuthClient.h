#pragma once

#include <string>
#include <optional>
#include <vector>

struct AuthResult {
    bool success;
    std::string token;
    std::string display_name;
    std::string error_message;
    
    AuthResult() : success(false) {}
};

struct UserInfo {
    std::string username;
    std::string display_name;
    std::vector<std::string> roles;
    
    UserInfo() = default;
    UserInfo(const std::string& user, const std::string& display)
        : username(user), display_name(display) {}
    UserInfo(const std::string& user, const std::string& display, const std::vector<std::string>& user_roles)
        : username(user), display_name(display), roles(user_roles) {}
};

class AuthClient {
public:
    AuthClient(const std::string& host = "localhost", int port = 3001);
    ~AuthClient() = default;
    
    // Authenticate and get token
    AuthResult authenticate(const std::string& username, const std::string& password);
    
    // Validate a token
    bool validate_token(const std::string& token);
    
    // Get user info from token
    std::optional<UserInfo> get_user_info(const std::string& token);
    
    // Register new user
    bool register_user(const std::string& username, const std::string& password, 
                      const std::string& display_name);
    
    // Revoke token
    bool revoke_token(const std::string& token);

private:
    std::string send_command(const std::string& command);
    
    std::string host_;
    int port_;
};
