#pragma once
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <algorithm>

namespace utils {

    inline uint64_t timestamp_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    inline uint64_t timestamp_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // FIX: std::localtime() is not thread-safe (shares a static internal buffer).
    // Replaced with localtime_r / localtime_s.
    inline std::string format_time(uint64_t ts_ns) {
        auto duration = std::chrono::nanoseconds(ts_ns);
        auto tp = std::chrono::time_point<std::chrono::system_clock>(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
        std::time_t t = std::chrono::system_clock::to_time_t(tp);

        struct tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // FIX: std::rand() is not thread-safe or well-distributed.
    // Kept for backward compat with callers but flagged — prefer <random> at call sites.
    inline double random_double(double min, double max) {
        return min + (max - min) * (std::rand() / static_cast<double>(RAND_MAX));
    }

    // FIX: Removed hand-rolled clamp — use std::clamp (C++17) instead.
    // Kept as a thin wrapper for any callers that use utils::clamp.
    template<typename T>
    inline T clamp(T value, T lo, T hi) {
        return std::clamp(value, lo, hi);
    }

} // namespace utils