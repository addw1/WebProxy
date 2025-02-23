#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class ProxyClient {
private:
    int clientSocket;
    const char* proxyHost;
    int proxyPort;
    static const int BUFFER_SIZE = 4096;

public:
    ProxyClient(const char* host, int port) : proxyHost(host), proxyPort(port) {
        // Create the client socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket < 0) {
            throw std::runtime_error("Failed to create socket");
        }
    }

    ~ProxyClient() {
        if (clientSocket >= 0) {
            close(clientSocket);
        }
    }

    bool connect() {
        struct sockaddr_in proxyAddr;
        proxyAddr.sin_family = AF_INET;
        proxyAddr.sin_port = htons(proxyPort);
        
        if (inet_pton(AF_INET, proxyHost, &proxyAddr.sin_addr) <= 0) {
            std::cerr << "Invalid address" << std::endl;
            return false;
        }

        if (::connect(clientSocket, (struct sockaddr*)&proxyAddr, sizeof(proxyAddr)) < 0) {
            std::cerr << "Connection failed" << std::endl;
            return false;
        }

        return true;
    }

    bool sendRequest(const std::string& request) {
        if (send(clientSocket, request.c_str(), request.length(), 0) < 0) {
            std::cerr << "Failed to send request" << std::endl;
            return false;
        }
        return true;
    }

    std::string receiveResponse() {
        char buffer[BUFFER_SIZE];
        std::string response;
        ssize_t bytesRead;
        // Loop until get the response
        while ((bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
            
            // Simple check for HTTP response completion
            if (response.find("\r\n\r\n") != std::string::npos) {
                break;
            }
        }

        return response;
    }
};

// Test functions
void testBasicRequest() {
    std::cout << "\n=== Testing Basic HTTP Request ===" << std::endl;
    try {
        ProxyClient client("127.0.0.1", 12345);
        // Connect with the web proxy
        if (!client.connect()) {
            std::cout << "Failed to connect to proxy server" << std::endl;
            return;
        }

        // Create a simple HTTP request
        std::string request = 
        "GET http://google.com/ HTTP/1.1\r\n"
        "Host: google.com\r\n"
        "Connection: close\r\n"
        "\r\n";
        

        if (!client.sendRequest(request)) {
            std::cout << "Failed to send request" << std::endl;
            return;
        }

        std::string response = client.receiveResponse();
        std::cout << "Response received:\n" << response << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void testMultipleRequests() {
    std::cout << "\n=== Testing Multiple Sequential Requests ===" << std::endl;
    try {
        for (int i = 0; i < 3; i++) {
            ProxyClient client("127.0.0.1", 8080);
            
            if (!client.connect()) {
                std::cout << "Failed to connect to proxy server" << std::endl;
                continue;
            }

            std::string request = 
                "GET http://example.com/ HTTP/1.1\r\n"
                "Host: example.com\r\n"
                "Connection: close\r\n"
                "\r\n";

            if (!client.sendRequest(request)) {
                std::cout << "Failed to send request" << std::endl;
                continue;
            }

            std::string response = client.receiveResponse();
            std::cout << "Request " << (i + 1) << " response length: " 
                     << response.length() << " bytes" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void testInvalidRequest() {
    std::cout << "\n=== Testing Invalid Request ===" << std::endl;
    try {
        ProxyClient client("127.0.0.1", 8080);
        
        if (!client.connect()) {
            std::cout << "Failed to connect to proxy server" << std::endl;
            return;
        }

        // Send an invalid request
        std::string request = "INVALID REQUEST\r\n\r\n";

        if (!client.sendRequest(request)) {
            std::cout << "Failed to send request" << std::endl;
            return;
        }

        std::string response = client.receiveResponse();
        std::cout << "Response to invalid request:\n" << response << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "Starting proxy client tests..." << std::endl;

    // Run tests
    testBasicRequest();
    // testMultipleRequests();
    // testInvalidRequest();

    return 0;
} 