#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>

constexpr int BUFFER_SIZE = 4096;

class ServerConnection {
private:
    int sock_;
    std::atomic<bool> connected_{false};
    std::thread receive_thread_;
    std::function<void(const std::string&)> message_callback_;
    std::function<void()> disconnect_callback_;

    void receive_loop();

public:
    ServerConnection();
    ~ServerConnection();

    bool connect_to_server(const std::string& ip, int port, std::string& error_msg);
    bool receive_protocol_message(std::string& message, std::string& error_msg);
    bool send_message(const std::string& message);
    void start_receiving(std::function<void(const std::string&)> on_message,
                        std::function<void()> on_disconnect);
    void disconnect();
    bool is_connected() const;
};
