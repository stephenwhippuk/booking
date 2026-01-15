#ifndef THREADSAFEQUEUE_H
#define THREADSAFEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;

public:
    ThreadSafeQueue() = default;
    
    // Prevent copying
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    
    /**
     * Push an item onto the queue (non-blocking except for mutex acquisition)
     * Notifies one waiting thread that an item is available
     */
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            return;  // Don't accept new items if stopped
        }
        queue_.push(item);
        cv_.notify_one();
    }
    
    /**
     * Push an item using move semantics
     */
    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            return;
        }
        queue_.push(std::move(item));
        cv_.notify_one();
    }
    
    /**
     * Try to pop an item from the queue with a timeout
     * Returns true if an item was retrieved, false if timeout or stopped
     */
    bool try_pop(T& item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!cv_.wait_for(lock, timeout, [this]{ 
            return !queue_.empty() || stopped_; 
        })) {
            return false;  // Timeout
        }
        
        if (stopped_ && queue_.empty()) {
            return false;  // Stopped and no items left
        }
        
        if (queue_.empty()) {
            return false;  // Spurious wakeup
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * Try to pop an item immediately without blocking
     * Returns true if an item was retrieved, false otherwise
     */
    bool try_pop_immediate(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty() || stopped_) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * Block until an item is available or queue is stopped
     * Returns true if item was retrieved, false if stopped
     */
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        cv_.wait(lock, [this]{ 
            return !queue_.empty() || stopped_; 
        });
        
        if (stopped_ && queue_.empty()) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * Stop the queue - wakes all waiting threads
     * After stop(), push() will be ignored and pop operations will return false
     */
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }
    
    /**
     * Check if queue is empty (thread-safe)
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * Get current size of queue (thread-safe)
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    /**
     * Check if queue has been stopped
     */
    bool is_stopped() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopped_;
    }
    
    /**
     * Clear all items from queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }
};

#endif // THREADSAFEQUEUE_H
