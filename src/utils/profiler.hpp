#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <iostream>

struct ProfileData {
    uint64_t total_time_ns{0};
    uint64_t call_count{0};
    uint64_t min_time_ns{UINT64_MAX};
    uint64_t max_time_ns{0};
    std::chrono::high_resolution_clock::time_point last_start;
};

class Profiler {
private:
    std::unordered_map<std::string, ProfileData> profiles;
    mutable std::mutex mutex;
    
public:
    void start(const std::string& name) {
        std::lock_guard lock(mutex);
        profiles[name].last_start = std::chrono::high_resolution_clock::now();
    }
    
    void stop(const std::string& name) {
        auto end = std::chrono::high_resolution_clock::now();
        
        std::lock_guard lock(mutex);
        auto it = profiles.find(name);
        if (it == profiles.end()) return;
        
        auto start = it->second.last_start;
        uint64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        
        it->second.total_time_ns += duration;
        it->second.call_count++;
        it->second.min_time_ns = std::min(it->second.min_time_ns, duration);
        it->second.max_time_ns = std::max(it->second.max_time_ns, duration);
    }
    
    void print_stats() const {
        std::lock_guard lock(mutex);
        
        std::cout << "\n=== PERFORMANCE PROFILER ===" << std::endl;
        std::cout << "Name\t\tCalls\tAvg (ns)\tMin (ns)\tMax (ns)\tTotal (ms)" << std::endl;
        std::cout << "----------------------------------------------------------------" << std::endl;
        
        for (const auto& [name, data] : profiles) {
            if (data.call_count == 0) continue;
            
            double avg_ns = static_cast<double>(data.total_time_ns) / data.call_count;
            double total_ms = data.total_time_ns / 1000000.0;
            
            std::cout << name << "\t\t"
                      << data.call_count << "\t"
                      << static_cast<uint64_t>(avg_ns) << "\t\t"
                      << data.min_time_ns << "\t\t"
                      << data.max_time_ns << "\t\t"
                      << total_ms << std::endl;
        }
    }
    
    void reset() {
        std::lock_guard lock(mutex);
        profiles.clear();
    }
    
    ProfileData get_profile(const std::string& name) const {
        std::lock_guard lock(mutex);
        auto it = profiles.find(name);
        return it != profiles.end() ? it->second : ProfileData{};
    }
    
    std::vector<std::string> get_profile_names() const {
        std::lock_guard lock(mutex);
        
        std::vector<std::string> names;
        for (const auto& pair : profiles) {
            names.push_back(pair.first);
        }
        
        return names;
    }
};

// RAII-style profiler for automatic start/stop
class ScopedProfiler {
private:
    Profiler& profiler;
    std::string name;
    
public:
    ScopedProfiler(Profiler& p, const std::string& n) : profiler(p), name(n) {
        profiler.start(name);
    }
    
    ~ScopedProfiler() {
        profiler.stop(name);
    }
};

// Global profiler instance
extern Profiler global_profiler;

// Macro for easy profiling
#define PROFILE_SCOPE(name) ScopedProfiler scoped_profiler_##__LINE__(global_profiler, name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)