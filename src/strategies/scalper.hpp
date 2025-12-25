#pragma once
#include "strategy.hpp"
#include <unordered_map>
#include <deque>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>

class ScalperStrategy : public Strategy {
private:
    struct SymbolData {
        std::deque<double> prices;
        std::deque<double> volumes;
        size_t window_size;
        double last_signal_price;
        uint64_t last_signal_time;
        bool in_position;
        
        SymbolData() : window_size(50), last_signal_price(0.0), 
                      last_signal_time(0), in_position(false) {}
    };
    
private:
    std::unordered_map<std::string, SymbolData> symbol_data;
    double position_size;
    double max_hold_time_ms;
    double take_profit_pct;
    double stop_loss_pct;
    double min_volatility;
    
public:
    ScalperStrategy() : position_size(10.0), max_hold_time_ms(5000),
                       take_profit_pct(0.001), stop_loss_pct(0.0005),
                       min_volatility(0.0001) {}
    
    ScalperStrategy(double pos_size, double take_profit, double stop_loss) 
        : position_size(pos_size), max_hold_time_ms(5000),
          take_profit_pct(take_profit), stop_loss_pct(stop_loss),
          min_volatility(0.0001) {}
    
    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid = false;
        
        SymbolData& data = symbol_data[tick.symbol];
        
        // Add price to history
        data.prices.push_back(tick.price);
        data.volumes.push_back(tick.volume);
        
        // Keep window size
        if (data.prices.size() > data.window_size) {
            data.prices.pop_front();
            data.volumes.pop_front();
        }
        
        // Need enough data
        if (data.prices.size() < 20) {
            return signal;
        }
        
        // Calculate metrics
        double volatility = calculate_volatility(data.prices);
        double volume_ratio = calculate_volume_ratio(data.volumes);
        
        // Check if we should exit existing position
        if (data.in_position) {
            uint64_t current_time = get_timestamp_ms();
            double hold_time = static_cast<double>(current_time - data.last_signal_time);
            
            double price_move = (tick.price - data.last_signal_price) / data.last_signal_price;
            
            if (price_move >= take_profit_pct || price_move <= -stop_loss_pct || hold_time > max_hold_time_ms) {
                // Create exit order
                Order exit_order;
                exit_order.symbol = tick.symbol;
                exit_order.price = tick.bid;  // Simplified
                exit_order.quantity = position_size;
                exit_order.is_buy = false;  // Exit by selling
                exit_order.type = "market";
                exit_order.tif = "ioc";
                
                signal.orders.push_back(exit_order);
                signal.valid = true;
                data.in_position = false;
            }
            
            return signal;
        }
        
        // Generate entry signal
        if (volatility > min_volatility && volume_ratio > 1.5) {
            double momentum = calculate_momentum(data.prices);
            
            if (std::abs(momentum) > 0.0002) {
                bool should_buy = momentum > 0;
                
                Order order;
                order.symbol = tick.symbol;
                order.price = should_buy ? tick.ask : tick.bid;
                order.quantity = position_size;
                order.is_buy = should_buy;
                order.type = "market";
                order.tif = "ioc";
                
                signal.orders.push_back(order);
                signal.valid = true;
                data.in_position = true;
                data.last_signal_price = tick.price;
                data.last_signal_time = get_timestamp_ms();
            }
        }
        
        return signal;
    }
    
    void on_order_filled(const Order& order) override {
        double commission = order.price * order.quantity * 0.001;
        pnl -= commission;
        update_position(order.symbol, order.is_buy ? order.quantity : -order.quantity, order.price);
    }
    
    void on_order_rejected(const Order& order) override {
        std::cout << "[SCALPER] Order rejected: " << order.symbol 
                  << " " << (order.is_buy ? "BUY" : "SELL") << std::endl;
    }
    
    // Remove override if base class doesn't have virtual get_pnl()
    double get_pnl() const {
        return pnl;
    }
    
private:
    double calculate_volatility(const std::deque<double>& prices) {
        if (prices.size() < 2) return 0.0;
        
        double mean = 0.0;
        for (double price : prices) mean += price;
        mean /= prices.size();
        
        double variance = 0.0;
        for (double price : prices) {
            double diff = price - mean;
            variance += diff * diff;
        }
        variance /= prices.size();
        
        return std::sqrt(variance) / mean;
    }
    
    double calculate_volume_ratio(const std::deque<double>& volumes) {
        if (volumes.size() < 10) return 1.0;
        
        int mid = volumes.size() / 2;
        double older_sum = 0.0, recent_sum = 0.0;
        
        for (int i = 0; i < mid; i++) older_sum += volumes[i];
        for (size_t i = mid; i < volumes.size(); i++) recent_sum += volumes[i];
        
        double older_avg = older_sum / mid;
        if (older_avg == 0.0) return 1.0;
        
        double recent_avg = recent_sum / (volumes.size() - mid);
        return recent_avg / older_avg;
    }
    
    double calculate_momentum(const std::deque<double>& prices) {
        if (prices.size() < 10) return 0.0;
        
        int lookback = 10;
        if (prices.size() < 10) lookback = prices.size();
        
        int half = lookback / 2;
        double older_sum = 0.0, recent_sum = 0.0;
        
        for (int i = 0; i < half; i++) older_sum += prices[prices.size() - i - 1];
        for (int i = half; i < lookback; i++) recent_sum += prices[prices.size() - i - 1];
        
        double older_avg = older_sum / half;
        if (older_avg == 0.0) return 0.0;
        
        double recent_avg = recent_sum / (lookback - half);
        return (recent_avg - older_avg) / older_avg;
    }
    
    uint64_t get_timestamp_ms() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};