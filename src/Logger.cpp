#include "Logger.hpp"

#include <fstream>
#include <filesystem>
#include <chrono>
#include <format>

namespace fs = std::filesystem;

Logger::Logger(std::string logFile = "/var/log/ServerLogs.log"){
    this->logFile = logFile;

    fs::path path(logFile);

    if (!fs::exists(path.parent_path()))
    {
        fs::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::app);
    if (!file)
    {
        std::cout << "Failed to open the file" << "\n";
    }
    file << "Log File Created. Logs start:";
    file.close();

    levelList[1] = "INFO";
    levelList[2] = "WARN";
    levelList[3] = "ERROR";
}

void Logger::log(
    int level, 
    const std::string& method, 
    const std::string& path, 
    int statusCode, 
    const std::string& message
){
    auto now = std::chrono::system_clock::now();
    std::string time = std::format("{:%Y-%m-%d %H:%M:%S}", now);

    std::ofstream file(logFile, std::ios::app);
    if (!file)
    {
        std::cout << "Failed to open the file" << "\n";
    }
    file << time << "\t" << levelList[level] << "\t" << statusCode << " " << method << " " << path << "\t" << message << "\n";
    file.close();
}