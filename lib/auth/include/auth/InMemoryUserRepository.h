#pragma once

#include "IUserRepository.h"
#include <unordered_map>
#include <mutex>

/**
 * InMemoryUserRepository - In-memory implementation of user repository
 * 
 * Stores users in a hash map. Not persistent across restarts.
 * Thread-safe with mutex protection.
 */
class InMemoryUserRepository : public IUserRepository {
public:
    InMemoryUserRepository();
    ~InMemoryUserRepository() override = default;
    
    std::future<std::optional<User>> find_user(const std::string& username) override;
    std::future<bool> create_user(const User& user) override;
    std::future<bool> update_user(const User& user) override;
    std::future<bool> delete_user(const std::string& username) override;
    std::future<bool> user_exists(const std::string& username) override;
    std::future<std::vector<User>> get_all_users() override;
    std::future<size_t> get_user_count() override;
    
private:
    std::unordered_map<std::string, User> users_;
    mutable std::mutex mutex_;
};
