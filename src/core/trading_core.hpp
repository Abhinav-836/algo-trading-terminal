#pragma once
#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include "types.hpp"
#include "../live/risk.hpp"
#include "../live/pnl_tracker.hpp"
#include "../live/positions.hpp"
#include "../data/market_data.hpp"

class Strategy;

class TradingCore {
private:
    std::atomic<bool> running{false};
    std::vector<std::unique_ptr<Strategy>> strategies;
    std::thread processing_thread;
    std::thread health_thread;
    mutable std::mutex strategy_mutex;
    
    // Components
    RiskManager risk_manager;
    PnLTracker pnl_tracker;
    PositionManager position_manager;
    MarketDataAggregator market_data;
    
    std::function<void(const Signal&)> order_router;
    
    // Metrics
    std::atomic<uint64_t> total_ticks_processed{0};
    std::atomic<uint64_t> total_signals_generated{0};
    
public:
    TradingCore() = default;
    
    ~TradingCore() {
        stop();
    }
    
    void set_order_router(std::function<void(const Signal&)> router) {
        order_router = router;
    }
    
    void start() {
        if (running) return;
        
        running = true;
        processing_thread = std::thread(&TradingCore::run, this);
        health_thread = std::thread(&TradingCore::health_check, this);
    }
    
    void stop() {
        running = false;
        if (processing_thread.joinable()) {
            processing_thread.join();
        }
        if (health_thread.joinable()) {
            health_thread.join();
        }
    }
    
    void add_strategy(std::unique_ptr<Strategy> strategy) {
        std::lock_guard<std::mutex> lock(strategy_mutex);
        strategies.push_back(std::move(strategy));
    }
    
    void process_tick(const Tick& tick) {
        total_ticks_processed++;
        
        // Aggregate market data
        market_data.on_tick(tick);
        
        // Process strategies
        std::lock_guard<std::mutex> lock(strategy_mutex);
        
        for (auto& strategy : strategies) {
            auto signal = strategy->generate_signal(tick);
            if (signal.valid && order_router) {
                total_signals_generated++;
                order_router(signal);
            }
        }
    }
    
    // Getters
    RiskManager& get_risk_manager() { return risk_manager; }
    PnLTracker& get_pnl_tracker() { return pnl_tracker; }
    PositionManager& get_position_manager() { return position_manager; }
    MarketDataAggregator& get_market_data() { return market_data; }
    
    uint64_t get_total_ticks() const { return total_ticks_processed.load(); }
    uint64_t get_total_signals() const { return total_signals_generated.load(); }
    
private:
    void run() {
        while (running) {
            // Heartbeat / maintenance tasks
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void health_check() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            std::cout << "[HEALTH] Ticks: " << total_ticks_processed.load()
                      << ", Signals: " << total_signals_generated.load()
                      << ", Strategies: " << strategies.size() << std::endl;
        }
    }
};