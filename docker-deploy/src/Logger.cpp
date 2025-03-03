#include "Logger.h"
#include <ctime>
#include <iomanip>

Logger::Logger(const std::string& logPath) : logPath(logPath) {
    logFile.open(logPath, std::ios::app);
    if (!logFile.is_open()) {
        throw std::runtime_error("Failed to open log file: " + logPath);
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    const char* levelStr;
    switch (level) {
        case DEBUG: levelStr = "DEBUG"; break;
        case INFO: levelStr = "INFO"; break;
        case WARNING: levelStr = "WARNING"; break;
        case ERROR: levelStr = "ERROR"; break;
        default: levelStr = "UNKNOWN";
    }
    
    logFile << "No clientID"
            << ": [" << levelStr << "] " 
            << message << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") <<std::endl;
}
void Logger::log(LogLevel level, const std::string& message, int clientId) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    const char* levelStr;
    switch (level) {
        case DEBUG: levelStr = "DEBUG"; break;
        case INFO: levelStr = "INFO"; break;
        case WARNING: levelStr = "WARNING"; break;
        case ERROR: levelStr = "ERROR"; break;
        default: levelStr = "UNKNOWN";
    }
    
    logFile << clientId
            << ": [" << levelStr << "] " 
            << message << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") <<std::endl;
}

void Logger::log(const std::string& message, int clientId) {
    std::lock_guard<std::mutex> lock(logMutex);
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    logFile << clientId << ": "
            << message << " " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
}

