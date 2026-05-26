#pragma once
#include "../core/types.hpp"
#include "cache.hpp"
#include <unordered_map>
#include <string>
#include <mutex>
#include <functional>
#include <vector>

class MarketDataAggregator {
private:
    MarketDataCache cache;
    std::unordered_map<std::string, std::vector<Tick>> symbol_ticks;
    std::unordered_map<std::string, double> last_prices;
    mutable std::mutex mutex;
    
    std::vector<std::function<void(const Tick&)>> subscribers;
    
public:
    void on_tick(const Tick& tick) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Cache the tick
        cache.cache_tick(tick);
        
        // Store in history (keep last 1000 ticks per symbol)
        auto& ticks = symbol_ticks[tick.symbol];
        ticks.push_back(tick);
        if (ticks.size() > 1000) {
            ticks.erase(ticks.begin());
        }
        
        // Update last price
        last_prices[tick.symbol] = tick.price;
        
        // Notify subscribers
        for (const auto& callback : subscribers) {
            callback(tick);
        }
    }
    
    void subscribe(std::function<void(const Tick&)> callback) {
        std::lock_guard<std::mutex> lock(mutex);
        subscribers.push_back(callback);
    }
    
    bool get_last_tick(const std::string& symbol, Tick& tick) {
        return cache.get_last_tick(symbol, tick);
    }
    
    double get_last_price(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = last_prices.find(symbol);
        return it != last_prices.end() ? it->second : 0.0;
    }
    
    std::vector<Tick> get_historical_ticks(const std::string& symbol, int count = 100) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = symbol_ticks.find(symbol);
        if (it == symbol_ticks.end()) return {};
        
        const auto& ticks = it->second;
        int start = std::max(0, static_cast<int>(ticks.size()) - count);
        return std::vector<Tick>(ticks.begin() + start, ticks.end());
    }
    
    double get_vwap(const std::string& symbol, int lookback = 20) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = symbol_ticks.find(symbol);
        if (it == symbol_ticks.end()) return 0.0;
        
        const auto& ticks = it->second;
        int start = std::max(0, static_cast<int>(ticks.size()) - lookback);
        
        double total_value = 0.0;
        double total_volume = 0.0;
        
        for (size_t i = start; i < ticks.size(); i++) {
            total_value += ticks[i].price * ticks[i].volume;
            total_volume += ticks[i].volume;
        }
        
        return total_volume > 0 ? total_value / total_volume : 0.0;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        cache.clear();
        symbol_ticks.clear();
        last_prices.clear();
    }
};