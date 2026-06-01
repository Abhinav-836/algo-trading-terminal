#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <mutex>
#include <cstdio>

// FIX: Logger::min_level was defined as a static member IN the header with no
// inline/extern guard. Every translation unit that includes this header gets
// its own definition, causing ODR (One Definition Rule) violations and linker
// errors in multi-TU builds. Fixed with `inline` (C++17) so there is exactly
// one definition across all TUs.
//
// FIX: The original log() used snprintf with a 1024-byte stack buffer and
// a non-literal format string, which is a format-string safety risk and
// truncates silently. Replaced with a variadic template approach using
// a stringstream for safety and unlimited length.

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

    static Level get_level() { return min_level; }

    template<typename... Args>
    static void debug(const std::string& msg, Args&&... args) {
        log(Level::DEBUG, msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(const std::string& msg, Args&&... args) {
        log(Level::INFO, msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warning(const std::string& msg, Args&&... args) {
        log(Level::WARNING, msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(const std::string& msg, Args&&... args) {
        log(Level::ERROR, msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void critical(const std::string& msg, Args&&... args) {
        log(Level::CRITICAL, msg, std::forward<Args>(args)...);
    }

private:
    // FIX: `inline` ensures a single definition across all translation units.
    inline static Level min_level{Level::INFO};
    inline static std::mutex log_mutex;

    static std::string level_to_string(Level level) {
        switch (level) {
            case Level::DEBUG:    return "DEBUG";
            case Level::INFO:     return "INFO ";
            case Level::WARNING:  return "WARN ";
            case Level::ERROR:    return "ERROR";
            case Level::CRITICAL: return "CRIT ";
            default:              return "?????";
        }
    }

    // Recursive variadic helper to build the message string safely.
    static void append_args(std::ostringstream&) {}

    template<typename T, typename... Rest>
    static void append_args(std::ostringstream& ss, T&& first, Rest&&... rest) {
        ss << first;
        append_args(ss, std::forward<Rest>(rest)...);
    }

    template<typename... Args>
    static void log(Level level, const std::string& msg, Args&&... args) {
        if (level < min_level) return;

        std::ostringstream ss;
        ss << "[" << level_to_string(level) << "] " << msg;
        append_args(ss, std::forward<Args>(args)...);

        // FIX: Protect concurrent stdout writes with a mutex.
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << ss.str() << "\n";
    }
};