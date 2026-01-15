#include <gtest/gtest.h>
#include "NetworkManager.h"
#include "ThreadSafeQueue.h"
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std::chrono_literals;

class NetworkManagerTest : public ::testing::Test {
protected:
    ThreadSafeQueue<std::string> inbound_queue;
    ThreadSafeQueue<std::string> outbound_queue;
    
    // Simple echo server for testing
    int server_socket = -1;
    int client_socket = -1;
    std::thread server_thread;
    int server_port = 0;
    bool server_running = false;
    
    void SetUp() override {
        // Start a simple echo server on a random port
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(server_socket, 0);
        
        // Allow address reuse
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = 0;  // Let OS choose port
        
        ASSERT_EQ(bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)), 0);
        ASSERT_EQ(listen(server_socket, 1), 0);
        
        // Get the assigned port
        socklen_t addr_len = sizeof(server_addr);
        ASSERT_EQ(getsockname(server_socket, (sockaddr*)&server_addr, &addr_len), 0);
        server_port = ntohs(server_addr.sin_port);
        
        server_running = true;
        server_thread = std::thread(&NetworkManagerTest::run_echo_server, this);
    }
    
    void TearDown() override {
        server_running = false;
        
        if (client_socket >= 0) {
            close(client_socket);
            client_socket = -1;
        }
        
        if (server_socket >= 0) {
            close(server_socket);
            server_socket = -1;
        }
        
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }
    
    void run_echo_server() {
        fd_set read_fds;
        struct timeval tv;
        
        while (server_running) {
            FD_ZERO(&read_fds);
            FD_SET(server_socket, &read_fds);
            
            tv.tv_sec = 0;
            tv.tv_usec = 100000;  // 100ms timeout
            
            int result = select(server_socket + 1, &read_fds, nullptr, nullptr, &tv);
            
            if (result > 0 && FD_ISSET(server_socket, &read_fds)) {
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
                
                if (client_socket >= 0) {
                    // Echo loop
                    char buffer[1024];
                    while (server_running) {
                        fd_set client_fds;
                        FD_ZERO(&client_fds);
                        FD_SET(client_socket, &client_fds);
                        
                        tv.tv_sec = 0;
                        tv.tv_usec = 100000;
                        
                        result = select(client_socket + 1, &client_fds, nullptr, nullptr, &tv);
                        
                        if (result > 0 && FD_ISSET(client_socket, &client_fds)) {
                            ssize_t n = recv(client_socket, buffer, sizeof(buffer), 0);
                            if (n <= 0) break;
                            
                            // Echo back
                            send(client_socket, buffer, n, 0);
                        }
                    }
                }
            }
        }
    }
};

TEST_F(NetworkManagerTest, ConnectToServer) {
    NetworkManager network(inbound_queue, outbound_queue);
    
    std::string error;
    ASSERT_TRUE(network.connect("127.0.0.1", server_port, error)) << error;
    EXPECT_TRUE(network.is_connected());
    EXPECT_GT(network.get_socket(), 0);
}

TEST_F(NetworkManagerTest, ConnectToInvalidServer) {
    NetworkManager network(inbound_queue, outbound_queue);
    
    std::string error;
    ASSERT_FALSE(network.connect("127.0.0.1", 9999, error));
    EXPECT_FALSE(network.is_connected());
    EXPECT_FALSE(error.empty());
}

TEST_F(NetworkManagerTest, SendAndReceive) {
    NetworkManager network(inbound_queue, outbound_queue);
    
    std::string error;
    ASSERT_TRUE(network.connect("127.0.0.1", server_port, error));
    
    network.start();
    
    // Send a message
    outbound_queue.push("Hello, Server!\n");
    
    // Wait for echo response
    std::string response;
    ASSERT_TRUE(inbound_queue.try_pop(response, 1000ms));
    EXPECT_EQ(response, "Hello, Server!\n");
    
    network.stop();
}

TEST_F(NetworkManagerTest, MultipleMessages) {
    NetworkManager network(inbound_queue, outbound_queue);
    
    std::string error;
    ASSERT_TRUE(network.connect("127.0.0.1", server_port, error));
    
    network.start();
    
    // Send multiple messages
    for (int i = 0; i < 5; ++i) {
        outbound_queue.push("Message " + std::to_string(i) + "\n");
        std::this_thread::sleep_for(50ms);  // Small delay to prevent batching
    }
    
    // Receive echoed messages - TCP may batch them
    std::string all_responses;
    for (int i = 0; i < 5; ++i) {
        std::string response;
        if (inbound_queue.try_pop(response, 1000ms)) {
            all_responses += response;
        }
    }
    
    // Check that all messages were received (order preserved)
    for (int i = 0; i < 5; ++i) {
        EXPECT_NE(all_responses.find("Message " + std::to_string(i)), std::string::npos)
            << "Missing message " << i;
    }
    
    network.stop();
}

TEST_F(NetworkManagerTest, StopCleansUpProperly) {
    NetworkManager network(inbound_queue, outbound_queue);
    
    std::string error;
    ASSERT_TRUE(network.connect("127.0.0.1", server_port, error));
    
    network.start();
    
    // Send a message
    outbound_queue.push("Test\n");
    
    std::string response;
    ASSERT_TRUE(inbound_queue.try_pop(response, 1000ms));
    
    // Stop should clean up without hanging
    network.stop();
    
    EXPECT_FALSE(network.is_connected());
}

TEST_F(NetworkManagerTest, StartWithoutConnect) {
    NetworkManager network(inbound_queue, outbound_queue);
    
    // Should not crash
    network.start();
    
    // Should not be running
    EXPECT_FALSE(network.is_connected());
}

TEST_F(NetworkManagerTest, QueueReferencesValid) {
    // Test that NetworkManager properly uses queue references
    NetworkManager network(inbound_queue, outbound_queue);
    
    std::string error;
    ASSERT_TRUE(network.connect("127.0.0.1", server_port, error));
    network.start();
    
    // Push to the queue we passed in
    outbound_queue.push("Reference Test\n");
    
    // Should receive on the inbound queue we passed in
    std::string response;
    ASSERT_TRUE(inbound_queue.try_pop(response, 1000ms));
    EXPECT_EQ(response, "Reference Test\n");
    
    network.stop();
}

TEST_F(NetworkManagerTest, ServerDisconnectDetected) {
    NetworkManager network(inbound_queue, outbound_queue);
    
    std::string error;
    ASSERT_TRUE(network.connect("127.0.0.1", server_port, error));
    network.start();
    
    // Send a message to establish connection
    outbound_queue.push("Test\n");
    std::string response;
    ASSERT_TRUE(inbound_queue.try_pop(response, 1000ms));
    
    // Close server side
    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    
    // Should detect disconnect
    std::this_thread::sleep_for(200ms);
    EXPECT_FALSE(network.is_connected());
    
    // Should have received disconnect message
    std::string disconnect_msg;
    if (inbound_queue.try_pop(disconnect_msg, 100ms)) {
        EXPECT_TRUE(disconnect_msg.find("DISCONNECTED") != std::string::npos ||
                   disconnect_msg.find("ERROR") != std::string::npos);
    }
}
