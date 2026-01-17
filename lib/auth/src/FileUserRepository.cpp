#include "auth/FileUserRepository.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

FileUserRepository::FileUserRepository(const std::string& file_path)
    : file_path_(file_path)
{
    load_from_file();
}

void FileUserRepository::load_from_file() {
    std::lock_guard<std::mutex> lock(mutex_);
    users_.clear();
    
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open user file: " << file_path_ << "\n";
        std::cerr << "Creating new empty user database.\n";
        return;
    }
    
    try {
        json j;
        file >> j;
        
        if (j.contains("users") && j["users"].is_array()) {
            for (const auto& user_json : j["users"]) {
                User user;
                user.username = user_json.value("username", "");
                user.password_hash = user_json.value("password_hash", "");
                user.display_name = user_json.value("display_name", "");
                
                if (user_json.contains("roles") && user_json["roles"].is_array()) {
                    for (const auto& role : user_json["roles"]) {
                        user.roles.push_back(role.get<std::string>());
                    }
                }
                
                if (!user.username.empty()) {
                    users_[user.username] = user;
                }
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "Error parsing JSON file: " << e.what() << "\n";
    }
    
    file.close();
}

void FileUserRepository::save_to_file() {
    // Note: mutex should already be locked by caller
    
    std::ofstream file(file_path_);
    if (!file.is_open()) {
        std::cerr << "Error: Could not write to user file: " << file_path_ << "\n";
        return;
    }
    
    json j;
    j["users"] = json::array();
    
    for (const auto& [username, user] : users_) {
        json user_json;
        user_json["username"] = user.username;
        user_json["password_hash"] = user.password_hash;
        user_json["display_name"] = user.display_name;
        user_json["roles"] = user.roles;
        
        j["users"].push_back(user_json);
    }
    
    file << j.dump(2);  // Pretty print with 2-space indent
    file.close();
}

std::future<std::optional<User>> FileUserRepository::find_user(const std::string& username) {
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

std::future<bool> FileUserRepository::create_user(const User& user) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (users_.find(user.username) != users_.end()) {
            promise.set_value(false);  // User already exists
        } else {
            users_[user.username] = user;
            save_to_file();
            promise.set_value(true);
        }
    }
    
    return promise.get_future();
}

std::future<bool> FileUserRepository::update_user(const User& user) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(user.username);
        if (it != users_.end()) {
            it->second = user;
            save_to_file();
            promise.set_value(true);
        } else {
            promise.set_value(false);  // User not found
        }
    }
    
    return promise.get_future();
}

std::future<bool> FileUserRepository::delete_user(const std::string& username) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t removed = users_.erase(username);
        if (removed > 0) {
            save_to_file();
        }
        promise.set_value(removed > 0);
    }
    
    return promise.get_future();
}

std::future<bool> FileUserRepository::user_exists(const std::string& username) {
    std::promise<bool> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        promise.set_value(users_.find(username) != users_.end());
    }
    
    return promise.get_future();
}

std::future<std::vector<User>> FileUserRepository::get_all_users() {
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

std::future<size_t> FileUserRepository::get_user_count() {
    std::promise<size_t> promise;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        promise.set_value(users_.size());
    }
    
    return promise.get_future();
}
