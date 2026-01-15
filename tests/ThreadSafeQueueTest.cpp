#include <gtest/gtest.h>
#include "ThreadSafeQueue.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

class ThreadSafeQueueTest : public ::testing::Test {
protected:
    ThreadSafeQueue<int> queue;
};

// Basic functionality tests
TEST_F(ThreadSafeQueueTest, PushAndPopSingleItem) {
    queue.push(42);
    
    int value;
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 42);
}

TEST_F(ThreadSafeQueueTest, PopFromEmptyQueueWithTimeout) {
    int value;
    auto start = std::chrono::steady_clock::now();
    ASSERT_FALSE(queue.try_pop(value, 50ms));
    auto duration = std::chrono::steady_clock::now() - start;
    
    // Should have waited approximately 50ms
    EXPECT_GE(duration, 50ms);
    EXPECT_LT(duration, 100ms);  // Allow some overhead
}

TEST_F(ThreadSafeQueueTest, TryPopImmediateFromEmptyQueue) {
    int value;
    ASSERT_FALSE(queue.try_pop_immediate(value));
}

TEST_F(ThreadSafeQueueTest, TryPopImmediateFromNonEmptyQueue) {
    queue.push(123);
    
    int value;
    ASSERT_TRUE(queue.try_pop_immediate(value));
    EXPECT_EQ(value, 123);
}

TEST_F(ThreadSafeQueueTest, MultipleItems) {
    for (int i = 0; i < 10; ++i) {
        queue.push(i);
    }
    
    EXPECT_EQ(queue.size(), 10);
    
    for (int i = 0; i < 10; ++i) {
        int value;
        ASSERT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);
    }
    
    EXPECT_TRUE(queue.empty());
}

// Thread safety tests
TEST_F(ThreadSafeQueueTest, ProducerConsumerSingleThread) {
    std::thread producer([this]() {
        for (int i = 0; i < 100; ++i) {
            queue.push(i);
            std::this_thread::sleep_for(1ms);
        }
    });
    
    std::thread consumer([this]() {
        for (int i = 0; i < 100; ++i) {
            int value;
            ASSERT_TRUE(queue.pop(value));
            EXPECT_EQ(value, i);
        }
    });
    
    producer.join();
    consumer.join();
}

TEST_F(ThreadSafeQueueTest, MultipleProducersMultipleConsumers) {
    const int num_producers = 4;
    const int num_consumers = 4;
    const int items_per_producer = 100;
    
    std::atomic<int> total_consumed{0};
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    
    // Start producers
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([this, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                queue.push(i);
            }
        });
    }
    
    // Start consumers
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([this, &total_consumed]() {
            int value;
            while (queue.pop(value)) {
                ++total_consumed;
            }
        });
    }
    
    // Wait for all producers to finish
    for (auto& t : producers) {
        t.join();
    }
    
    // Give consumers time to drain the queue
    std::this_thread::sleep_for(100ms);
    
    // Stop queue to release waiting consumers
    queue.stop();
    
    // Wait for all consumers
    for (auto& t : consumers) {
        t.join();
    }
    
    EXPECT_EQ(total_consumed, num_producers * items_per_producer);
}

// Stop functionality tests
TEST_F(ThreadSafeQueueTest, StopWakesWaitingThread) {
    std::atomic<bool> thread_finished{false};
    
    std::thread consumer([this, &thread_finished]() {
        int value;
        ASSERT_FALSE(queue.pop(value));  // Should return false when stopped
        thread_finished = true;
    });
    
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(thread_finished);
    
    queue.stop();
    
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(thread_finished);
    
    consumer.join();
}

TEST_F(ThreadSafeQueueTest, StopPreventsNewPushes) {
    queue.stop();
    
    queue.push(42);  // Should be ignored
    
    EXPECT_TRUE(queue.empty());
}

TEST_F(ThreadSafeQueueTest, StopAllowsDrainingExistingItems) {
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    queue.stop();
    
    int value;
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 2);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 3);
    
    // Now should return false
    ASSERT_FALSE(queue.pop(value));
}

// Size and empty tests
TEST_F(ThreadSafeQueueTest, SizeTracking) {
    EXPECT_EQ(queue.size(), 0);
    
    queue.push(1);
    EXPECT_EQ(queue.size(), 1);
    
    queue.push(2);
    EXPECT_EQ(queue.size(), 2);
    
    int value;
    queue.pop(value);
    EXPECT_EQ(queue.size(), 1);
    
    queue.pop(value);
    EXPECT_EQ(queue.size(), 0);
}

TEST_F(ThreadSafeQueueTest, EmptyCheck) {
    EXPECT_TRUE(queue.empty());
    
    queue.push(1);
    EXPECT_FALSE(queue.empty());
    
    int value;
    queue.pop(value);
    EXPECT_TRUE(queue.empty());
}

TEST_F(ThreadSafeQueueTest, ClearQueue) {
    for (int i = 0; i < 10; ++i) {
        queue.push(i);
    }
    
    EXPECT_EQ(queue.size(), 10);
    
    queue.clear();
    
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
}

// Move semantics test
TEST_F(ThreadSafeQueueTest, MoveSemantics) {
    std::string str = "Hello, World!";
    queue.push(42);
    
    ThreadSafeQueue<std::string> str_queue;
    str_queue.push(std::move(str));
    
    std::string result;
    ASSERT_TRUE(str_queue.pop(result));
    EXPECT_EQ(result, "Hello, World!");
    EXPECT_TRUE(str.empty());  // Original should be moved from
}

// Stress test
TEST_F(ThreadSafeQueueTest, StressTest) {
    const int num_threads = 8;
    const int operations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    
    // Mix of producers and consumers
    for (int i = 0; i < num_threads; ++i) {
        if (i % 2 == 0) {
            // Producer
            threads.emplace_back([this, operations_per_thread, &push_count]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    queue.push(j);
                    ++push_count;
                }
            });
        } else {
            // Consumer
            threads.emplace_back([this, &pop_count]() {
                int value;
                while (queue.try_pop(value, 10ms)) {
                    ++pop_count;
                }
            });
        }
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Drain remaining items
    int value;
    while (queue.try_pop_immediate(value)) {
        ++pop_count;
    }
    
    EXPECT_EQ(pop_count, push_count);
    EXPECT_TRUE(queue.empty());
}
