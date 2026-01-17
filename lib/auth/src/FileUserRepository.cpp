#include "auth/FileUserRepository.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

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
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;  // Skip empty lines and comments
        }
        
        std::istringstream iss(line);
        std::string username, password_hash, display_name;
        
        // Parse CSV: username,password_hash,display_name
        if (std::getline(iss, username, ',') &&
            std::getline(iss, password_hash, ',') &&
            std::getline(iss, display_name)) {
            
            User user;
            user.username = unescape_csv(username);
            user.password_hash = unescape_csv(password_hash);
            user.display_name = unescape_csv(display_name);
            
            users_[user.username] = user;
        }
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
    
    file << "# User database - Format: username,password_hash,display_name\n";
    
    for (const auto& [username, user] : users_) {
        file << escape_csv(user.username) << ","
             << escape_csv(user.password_hash) << ","
             << escape_csv(user.display_name) << "\n";
    }
    
    file.close();
}

std::string FileUserRepository::escape_csv(const std::string& str) {
    // Simple CSV escaping: quote strings with commas or quotes
    if (str.find(',') != std::string::npos || str.find('"') != std::string::npos) {
        std::string escaped = "\"";
        for (char ch : str) {
            if (ch == '"') {
                escaped += "\"\"";  // Double quotes
            } else {
                escaped += ch;
            }
        }
        escaped += "\"";
        return escaped;
    }
    return str;
}

std::string FileUserRepository::unescape_csv(const std::string& str) {
    if (str.empty() || str[0] != '"') {
        return str;
    }
    
    std::string unescaped;
    bool in_quote = false;
    
    for (size_t i = 1; i < str.length() - 1; ++i) {
        if (str[i] == '"' && i + 1 < str.length() && str[i + 1] == '"') {
            unescaped += '"';
            ++i;  // Skip next quote
        } else {
            unescaped += str[i];
        }
    }
    
    return unescaped;
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
