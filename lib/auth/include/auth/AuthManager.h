#pragma once

#include "AuthToken.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <memory>
#include <vector>

class IUserRepository;

struct User {
    std::string username;
    std::string password_hash;
    std::string display_name;
    std::vector<std::string> roles;
    
    User() = default;
    User(const std::string& user, const std::string& pass_hash, const std::string& display)
        : username(user), password_hash(pass_hash), display_name(display) {}
    User(const std::string& user, const std::string& pass_hash, const std::string& display, const std::vector<std::string>& user_roles)
        : username(user), password_hash(pass_hash), display_name(display), roles(user_roles) {}
};

class AuthManager {
public:
    AuthManager(std::shared_ptr<IUserRepository> user_repository);
    ~AuthManager() = default;
    
    // Authenticate user and return token
    AuthToken authenticate(const std::string& username, const std::string& password);
    
    // Validate token
    bool validate_token(const std::string& token);
    
    // Get username from token
    std::optional<std::string> get_username(const std::string& token);
    
    // Get display name from token
    std::optional<std::string> get_display_name(const std::string& token);
    
    // Get roles from token
    std::optional<std::vector<std::string>> get_roles(const std::string& token);
    
    // Revoke token (logout)
    void revoke_token(const std::string& token);
    
    // Register new user
    bool register_user(const std::string& username, const std::string& password, const std::string& display_name);
    
    // Clean up expired tokens
    void cleanup_expired_tokens();

private:
    std::string generate_token();
    std::string hash_password(const std::string& password);
    
    std::shared_ptr<IUserRepository> user_repository_;
    std::unordered_map<std::string, AuthToken> active_tokens_;  // token -> AuthToken
    mutable std::mutex mutex_;
};
