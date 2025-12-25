#pragma once
#include <map>
#include <vector>
#include <mutex>
#include <string>

struct OrderBookLevel {
    double price;
    double size;
    int order_count;
};

class PaperOrderBook {
private:
    std::multimap<double, double, std::greater<double>> bids;  // price -> size, sorted high to low
    std::multimap<double, double> asks;  // price -> size, sorted low to high
    mutable std::mutex mutex;
    
    double spread_multiplier{1.0};
    double depth_levels{10};
    double min_spread{0.01};
    
public:
    PaperOrderBook() = default;
    
    void update_from_tick(double bid, double ask, double bid_size, double ask_size) {
        std::lock_guard lock(mutex);
        
        bids.clear();
        asks.clear();
        
        // Create realistic order book levels
        for (int i = 0; i < depth_levels; i++) {
            double bid_price = bid - i * min_spread * spread_multiplier;
            double ask_price = ask + i * min_spread * spread_multiplier;
            
            double bid_level_size = bid_size / (i + 1);
            double ask_level_size = ask_size / (i + 1);
            
            bids.insert({bid_price, bid_level_size});
            asks.insert({ask_price, ask_level_size});
        }
    }
    
    std::pair<double, double> get_best_bid_ask() const {
        std::lock_guard lock(mutex);
        
        if (bids.empty() || asks.empty()) {
            return {0.0, 0.0};
        }
        
        return {bids.begin()->first, asks.begin()->first};
    }
    
    double get_mid_price() const {
        auto [bid, ask] = get_best_bid_ask();
        return (bid + ask) / 2.0;
    }
    
    double get_spread() const {
        auto [bid, ask] = get_best_bid_ask();
        return ask - bid;
    }
    
    std::vector<OrderBookLevel> get_bid_levels(int levels = 5) const {
        std::lock_guard lock(mutex);
        
        std::vector<OrderBookLevel> result;
        int count = 0;
        
        for (auto it = bids.begin(); it != bids.end() && count < levels; ++it, ++count) {
            result.push_back({it->first, it->second, 1});
        }
        
        return result;
    }
    
    std::vector<OrderBookLevel> get_ask_levels(int levels = 5) const {
        std::lock_guard lock(mutex);
        
        std::vector<OrderBookLevel> result;
        int count = 0;
        
        for (auto it = asks.begin(); it != asks.end() && count < levels; ++it, ++count) {
            result.push_back({it->first, it->second, 1});
        }
        
        return result;
    }
    
    std::pair<double, double> get_market_depth() const {
        std::lock_guard lock(mutex);
        
        double bid_depth = 0.0;
        double ask_depth = 0.0;
        
        for (const auto& [price, size] : bids) {
            bid_depth += size;
        }
        
        for (const auto& [price, size] : asks) {
            ask_depth += size;
        }
        
        return {bid_depth, ask_depth};
    }
    
    double get_vwap(int levels = 3) const {
        std::lock_guard lock(mutex);
        
        double total_size = 0.0;
        double total_value = 0.0;
        int count = 0;
        
        // Bid side VWAP
        for (auto it = bids.begin(); it != bids.end() && count < levels; ++it, ++count) {
            total_size += it->second;
            total_value += it->first * it->second;
        }
        
        count = 0;
        // Ask side VWAP
        for (auto it = asks.begin(); it != asks.end() && count < levels; ++it, ++count) {
            total_size += it->second;
            total_value += it->first * it->second;
        }
        
        return total_size > 0 ? total_value / total_size : 0.0;
    }
    
    void set_spread_multiplier(double multiplier) {
        spread_multiplier = multiplier;
    }
    
    void set_depth_levels(int levels) {
        depth_levels = levels;
    }
    
    void set_min_spread(double spread) {
        min_spread = spread;
    }
    
    void clear() {
        std::lock_guard lock(mutex);
        bids.clear();
        asks.clear();
    }
    
    bool is_empty() const {
        std::lock_guard lock(mutex);
        return bids.empty() && asks.empty();
    }
};