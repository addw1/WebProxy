#include "MessageForwarder.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>

std::string MessageForwarder::forwardGet(HttpRequest& req){
    // Call 80 port
    return forwardRequest(req, "80"); 
}

std::string MessageForwarder::forwardPost(HttpRequest& req){
    // Call 80 port
    return forwardRequest(req, "80");
}
    
std::string MessageForwarder::forwardConnect(HttpRequest& req, int clientSock){
    // Extract host and port from url
    std::string hostPort = req.url;
    size_t colonPos = hostPort.find(':');
    std::string host = (colonPos != std::string::npos) ? hostPort.substr(0, colonPos) : hostPort;
    std::string port = (colonPos != std::string::npos) ? hostPort.substr(colonPos + 1) : "443";

    // Create the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "Error: Failed to create socket";

    struct addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (status != 0) {
        close(sock);
        std::cout << "Host: " << host << " - Error: " << gai_strerror(status) << std::endl;
        return "Error: Host not found";
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        close(sock);
        return "Error: Connection failed";
    }
    freeaddrinfo(result);

    // Send 200 OK to client for CONNECT
    std::string connectResponse = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(clientSock, connectResponse.c_str(), connectResponse.length(), 0);

    // Tunnel data
    char buffer[4096];
    while (true) {
        // Get the receive from the client
        int received = recv(clientSock, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        // Send to the server
        send(sock, buffer, received, 0);
        // Receive from the server
        received = recv(sock, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        // Send to the client
        send(clientSock, buffer, received, 0);
        // Test 
        std::cout << "test" << std::endl;
    }
    close(sock);
    // Do not need to return
    return ""; 
}

std::string MessageForwarder::forwardRequest(HttpRequest& req, const std::string& port){
        std::string host = req.headers.at("Host");
        host.erase(0, host.find_first_not_of(" \t\r\n"));
        host.erase(host.find_last_not_of(" \t\r\n") + 1);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return "Error: Failed to create socket";

        struct addrinfo hints{}, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
        if (status != 0) {
            close(sock);
            std::cout << "Host: " << host << " - Error: " << gai_strerror(status) << std::endl;
            return "Error: Host not found";
        }

        if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
            freeaddrinfo(result);
            close(sock);
            return "Error: Connection failed";
        }
        freeaddrinfo(result);

        // Construct the full request from HttpRequest
        std::string fullRequest = req.method + " " + req.url + " " + req.version + "\r\n";
        for (const auto& header : req.headers) {
            fullRequest += header.first + ": " + header.second + "\r\n";
        }
        fullRequest += "\r\n" + req.body;

        ssize_t sent = send(sock, fullRequest.c_str(), fullRequest.length(), 0);
        if (sent < 0) {
            close(sock);
            return "Error: Failed to send request";
        }

        std::string response;
        char buffer[4096];
        while (true) {
            int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived <= 0) break;
            buffer[bytesReceived] = '\0';
            response += buffer;
        }

        close(sock);
        return response;
}