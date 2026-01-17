#pragma once

#include "AuthManager.h"
#include <string>
#include <optional>
#include <future>
#include <vector>

/**
 * IUserRepository - Abstract interface for user data access
 * 
 * This interface provides async-ready methods for user management.
 * All methods return futures to support asynchronous operations
 * when backed by databases or remote storage.
 */
class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    
    /**
     * Find a user by username
     * Returns User if found, nullopt otherwise
     */
    virtual std::future<std::optional<User>> find_user(const std::string& username) = 0;
    
    /**
     * Create a new user
     * Returns true if created, false if user already exists
     */
    virtual std::future<bool> create_user(const User& user) = 0;
    
    /**
     * Update an existing user
     * Returns true if updated, false if user not found
     */
    virtual std::future<bool> update_user(const User& user) = 0;
    
    /**
     * Delete a user by username
     * Returns true if deleted, false if user not found
     */
    virtual std::future<bool> delete_user(const std::string& username) = 0;
    
    /**
     * Check if a user exists
     */
    virtual std::future<bool> user_exists(const std::string& username) = 0;
    
    /**
     * Get all users (for admin purposes)
     */
    virtual std::future<std::vector<User>> get_all_users() = 0;
    
    /**
     * Get count of users
     */
    virtual std::future<size_t> get_user_count() = 0;
};
