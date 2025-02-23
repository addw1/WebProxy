#include "RequestHandler.h"
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <unistd.h>


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
        std::string response = forwardRequest(parsedRequest, request);
        // TODO: test cache later 
        // cacheManager->put(cacheKey, response, time(nullptr) + 300); // Cache for 5 minutes
        return response;
    } catch (const std::exception& e) {
        logger->log(Logger::ERROR, std::string("Error handling request: ") + e.what());
        return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    }
}

std::string RequestHandler::forwardRequest(HttpRequest httpRequest, const std::string& request) {
    try {
        std::string host = httpRequest.headers.at("Host");
        
        // Trim whitespace from both ends
        host.erase(0, host.find_first_not_of(" \t\r\n")); // Leading whitespace
        host.erase(host.find_last_not_of(" \t\r\n") + 1); // Trailing whitespace
        
        std::cout << "Host string length: " << host.length() << ", content: '" << host << "'" << std::endl;

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) throw std::runtime_error("Failed to create socket");

        struct addrinfo hints{}, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(host.c_str(), "80", &hints, &result);
        if (status != 0) {
            close(sock);
            std::cout << "Host: " << host << " - Error: " << gai_strerror(status) << std::endl;
            throw std::runtime_error("Host not found");
        }

        if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
            freeaddrinfo(result);
            close(sock);
            throw std::runtime_error("Connection failed");
        }

        freeaddrinfo(result);

        ssize_t sent = send(sock, request.c_str(), request.length(), 0);
        if (sent < 0) {
            close(sock);
            throw std::runtime_error("Failed to send request");
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
    catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}