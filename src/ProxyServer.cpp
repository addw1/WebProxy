#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <stdexcept>

#include "ProxyServer.h"
#include "Logger.h"
#include "CacheManager.h"
#include "RequestHandler.h"
#include "ConnectionHandler.h"

#define BUFFER_SIZE 4096  // 4 KB buffer


ProxyServer::ProxyServer(int port) : port(port), running(false) {
    logger = std::make_shared<Logger>("logs/proxy.log");
    cacheManager = std::make_shared<CacheManager>();
    auto requestHandler = std::make_shared<RequestHandler>(cacheManager, logger);
    connectionHandler = std::make_unique<ConnectionHandler>(requestHandler, logger);
}

ProxyServer::~ProxyServer() {
    stop();
}

void ProxyServer::start() {
    // Prevent creating multi instances in one server
    if (running) {
        return;
    }
    // Update the state
    running = true;
    // Write the log file
    logger->log(Logger::INFO, "Starting proxy server on port " + std::to_string(port));
    connectionHandler->start(port);
}

void ProxyServer::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    logger->log(Logger::INFO, "Stopping proxy server");
    connectionHandler->stop();
}

bool ProxyServer::isRunning() const {
    return running;
}


void ProxyServer::handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    std::memset(buffer, 0, BUFFER_SIZE);

    // Receive request from client
    ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesRead < 0) {
        std::cerr << "Error reading from client" << std::endl;
        close(clientSocket);
        return;
    }

    // Create connection to target server
    int targetSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (targetSocket < 0) {
        std::cerr << "Failed to create target socket" << std::endl;
        close(clientSocket);
        return;
    }

    // Parse target host and port from request
    // This is a simplified version - you'd need proper HTTP parsing in production
    std::string request(buffer);
    size_t hostPos = request.find("Host: ");
    if (hostPos == std::string::npos) {
        close(clientSocket);
        close(targetSocket);
        return;
    }

    size_t hostEnd = request.find("\r\n", hostPos);
    std::string host = request.substr(hostPos + 6, hostEnd - (hostPos + 6));
    int targetPort = 80; // Default HTTP port

    // Connect to target server
    struct sockaddr_in targetAddr;
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(targetPort);
    if (inet_pton(AF_INET, host.c_str(), &targetAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address" << std::endl;
        close(clientSocket);
        close(targetSocket);
        return;
    }

    if (connect(targetSocket, (struct sockaddr*)&targetAddr, sizeof(targetAddr)) < 0) {
        std::cerr << "Connection to target server failed" << std::endl;
        close(clientSocket);
        close(targetSocket);
        return;
    }

    // Forward request to target server
    send(targetSocket, buffer, bytesRead, 0);

    // Receive response from target and forward to client
    while ((bytesRead = recv(targetSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        send(clientSocket, buffer, bytesRead, 0);
    }

    close(targetSocket);
    close(clientSocket);
}