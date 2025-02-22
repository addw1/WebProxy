#pragma once
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

class HttpParser {
public:
    HttpParser();
    HttpRequest parseRequest(const std::string& rawRequest);
    std::string buildRequest(const HttpRequest& request);
    bool isValidRequest(const HttpRequest& request);
}; 