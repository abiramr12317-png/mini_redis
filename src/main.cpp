#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "store.hpp"

using namespace std;

int main() {
    miniredis::Store store(3); // Set a maximum of 3 keys for testing eviction
    string line;

    cout << "mini-redis : type SET/GET/DEL, or QUIT to exit\n";

    while (getline(cin, line)) {
        istringstream iss(line);
        vector<string> tokens;
        string tok;
        while (iss >> tok) {
            tokens.push_back(tok);
        }

        if (tokens.empty()) {
            continue;
        }

        const string& cmd = tokens[0];

        if (cmd == "SET") {
            if (tokens.size() != 3 && tokens.size() != 5) {
                cout << "ERR wrong number of arguments for SET\n";
                continue;
            }
            chrono::seconds ttl(0);
            if (tokens.size() == 5) {
                if (tokens[3] != "EX") {
                    cout << "ERR expected EX before ttl value\n";
                    continue;
                }
                ttl = chrono::seconds(stoi(tokens[4]));
            }
            store.set(tokens[1], tokens[2], ttl);
            cout << "OK\n";
        } 
        
        else if (cmd == "GET") {  
            if (tokens.size() != 2) {
                cout << "ERR wrong number of arguments for GET\n";
                continue;
            }
            auto result = store.get(tokens[1]);
            if (result.has_value()) {
                cout << *result << "\n";
            } else {
                cout << "(nil)\n";
            }

        } 
        
        else if (cmd == "DEL") {
            if (tokens.size() != 2) {
                cout << "ERR wrong number of arguments for DEL\n";
                continue;
            }
            bool deleted = store.del(tokens[1]);
            cout << (deleted ? "1" : "0") << "\n";

        } 
        
        else if (cmd == "QUIT") {
            break;

        } 
        
        else {
            cout << "ERR unknown command: " << cmd << "\n";
        }
    }

    return 0;
}