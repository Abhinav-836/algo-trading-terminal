#pragma once
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace utils {
    
    inline uint64_t timestamp_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
    
    inline uint64_t timestamp_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    inline std::string format_time(uint64_t timestamp_ns) {
        auto duration = std::chrono::nanoseconds(timestamp_ns);
        auto time = std::chrono::time_point<std::chrono::system_clock>(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
        auto time_t = std::chrono::system_clock::to_time_t(time);
        
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    inline double random_double(double min, double max) {
        return min + (max - min) * (std::rand() / double(RAND_MAX));
    }
    
    template<typename T>
    inline T clamp(T value, T min, T max) {
        return value < min ? min : (value > max ? max : value);
    }
}