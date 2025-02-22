#include "CacheManager.h"

CacheManager::CacheManager(size_t maxSize) : maxCacheSize(maxSize) {}

bool CacheManager::get(const std::string& key, std::string& response) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    auto it = cache.find(key);
    if (it == cache.end()) {
        return false;
    }
    
    if (time(nullptr) > it->second.expiry) {
        cache.erase(it);
        return false;
    }

    response = it->second.response;
    return true;
}

void CacheManager::put(const std::string& key, const std::string& response, time_t expiry) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    if (cache.size() >= maxCacheSize) {
        // Simple cache eviction strategy: remove oldest entry
        time_t oldest = time(nullptr);
        auto oldestIt = cache.begin();
        
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.timestamp < oldest) {
                oldest = it->second.timestamp;
                oldestIt = it;
            }
        }
        
        cache.erase(oldestIt);
    }

    CacheEntry entry{response, time(nullptr), expiry};
    cache[key] = entry;
}

void CacheManager::clear() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache.clear();
}

size_t CacheManager::size() const {
    return cache.size();
} 