#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "store.hpp"

int main() {
    miniredis::Store store;
    std::string line;

    std::cout << "mini-redis : type SET/GET/DEL, or QUIT to exit\n";

    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) {
            tokens.push_back(tok);
        }

        if (tokens.empty()) {
            continue;
        }

        const std::string& cmd = tokens[0];

        if (cmd == "SET") {
            if (tokens.size() != 3 && tokens.size() != 5) {
                std::cout << "ERR wrong number of arguments for SET\n";
                continue;
            }
            std::chrono::seconds ttl(0);
            if (tokens.size() == 5) {
                if (tokens[3] != "EX") {
                    std::cout << "ERR expected EX before ttl value\n";
                    continue;
                }
                ttl = std::chrono::seconds(std::stoi(tokens[4]));
            }
            store.set(tokens[1], tokens[2], ttl);
            std::cout << "OK\n";
        } 
        
        else if (cmd == "GET") {  
            if (tokens.size() != 2) {
                std::cout << "ERR wrong number of arguments for GET\n";
                continue;
            }
            auto result = store.get(tokens[1]);
            if (result.has_value()) {
                std::cout << *result << "\n";
            } else {
                std::cout << "(nil)\n";
            }

        } 
        
        else if (cmd == "DEL") {
            if (tokens.size() != 2) {
                std::cout << "ERR wrong number of arguments for DEL\n";
                continue;
            }
            bool deleted = store.del(tokens[1]);
            std::cout << (deleted ? "1" : "0") << "\n";

        } 
        
        else if (cmd == "QUIT") {
            break;

        } 
        
        else {
            std::cout << "ERR unknown command: " << cmd << "\n";
        }
    }

    return 0;
}