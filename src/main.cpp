#include "ProxyServer.h"
#include <iostream>

int main() {
    try {
        // Create the server and listen at 8080
        ProxyServer server(12345);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
} 