#pragma once
#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include "types.hpp"
#include "strategy.hpp" // Include the header file for the Strategy class

class TradingCore {
private:
    std::atomic<bool> running{false};
    std::vector<std::unique_ptr<class Strategy>> strategies;
    std::thread processing_thread;
    
public:
    TradingCore() = default;
    
    void start() {
        running = true;
        processing_thread = std::thread(&TradingCore::run, this);
    }
    
    void stop() {
        running = false;
        if (processing_thread.joinable()) {
            processing_thread.join();
        }
    }
    
    void add_strategy(std::unique_ptr<class Strategy> strategy) {
        strategies.push_back(std::move(strategy));
    }
    
    void process_tick(const Tick & tick) {
        for (auto& strategy : strategies) {
            auto signal = strategy->generate_signal(tick);
            if (signal.valid) {
                // Process signal
                process_signal(signal);
            }
        }
    }
    
private:
    void run() {
        while (running) {
            // Main processing loop
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    void process_signal(const Signal& signal) {
        // Process trading signal
        // Implementation depends on exchange adapter
    }
};