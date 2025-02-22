#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

struct CacheEntry {
    std::string response;
    time_t timestamp;
    time_t expiry;
};

class CacheManager {
private:
    std::unordered_map<std::string, CacheEntry> cache;
    std::mutex cacheMutex;
    size_t maxCacheSize;

public:
    CacheManager(size_t maxSize = 1024);
    bool get(const std::string& key, std::string& response);
    void put(const std::string& key, const std::string& response, time_t expiry);
    void clear();
    size_t size() const;
}; 