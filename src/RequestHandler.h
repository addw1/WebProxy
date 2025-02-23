#pragma once
#include <string>
#include <memory>
#include "HttpParser.h"
#include "CacheManager.h"
#include "Logger.h"

class RequestHandler {
private:
    std::shared_ptr<CacheManager> cacheManager;
    std::shared_ptr<Logger> logger;
    std::unique_ptr<HttpParser> httpParser;

public:
    RequestHandler(std::shared_ptr<CacheManager> cache, std::shared_ptr<Logger> logger);
    std::string handleRequest(const std::string& request);
    std::string forwardRequest(HttpRequest httpRequest, const std::string& request);
}; 