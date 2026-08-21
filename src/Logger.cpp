#include "Logger.hpp"

#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

std::string formatTime(std::chrono::system_clock::time_point now) {
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);   // or gmtime for UTC

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

Logger::Logger(std::string logFile){
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
    file << "Log File Created. Logs start:" << "\n";
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
    std::string time = formatTime(now);

    std::ofstream file(logFile, std::ios::app);
    if (!file)
    {
        std::cout << "Failed to open the file" << "\n";
    }
    file << time << "\t" << levelList[level] << "\t" << statusCode << " " << method << " " << path << "\t" << message << "\n";
    file.close();
}