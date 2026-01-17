#pragma once

#include "IUserRepository.h"
#include <string>
#include <mutex>

/**
 * FileUserRepository - JSON file-based implementation of user repository
 * 
 * Stores users in a JSON file with format:
 * {
 *   "users": [
 *     {"username": "...", "password_hash": "...", "display_name": "...", "roles": ["role1", "role2"]},
 *     ...
 *   ]
 * }
 * Thread-safe with mutex protection.
 * Loads all users into memory on construction and writes back on modifications.
 */
class FileUserRepository : public IUserRepository {
public:
    FileUserRepository(const std::string& file_path);
    ~FileUserRepository() override = default;
    
    std::future<std::optional<User>> find_user(const std::string& username) override;
    std::future<bool> create_user(const User& user) override;
    std::future<bool> update_user(const User& user) override;
    std::future<bool> delete_user(const std::string& username) override;
    std::future<bool> user_exists(const std::string& username) override;
    std::future<std::vector<User>> get_all_users() override;
    std::future<size_t> get_user_count() override;
    
private:
    void load_from_file();
    void save_to_file();
    
    std::string file_path_;
    std::unordered_map<std::string, User> users_;
    mutable std::mutex mutex_;
};
