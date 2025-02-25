#pragma once
#include <string>
#include <memory>
#include "ConnectionHandler.h"
#include "CacheManager.h"
#include "Logger.h"

class ProxyServer {
private:
    int port;
    bool running;
    std::unique_ptr<ConnectionHandler> connectionHandler;
    std::shared_ptr<CacheManager> cacheManager;
    std::shared_ptr<Logger> logger;

public:
    ProxyServer(int port = 8080);
    ~ProxyServer();
    
    void start();
    void stop();
    bool isRunning() const;
}; 