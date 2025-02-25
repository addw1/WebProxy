#include "HttpParser.h"
#include <sstream>

HttpParser::HttpParser() {}

HttpRequest HttpParser::parseRequest(const std::string& rawRequest) {
    HttpRequest request;
    // Bind the stream with request
    std::istringstream stream(rawRequest);
    std::string line;

    // Parse request line
    std::getline(stream, line);
    std::istringstream requestLine(line);
    requestLine >> request.method >> request.url >> request.version;

    // Parse headers
    while (std::getline(stream, line) && line != "\r" && line != "") {
        // Format key : value
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 2);
            request.headers[key] = value;
        }
    }

    // Parse body if present
    std::stringstream bodyStream;
    while (std::getline(stream, line)) {
        bodyStream << line << "\n";
    }
    request.body = bodyStream.str();

    return request;
}

std::string HttpParser::buildRequest(const HttpRequest& request) {
    std::stringstream ss;
    
    ss << request.method << " " << request.url << " " << request.version << "\r\n";
    
    for (const auto& header : request.headers) {
        ss << header.first << ": " << header.second << "\r\n";
    }
    
    ss << "\r\n" << request.body;
    return ss.str();
}

bool HttpParser::isValidRequest(const HttpRequest& request) {
    // Basic validation
    if (request.method.empty() || request.url.empty() || request.version.empty()) {
        return false;
    }
    
    // Validate method
    if (request.method != "GET" && request.method != "POST" && 
        request.method != "CONNECT") {
        return false;
    }
    
    // Validate HTTP version
    if (request.version != "HTTP/1.0" && request.version != "HTTP/1.1") {
        return false;
    }
    
    return true;
} 