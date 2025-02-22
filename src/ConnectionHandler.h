#pragma once
#include <vector>
#include <thread>
#include <memory>
#include "RequestHandler.h"
#include "Logger.h"

class ConnectionHandler {
private:
    int serverSocket;
    // store all clients' threads
    std::vector<std::thread> clientThreads;
    std::shared_ptr<RequestHandler> requestHandler;
    std::shared_ptr<Logger> logger;

public:
    ConnectionHandler(std::shared_ptr<RequestHandler> handler, std::shared_ptr<Logger> logger);
    ~ConnectionHandler();

    void start(int port);
    void stop();
    void handleClient(int clientSocket);
}; 