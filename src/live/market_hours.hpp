#pragma once
#include <chrono>
#include <ctime>
#include <string>
#include <iostream>

class MarketHours {
public:
    static bool is_market_open() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm* local_tm = std::localtime(&now_time);
        
        // Check if weekend
        int weekday = local_tm->tm_wday;
        if (weekday == 0 || weekday == 6) return false;
        
        // Check time (9:30 AM to 4:00 PM ET)
        int hour = local_tm->tm_hour;
        int minute = local_tm->tm_min;
        
        // Convert to seconds since midnight
        int current_seconds = hour * 3600 + minute * 60;
        int open_seconds = 9 * 3600 + 30 * 60;   // 9:30 AM
        int close_seconds = 16 * 3600;            // 4:00 PM
        
        return current_seconds >= open_seconds && current_seconds < close_seconds;
    }
    
    static std::string get_next_open_time() {
        return "Next trading day 9:30 AM ET";
    }
    
    static int seconds_until_open() {
        if (is_market_open()) return 0;
        
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm* local_tm = std::localtime(&now_time);
        
        // Set to next day 9:30 AM
        local_tm->tm_hour = 9;
        local_tm->tm_min = 30;
        local_tm->tm_sec = 0;
        local_tm->tm_mday++;
        
        std::time_t next_open = std::mktime(local_tm);
        return std::difftime(next_open, now_time);
    }
};