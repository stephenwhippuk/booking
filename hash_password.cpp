#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

std::string hash_password(const std::string& password) {
    std::hash<std::string> hasher;
    std::stringstream ss;
    ss << std::hex << hasher(password + "salt_value");
    return ss.str();
}

int main() {
    std::cout << "Stephen,Password -> " << hash_password("Password") << std::endl;
    std::cout << "John,Password -> " << hash_password("Password") << std::endl;
    std::cout << "David,Password -> " << hash_password("Password") << std::endl;
    return 0;
}
