#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <string>

class Logger {
private:
    std::ofstream logFile;
    std::mutex logMutex;
    std::string logPath;

public:
    enum LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    Logger(const std::string& logPath);
    ~Logger();
    
    void log(LogLevel level, const std::string& message);
    void log(const std::string& message, int clientId);
}; 