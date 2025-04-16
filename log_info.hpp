#ifndef LOG_INFO_HPP
#define LOG_INFO_HPP

#include <string>
#include <vector>
#include <cstdio>
#include <sstream>
#include <QString>

namespace DiveComputer {
    extern const std::string LOG_FILE_NAME;

    // Forward declaration of function from global.hpp
    std::string getFilePath(const std::string& filename);
    void ensureAppInfoSet();

    // Log basic message
    void logWrite(const std::string& message);
    void logClear();

    // Alternative that handles any type that can be converted to string using stringstream
    template<typename T>
    void logWrite(const T& value) {
        std::stringstream ss;
        ss << value;
        logWrite(ss.str());
    }

    // Template for string concatenation with one or more values
    template<typename T, typename... Args>
    void logWrite(const std::string& prefix, const T& value, Args... args) {
        std::stringstream ss;
        ss << prefix << value;
        logWrite(ss.str(), args...);
    }

    // Specialization for string parameters
    template<typename... Args>
    void logWrite(const std::string& prefix, const std::string& value, Args... args) {
        std::string message = prefix + value;
        logWrite(message, args...);
    }

    // Specialization for const char* parameters
    template<typename... Args>
    void logWrite(const std::string& prefix, const char* value, Args... args) {
        std::string message = prefix + value;
        logWrite(message, args...);
    }

    // Specialization for QString parameters
    template<typename... Args>
    void logWrite(const std::string& prefix, const QString& value, Args... args) {
        std::string message = prefix + value.toStdString();
        logWrite(message, args...);
    }

    // Format strings with printf-style formatting
    template<typename... Args>
    void logWriteF(const std::string& format, Args... args) {
        int size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
        if (size <= 0) {
            logWrite("Error formatting log message");
            return;
        }
        
        std::vector<char> buf(size);
        snprintf(buf.data(), size, format.c_str(), args...);
        
        logWrite(std::string(buf.data(), buf.data() + size - 1));
    }
}

#endif // LOG_INFO_HPP