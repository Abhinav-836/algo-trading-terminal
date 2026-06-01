#pragma once
#include <chrono>
#include <ctime>
#include <string>
#include <iostream>

class MarketHours {
public:
    // FIX: Both is_market_open() and seconds_until_open() called std::localtime()
    // which returns a pointer to a shared static struct — not thread-safe.
    // Replaced with localtime_r (POSIX) / localtime_s (MSVC) throughout.
    //
    // NOTE: The original used LOCAL machine time for market hours checks, which
    // is only correct if the machine runs in US/Eastern time.
    // A production system should convert UTC to ET explicitly.

    static bool is_market_open() {
        auto now      = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        struct tm local_tm{};
#ifdef _WIN32
        localtime_s(&local_tm, &t);
#else
        localtime_r(&t, &local_tm);
#endif

        int weekday = local_tm.tm_wday;
        if (weekday == 0 || weekday == 6) return false;  // Sat/Sun

        int current_seconds = local_tm.tm_hour * 3600 + local_tm.tm_min * 60;
        int open_seconds    = 9 * 3600 + 30 * 60;   // 9:30 AM
        int close_seconds   = 16 * 3600;             // 4:00 PM

        return current_seconds >= open_seconds && current_seconds < close_seconds;
    }

    static std::string get_next_open_time() {
        return "Next trading day 9:30 AM ET";
    }

    static int seconds_until_open() {
        if (is_market_open()) return 0;

        auto now      = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        struct tm next_open_tm{};
#ifdef _WIN32
        localtime_s(&next_open_tm, &t);
#else
        localtime_r(&t, &next_open_tm);
#endif

        next_open_tm.tm_hour = 9;
        next_open_tm.tm_min  = 30;
        next_open_tm.tm_sec  = 0;
        next_open_tm.tm_mday++;   // advance to next calendar day

        std::time_t next_open = std::mktime(&next_open_tm);
        return static_cast<int>(std::difftime(next_open, t));
    }
};