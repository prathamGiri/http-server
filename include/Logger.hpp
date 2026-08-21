#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>

class Logger{
    public:
        std::string logFile;
        std::unordered_map<int, std::string> levelList;
        Logger(std::string logFile): logFile(logFile){
        };
        void log(
            int level, 
            const std::string& method, 
            const std::string& path, 
            int statusCode, 
            const std::string& message
        );
};