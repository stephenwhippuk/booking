#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "ThreadSafeQueue.h"
#include <string>
#include <thread>
#include <atomic>

/**
 * NetworkManager - Pure TCP I/O layer
 * 
 * Responsibilities:
 * - TCP socket management (connect/disconnect)
 * - Receive data from socket -> enqueue to inbound queue
 * - Dequeue from outbound queue -> send to socket
 * 
 * Does NOT:
 * - Parse messages
 * - Handle business logic
 * - Know about events or UI
 * 
 * This is a completely protocol-agnostic transport layer.
 */
class NetworkManager {
private:
    // Queue references (owned by application layer)
    ThreadSafeQueue<std::string>& inbound_queue_;
    ThreadSafeQueue<std::string>& outbound_queue_;
    
    // Socket state
    int socket_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    
    // Network I/O thread
    std::thread network_thread_;
    
    // Internal methods
    void network_loop();
    void receive_data();
    void send_data();

public:
    /**
     * Constructor takes references to existing queues
     * Queues must outlive this NetworkManager instance
     */
    NetworkManager(ThreadSafeQueue<std::string>& inbound,
                   ThreadSafeQueue<std::string>& outbound);
    
    ~NetworkManager();
    
    // Prevent copying
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    
    /**
     * Connect to server
     * Returns true on success, false on failure
     */
    bool connect(const std::string& host, int port, std::string& error_msg);
    
    /**
     * Start the network I/O thread
     * Must be called after connect()
     */
    void start();
    
    /**
     * Stop the network I/O thread and disconnect
     */
    void stop();
    
    /**
     * Check if currently connected
     */
    bool is_connected() const;
    
    /**
     * Get socket file descriptor (for testing/debugging)
     */
    int get_socket() const;
};

#endif // NETWORKMANAGER_H
