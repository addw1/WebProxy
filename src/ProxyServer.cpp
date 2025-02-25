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