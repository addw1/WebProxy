#include "RequestHandler.h"
#include <sstream>

RequestHandler::RequestHandler(std::shared_ptr<CacheManager> cache, std::shared_ptr<Logger> logger)
    : cacheManager(cache), logger(logger), httpParser(std::make_unique<HttpParser>()) {}

std::string RequestHandler::handleRequest(const std::string& request) {
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
        std::string response = forwardRequest(request);
        // TODO: test cache later 
        // cacheManager->put(cacheKey, response, time(nullptr) + 300); // Cache for 5 minutes
        return response;
    } catch (const std::exception& e) {
        logger->log(Logger::ERROR, std::string("Error handling request: ") + e.what());
        return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    }
}

std::string RequestHandler::forwardRequest(const std::string& request) {
    // mock the server
    //TODO: implement the real server
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nResponse from target server";
} 