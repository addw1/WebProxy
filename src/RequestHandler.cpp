#include "RequestHandler.h"
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include "MessageForwarder.h"


RequestHandler::RequestHandler(std::shared_ptr<CacheManager> cache, std::shared_ptr<Logger> logger)
    : cacheManager(cache), logger(logger), httpParser(std::make_unique<HttpParser>()) {}

std::string RequestHandler::handleRequest(const std::string& request, int clientSocket) {
    try {
        // Parse the http request
        HttpRequest parsedRequest = httpParser->parseRequest(request);
        
        if (!httpParser->isValidRequest(parsedRequest)) {
            logger->log(Logger::ERROR, "Invalid request received");
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }
        // Build the cache keys
        std::string cacheKey = parsedRequest.method + " " + parsedRequest.url;
        std::string cachedResponse;
        //TODO: test cache later Feb-22
        /*
        if (cacheManager->get(cacheKey, cachedResponse)) {
            logger->log(Logger::INFO, "Cache hit for: " + parsedRequest.url);
            return cachedResponse;
        }
        */
        std::string response = forwardRequest(parsedRequest, clientSocket);
        logger->log(Logger::INFO, response);
        // TODO: test cache later 
        // cacheManager->put(cacheKey, response, time(nullptr) + 300); // Cache for 5 minutes
        std::cout << response << std::endl;
        return response;
    } catch (const std::exception& e) {
        logger->log(Logger::ERROR, std::string("Error handling request: ") + e.what());
        return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    }
}

std::string RequestHandler::forwardRequest(HttpRequest httpRequest, int clientSocket) {
    try {
            MessageForwarder forwarder;
            if (httpRequest.method == "GET") {
                return forwarder.forwardGet(httpRequest);
            } else if (httpRequest.method == "POST") {
                return forwarder.forwardPost(httpRequest);
            } else if (httpRequest.method == "CONNECT") {
                return forwarder.forwardConnect(httpRequest, clientSocket);
            } else {
                return "Error: Unsupported method";
            }
        } catch (const std::exception& e) {
            return std::string("Error: ") + e.what();
    }
}
