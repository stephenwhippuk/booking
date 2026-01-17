#include "auth/InMemoryUserRepository.h"
#include <algorithm>
#include <sstream>

InMemoryUserRepository::InMemoryUserRepository() {
    // Initialize with test user for development
    User test_user;
    test_user.username = "test";
    test_user.display_name = "Test User";
    // Pre-hashed password for "test123" (using simple hash from AuthManager)
    std::hash<std::string> hasher;
    std::stringstream ss;
    ss << std::hex << hasher("test123" + std::string("salt_value"));
    test_user.password_hash = ss.str();
    
    users_[test_user.username] = test_user;
}

std::future<std::optional<User>> InMemoryUserRepository::find_user(const std::string& username) {
    std::promise<std::optional<User>> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(username);
        if (it != users_.end()) {
            promise.set_value(it->second);
        } else {
            promise.set_value(std::nullopt);
        }
    }
    
    return promise.get_future();
}

std::future<bool> InMemoryUserRepository::create_user(const User& user) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (users_.find(user.username) != users_.end()) {
            promise.set_value(false);  // User already exists
        } else {
            users_[user.username] = user;
            promise.set_value(true);
        }
    }
    
    return promise.get_future();
}

std::future<bool> InMemoryUserRepository::update_user(const User& user) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(user.username);
        if (it != users_.end()) {
            it->second = user;
            promise.set_value(true);
        } else {
            promise.set_value(false);  // User not found
        }
    }
    
    return promise.get_future();
}

std::future<bool> InMemoryUserRepository::delete_user(const std::string& username) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t removed = users_.erase(username);
        promise.set_value(removed > 0);
    }
    
    return promise.get_future();
}

std::future<bool> InMemoryUserRepository::user_exists(const std::string& username) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        promise.set_value(users_.find(username) != users_.end());
    }
    
    return promise.get_future();
}

std::future<std::vector<User>> InMemoryUserRepository::get_all_users() {
    std::promise<std::vector<User>> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<User> users;
        users.reserve(users_.size());
        
        for (const auto& [username, user] : users_) {
            users.push_back(user);
        }
        
        promise.set_value(std::move(users));
    }
    
    return promise.get_future();
}

std::future<size_t> InMemoryUserRepository::get_user_count() {
    std::promise<size_t> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        promise.set_value(users_.size());
    }
    
    return promise.get_future();
}
