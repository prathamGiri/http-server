#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <mutex>

enum class LogLevels {INFO, WARN, ERROR};

class Logger{
public:
    std::string logFile;
    std::unordered_map<LogLevels, std::string> levelList;
    mutable std::mutex logMutex;

    explicit Logger(std::string logFile);
    
    void log(
        LogLevels level, 
        const std::string& method, 
        const std::string& path, 
        int statusCode, 
        const std::string& message
    );
};