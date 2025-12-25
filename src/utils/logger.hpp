#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };
    
    static void set_level(Level level) {
        min_level = level;
    }
    
    template<typename... Args>
    static void debug(const std::string& format, Args... args) {
        log(Level::DEBUG, format, args...);
    }
    
    template<typename... Args>
    static void info(const std::string& format, Args... args) {
        log(Level::INFO, format, args...);
    }
    
    template<typename... Args>
    static void warning(const std::string& format, Args... args) {
        log(Level::WARNING, format, args...);
    }
    
    template<typename... Args>
    static void error(const std::string& format, Args... args) {
        log(Level::ERROR, format, args...);
    }
    
private:
    static Level min_level;
    
    template<typename... Args>
    static void log(Level level, const std::string& format, Args... args) {
        if (level < min_level) return;
        
        std::ostringstream ss;
        ss << "[" << level_to_string(level) << "] ";
        
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format.c_str(), args...);
        ss << buffer;
        
        std::cout << ss.str() << std::endl;
    }
    
    static std::string level_to_string(Level level) {
        switch(level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO: return "INFO";
            case Level::WARNING: return "WARN";
            case Level::ERROR: return "ERROR";
            case Level::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
};

Logger::Level Logger::min_level = Logger::Level::INFO;