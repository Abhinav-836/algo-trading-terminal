#pragma once
#include "../core/types.hpp"
#include "../data/historical.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
#include <functional> // Added for std::function

class ReplayEngine {
private:
    std::vector<Tick> historical_ticks;
    size_t current_index{0};
    std::atomic<bool> replaying{false};
    std::thread replay_thread;
    
    double speed_multiplier{1.0};  // 1.0 = realtime
    bool loop_mode{false};
    std::string current_symbol;
    
public:
    ReplayEngine() = default;
    
    bool load_historical_data(const std::string& symbol, const std::string& filename = "ticks.csv") {
        HistoricalData historical;
        historical_ticks = historical.load_ticks(symbol, filename);
        
        if (historical_ticks.empty()) {
            std::cout << "No historical data found for " << symbol << std::endl;
            
            // Generate sample data
            generate_sample_data(symbol);
        }
        
        current_symbol = symbol;
        current_index = 0;
        
        std::cout << "Loaded " << historical_ticks.size() << " ticks for " << symbol << std::endl;
        return !historical_ticks.empty();
    }
    
    void start_replay(std::function<void(const Tick&)> callback) {
        if (historical_ticks.empty()) {
            std::cerr << "No data loaded for replay" << std::endl;
            return;
        }
        
        replaying = true;
        replay_thread = std::thread(&ReplayEngine::replay_loop, this, callback);
        std::cout << "Replay started for " << current_symbol << " at " << speed_multiplier << "x speed" << std::endl;
    }
    
    void stop_replay() {
        replaying = false;
        if (replay_thread.joinable()) {
            replay_thread.join();
        }
    }
    
    void set_speed(double multiplier) {
        speed_multiplier = multiplier;
    }
    
    void set_loop_mode(bool loop) {
        loop_mode = loop;
    }
    
    size_t get_total_ticks() const {
        return historical_ticks.size();
    }
    
    size_t get_current_index() const {
        return current_index;
    }
    
    double get_progress() const {
        if (historical_ticks.empty()) return 0.0;
        return static_cast<double>(current_index) / historical_ticks.size();
    }
    
    void reset() {
        current_index = 0;
    }
    
    bool is_replaying() const {
        return replaying.load();
    }
    
private:
    void replay_loop(std::function<void(const Tick&)> callback) {
        uint64_t last_timestamp = 0;
        
        while (replaying && current_index < historical_ticks.size()) {
            const Tick& tick = historical_ticks[current_index];
            
            // Calculate delay based on timestamps
            if (current_index > 0 && last_timestamp > 0) {
                uint64_t time_diff = tick.timestamp - last_timestamp;
                uint64_t delay_ms = time_diff / 1000000;  // Convert ns to ms
                
                if (delay_ms > 0) {
                    // Apply speed multiplier
                    delay_ms = static_cast<uint64_t>(delay_ms / speed_multiplier);
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                }
            }
            
            // Send tick to callback
            callback(tick);
            
            last_timestamp = tick.timestamp;
            current_index++;
            
            // Loop if enabled
            if (loop_mode && current_index >= historical_ticks.size()) {
                current_index = 0;
                last_timestamp = 0;
                std::cout << "Replay loop restarted" << std::endl;
            }
        }
        
        replaying = false;
        std::cout << "Replay finished" << std::endl;
    }
    
    void generate_sample_data(const std::string& symbol) {
        std::cout << "Generating sample data for " << symbol << std::endl;
        
        double base_price = 150.0;
        uint64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count() - 86400000000000; // 24 hours ago
        
        for (int i = 0; i < 10000; i++) {
            // Random walk price
            base_price += (std::rand() % 100 - 50) * 0.01;
            base_price = std::max(base_price, 100.0);
            base_price = std::min(base_price, 200.0);
            
            double spread = 0.01;
            double bid = base_price - spread / 2;
            double ask = base_price + spread / 2;
            
            historical_ticks.push_back(Tick(
                timestamp + i * 1000000000,  // 1 second intervals
                symbol,
                base_price,
                100.0 + std::rand() % 900,
                bid,
                ask,
                100.0,
                100.0
            ));
        }
        
        // Save to file for future use
        HistoricalData historical;
        for (const auto& tick : historical_ticks) {
            historical.save_tick(tick);
        }
    }
};