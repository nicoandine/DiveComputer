#include "log_info.hpp"
#include "global.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

namespace DiveComputer {

// Initialize the constant
const std::string LOG_FILE_NAME = "divelog.txt";

void logWrite(const std::string& message) {
    // Ensure app info is set for proper path resolution
    ensureAppInfoSet();
    
    // Get the current time
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    
    // Format the timestamp
    char timeBuffer[30];
    std::strftime(timeBuffer, sizeof(timeBuffer), "[%Y-%m-%d %H:%M:%S]", std::localtime(&time));
    
    // Combine timestamp and message
    std::string logEntry = std::string(timeBuffer) + " " + message;
    
    // Print to console
    std::cout << logEntry << std::endl;
    
    // Get the log file path
    std::string logFilePath = getFilePath(LOG_FILE_NAME);
    
    // Open the file in append mode
    std::ofstream logFile(logFilePath, std::ios::app);
    
    if (logFile.is_open()) {
        // Write to the log file
        logFile << logEntry << std::endl;
        logFile.close();
    } else {
        // If we can't open the log file, at least print an error to the console
        std::cerr << timeBuffer << " [ERROR] Could not open log file at: " << logFilePath << std::endl;
    }
}

void logClear() {
    // Clear terminal window
    #ifdef _WIN32
        // For Windows
        system("cls");
    #else
        // For Unix/Linux/macOS
        system("clear");
    #endif

    // Delete log file
    std::string logFilePath = getFilePath(LOG_FILE_NAME);
    
    // First, check if file exists
    std::ifstream fileCheck(logFilePath);
    if (fileCheck.good()) {
        fileCheck.close();
        
        // Delete the file
        if (std::remove(logFilePath.c_str()) == 0) {
            std::cout << "Log file cleared successfully." << std::endl;
        } else {
            std::cerr << "Error: could not delete log file at: " << logFilePath << std::endl;
        }
    }
    
    // Create a new, empty log file
    std::ofstream newFile(logFilePath);
    if (newFile.is_open()) {
        // Add header or initial message
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        
        char timeBuffer[30];
        std::strftime(timeBuffer, sizeof(timeBuffer), "[%Y-%m-%d %H:%M:%S]", std::localtime(&time));
        
        newFile << timeBuffer << " Log started" << std::endl;
        newFile.close();
    }
}

} // namespace DiveComputer