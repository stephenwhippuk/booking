#include "auth/AuthManager.h"
#include "auth/IUserRepository.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

AuthManager::AuthManager(std::shared_ptr<IUserRepository> user_repository)
    : user_repository_(user_repository)
{
}

AuthToken AuthManager::authenticate(const std::string& username, const std::string& password) {
    // Find user from repository
    auto user_future = user_repository_->find_user(username);
    auto user_opt = user_future.get();
    
    if (!user_opt) {
        return AuthToken();  // Invalid token
    }
    
    auto& user = *user_opt;
    std::string password_hash = hash_password(password);
    if (user.password_hash != password_hash) {
        return AuthToken();  // Invalid token
    }
    
    // Generate new token
    std::lock_guard<std::mutex> lock(mutex_);
    std::string token = generate_token();
    AuthToken auth_token(token, username, user.display_name, user.roles);
    active_tokens_[token] = auth_token;
    
    return auth_token;
}

bool AuthManager::validate_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = active_tokens_.find(token);
    if (it == active_tokens_.end()) {
        return false;
    }
    
    if (it->second.is_expired()) {
        active_tokens_.erase(it);
        return false;
    }
    
    return it->second.is_valid;
}

std::optional<std::string> AuthManager::get_username(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = active_tokens_.find(token);
    if (it == active_tokens_.end()) {
        return std::nullopt;
    }
    
    if (it->second.is_expired()) {
        active_tokens_.erase(it);
        return std::nullopt;
    }
    
    return it->second.username;
}

std::optional<std::string> AuthManager::get_display_name(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = active_tokens_.find(token);
    if (it == active_tokens_.end()) {
        return std::nullopt;
    }
    
    if (it->second.is_expired()) {
        active_tokens_.erase(it);
        return std::nullopt;
    }
    
    return it->second.display_name;
}

std::optional<std::vector<std::string>> AuthManager::get_roles(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = active_tokens_.find(token);
    if (it == active_tokens_.end()) {
        return std::nullopt;
    }
    
    if (it->second.is_expired()) {
        active_tokens_.erase(it);
        return std::nullopt;
    }
    
    return it->second.roles;
}

void AuthManager::revoke_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_tokens_.erase(token);
}

bool AuthManager::register_user(const std::string& username, const std::string& password, const std::string& display_name) {
    User user(username, hash_password(password), display_name);
    auto result_future = user_repository_->create_user(user);
    return result_future.get();
}

void AuthManager::cleanup_expired_tokens() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = active_tokens_.begin();
    while (it != active_tokens_.end()) {
        if (it->second.is_expired()) {
            it = active_tokens_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string AuthManager::generate_token() {
    // Generate a random token (32 hex characters)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    
    return ss.str();
}

std::string AuthManager::hash_password(const std::string& password) {
    // Simple hash for demonstration (in production, use bcrypt/argon2)
    std::hash<std::string> hasher;
    std::stringstream ss;
    ss << std::hex << hasher(password + "salt_value");
    return ss.str();
}
