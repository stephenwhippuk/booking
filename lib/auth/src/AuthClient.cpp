#include "auth/AuthClient.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

AuthClient::AuthClient(const std::string& host, int port)
    : host_(host), port_(port)
{
}

AuthResult AuthClient::authenticate(const std::string& username, const std::string& password) {
    AuthResult result;
    
    std::string command = "AUTH " + username + " " + password + "\n";
    std::string response = send_command(command);
    
    if (response.empty()) {
        result.error_message = "No response from auth server";
        return result;
    }
    
    std::istringstream iss(response);
    std::string status;
    iss >> status;
    
    if (status == "OK") {
        iss >> result.token;
        // Read rest of line as display name (may contain spaces)
        std::getline(iss >> std::ws, result.display_name);
        result.success = true;
    } else {
        result.error_message = "Authentication failed";
    }
    
    return result;
}

bool AuthClient::validate_token(const std::string& token) {
    std::string command = "VALIDATE " + token + "\n";
    std::string response = send_command(command);
    
    return response.find("VALID") == 0;
}

std::optional<UserInfo> AuthClient::get_user_info(const std::string& token) {
    std::string command = "GETUSER " + token + "\n";
    std::string response = send_command(command);
    
    if (response.empty() || response.find("NOTFOUND") == 0) {
        return std::nullopt;
    }
    
    std::istringstream iss(response);
    std::string status;
    iss >> status;
    
    if (status == "USER") {
        UserInfo info;
        std::string display_and_roles;
        iss >> info.username;
        // Read rest of line as display name and roles
        std::getline(iss >> std::ws, display_and_roles);
        
        // Split display_and_roles by last space to separate display name from roles
        size_t last_space = display_and_roles.rfind(' ');
        if (last_space != std::string::npos) {
            info.display_name = display_and_roles.substr(0, last_space);
            std::string roles_str = display_and_roles.substr(last_space + 1);
            
            // Parse roles (semicolon-separated)
            if (!roles_str.empty()) {
                std::istringstream roles_iss(roles_str);
                std::string role;
                while (std::getline(roles_iss, role, ';')) {
                    if (!role.empty()) {
                        info.roles.push_back(role);
                    }
                }
            }
        } else {
            // No roles, display name is the entire string
            info.display_name = display_and_roles;
        }
        return info;
    }
    
    return std::nullopt;
}

bool AuthClient::register_user(const std::string& username, const std::string& password,
                               const std::string& display_name) {
    std::string command = "REGISTER " + username + " " + password + " " + display_name + "\n";
    std::string response = send_command(command);
    
    return response.find("REGISTERED") == 0;
}

bool AuthClient::revoke_token(const std::string& token) {
    std::string command = "REVOKE " + token + "\n";
    std::string response = send_command(command);
    
    return response.find("REVOKED") == 0;
}

std::string AuthClient::send_command(const std::string& command) {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return "";
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect to server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return "";
    }
    
    // Send command
    ssize_t sent = send(sock, command.c_str(), command.length(), 0);
    if (sent < 0) {
        close(sock);
        return "";
    }
    
    // Read response
    char buffer[4096];
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    
    close(sock);
    
    if (received <= 0) {
        return "";
    }
    
    buffer[received] = '\0';
    std::string response(buffer);
    
    // Remove trailing newline
    if (!response.empty() && response.back() == '\n') {
        response.pop_back();
    }
    
    return response;
}
