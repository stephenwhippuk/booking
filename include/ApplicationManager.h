#ifndef APPLICATIONMANAGER_H
#define APPLICATIONMANAGER_H

#include "ThreadSafeQueue.h"
#include "ApplicationState.h"
#include "UICommand.h"
#include "RoomInfo.h"
#include <string>
#include <thread>
#include <atomic>

/**
 * ApplicationManager - The business logic layer
 * 
 * Responsibilities:
 * - Consume network_inbound queue (raw server messages)
 * - Parse messages and update ApplicationState
 * - Generate outbound network messages
 * - Generate UI commands for presentation
 * - Handle user input events
 * 
 * This is the central "brain" of the application that ties everything together.
 */
class ApplicationManager {
private:
    // Queue references (owned externally)
    ThreadSafeQueue<std::string>& network_inbound_;
    ThreadSafeQueue<std::string>& network_outbound_;
    ThreadSafeQueue<UICommand>& ui_commands_;
    ThreadSafeQueue<std::string>& input_events_;
    
    // Application state (owned by this manager)
    ApplicationState state_;
    
    // Thread control
    std::thread app_thread_;
    std::atomic<bool> running_;
    
    // Internal processing methods
    void application_loop();
    void process_network_message(const std::string& message);
    void process_input_event(const std::string& event);
    
    // Protocol parsing
    std::vector<RoomInfo> parse_room_list(const std::string& data);
    bool is_in_room() const;
    
    // State tracking for protocol
    bool in_room_;

public:
    /**
     * Constructor takes references to all queues
     * Queues must outlive this ApplicationManager instance
     */
    ApplicationManager(
        ThreadSafeQueue<std::string>& network_inbound,
        ThreadSafeQueue<std::string>& network_outbound,
        ThreadSafeQueue<UICommand>& ui_commands,
        ThreadSafeQueue<std::string>& input_events
    );
    
    ~ApplicationManager();
    
    // Prevent copying
    ApplicationManager(const ApplicationManager&) = delete;
    ApplicationManager& operator=(const ApplicationManager&) = delete;
    
    /**
     * Start the application processing thread
     */
    void start();
    
    /**
     * Stop the application processing thread
     */
    void stop();
    
    /**
     * Check if application is running
     */
    bool is_running() const;
    
    /**
     * Get current application state (for testing)
     */
    const ApplicationState& get_state() const;
};

#endif // APPLICATIONMANAGER_H
