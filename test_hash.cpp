#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <functional>

std::string hash_password(const std::string& password) {
    std::hash<std::string> hasher;
    std::stringstream ss;
    ss << std::hex << hasher(password + "salt_value");
    return ss.str();
}

int main() {
    std::cout << "Hash for 'Password': " << hash_password("Password") << std::endl;
    return 0;
}
