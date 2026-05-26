#pragma once
#include "strategy.hpp"
#include <deque>
#include <cmath>
#include <numeric>
#include <algorithm>

class MomentumStrategy : public Strategy {
private:
    struct SymbolData {
        std::deque<double> prices;
        std::deque<double> volumes;
        std::deque<double> returns;
        
        double sma_20{0.0};
        double sma_50{0.0};
        double rsi{50.0};
        double momentum{0.0};
        double volatility{0.0};
        
        bool in_position{false};
        double entry_price{0.0};
        uint64_t entry_time{0};
        double trailing_stop{0.0};
        double highest_price{0.0};
        
        static constexpr size_t WINDOW_20 = 20;
        static constexpr size_t WINDOW_50 = 50;
    };
    
    std::unordered_map<std::string, SymbolData> m_data;
    
    // Strategy parameters
    double m_position_size{100.0};
    double m_rsi_upper{70.0};
    double m_rsi_lower{30.0};
    double m_trailing_stop_pct{0.02};  // 2% trailing stop
    double m_take_profit_pct{0.05};    // 5% take profit
    double m_max_hold_seconds{300};     // 5 minutes max hold
    
    // Risk parameters
    double m_max_daily_loss{500.0};
    double m_max_position_value{5000.0};
    
public:
    MomentumStrategy() = default;
    
    MomentumStrategy(double position_size, double rsi_upper, double rsi_lower)
        : m_position_size(position_size), m_rsi_upper(rsi_upper), m_rsi_lower(rsi_lower) {}
    
    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid = false;
        
        auto& data = m_data[tick.symbol];
        
        // Update price history
        data.prices.push_back(tick.price);
        data.volumes.push_back(tick.volume);
        
        // Maintain window sizes
        while (data.prices.size() > SymbolData::WINDOW_50) {
            data.prices.pop_front();
            data.volumes.pop_front();
        }
        
        // Calculate indicators if we have enough data
        if (data.prices.size() >= SymbolData::WINDOW_50) {
            calculate_sma(data);
            calculate_rsi(data);
            calculate_momentum(data);
            calculate_volatility(data);
            
            // Check for exit if in position
            if (data.in_position) {
                double current_price = tick.price;
                uint64_t now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
                double hold_time = now - data.entry_time;
                
                // Update highest price for trailing stop
                if (current_price > data.highest_price) {
                    data.highest_price = current_price;
                    data.trailing_stop = data.highest_price * (1 - m_trailing_stop_pct);
                }
                
                double price_change = (current_price - data.entry_price) / data.entry_price;
                
                bool should_exit = false;
                std::string exit_reason;
                
                // Take profit
                if (price_change >= m_take_profit_pct) {
                    should_exit = true;
                    exit_reason = "take_profit";
                }
                // Trailing stop
                else if (current_price <= data.trailing_stop) {
                    should_exit = true;
                    exit_reason = "trailing_stop";
                }
                // RSI reversal
                else if ((price_change > 0 && data.rsi > 80) || (price_change < 0 && data.rsi < 20)) {
                    should_exit = true;
                    exit_reason = "rsi_reversal";
                }
                // Max hold time
                else if (hold_time > m_max_hold_seconds) {
                    should_exit = true;
                    exit_reason = "max_hold";
                }
                
                if (should_exit) {
                    Order exit_order;
                    exit_order.symbol = tick.symbol;
                    exit_order.price = tick.bid;
                    exit_order.quantity = m_position_size;
                    exit_order.is_buy = false;
                    exit_order.type = "market";
                    exit_order.tif = "day";
                    
                    signal.orders.push_back(exit_order);
                    signal.valid = true;
                    signal.reason = exit_reason;
                    signal.confidence = 0.9;
                    
                    data.in_position = false;
                    
                    double pnl_change = price_change * m_position_size * tick.price;
                    pnl += pnl_change;
                    
                    std::cout << "📉 EXIT " << tick.symbol << " | P&L: " << (pnl_change >= 0 ? "+" : "") 
                              << "$" << pnl_change << " | Reason: " << exit_reason << std::endl;
                }
            }
            // Check for entry if not in position
            else {
                bool should_buy = false;
                std::string entry_reason;
                double confidence = 0.0;
                
                // Momentum crossover (price > SMA20 > SMA50)
                if (tick.price > data.sma_20 && data.sma_20 > data.sma_50 && data.momentum > 0.01) {
                    should_buy = true;
                    entry_reason = "golden_cross";
                    confidence = 0.7;
                }
                // RSI oversold bounce
                else if (data.rsi < m_rsi_lower && data.momentum > 0) {
                    should_buy = true;
                    entry_reason = "rsi_oversold";
                    confidence = 0.6;
                }
                // Strong momentum with high volume
                else if (data.momentum > 0.02 && data.volatility > 0.005) {
                    should_buy = true;
                    entry_reason = "strong_momentum";
                    confidence = 0.5;
                }
                
                if (should_buy) {
                    Order order;
                    order.symbol = tick.symbol;
                    order.price = tick.ask;
                    order.quantity = m_position_size;
                    order.is_buy = true;
                    order.type = "market";
                    order.tif = "day";
                    
                    signal.orders.push_back(order);
                    signal.valid = true;
                    signal.reason = entry_reason;
                    signal.confidence = confidence;
                    
                    data.in_position = true;
                    data.entry_price = tick.price;
                    data.entry_time = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
                    data.highest_price = tick.price;
                    data.trailing_stop = tick.price * (1 - m_trailing_stop_pct);
                    
                    std::cout << "📈 ENTRY " << tick.symbol << " | Reason: " << entry_reason 
                              << " | Confidence: " << (confidence * 100) << "%" << std::endl;
                }
            }
        }
        
        return signal;
    }
    
    void on_order_filled(const Order& order) override {
        double commission = order.price * order.quantity * 0.0005;
        pnl -= commission;
        update_position(order.symbol, order.is_buy ? order.quantity : -order.quantity, order.price);
    }
    
    void on_order_rejected(const Order& order) override {
        std::cout << "❌ ORDER REJECTED: " << order.symbol << " " 
                  << (order.is_buy ? "BUY" : "SELL") << std::endl;
        
        auto& data = m_data[order.symbol];
        data.in_position = false;
    }
    
private:
    void calculate_sma(SymbolData& data) {
        if (data.prices.size() >= 20) {
            double sum_20 = 0;
            for (size_t i = data.prices.size() - 20; i < data.prices.size(); i++) {
                sum_20 += data.prices[i];
            }
            data.sma_20 = sum_20 / 20;
        }
        
        if (data.prices.size() >= 50) {
            double sum_50 = 0;
            for (size_t i = data.prices.size() - 50; i < data.prices.size(); i++) {
                sum_50 += data.prices[i];
            }
            data.sma_50 = sum_50 / 50;
        }
    }
    
    void calculate_rsi(SymbolData& data) {
        if (data.prices.size() < 15) return;
        
        double avg_gain = 0, avg_loss = 0;
        size_t start = data.prices.size() - 15;
        
        for (size_t i = start; i < data.prices.size() - 1; i++) {
            double change = data.prices[i + 1] - data.prices[i];
            if (change > 0) avg_gain += change;
            else avg_loss -= change;
        }
        
        avg_gain /= 14;
        avg_loss /= 14;
        
        if (avg_loss == 0) {
            data.rsi = 100;
        } else {
            double rs = avg_gain / avg_loss;
            data.rsi = 100 - (100 / (1 + rs));
        }
    }
    
    void calculate_momentum(SymbolData& data) {
        if (data.prices.size() < 20) return;
        
        double old_price = data.prices[data.prices.size() - 20];
        double new_price = data.prices.back();
        data.momentum = (new_price - old_price) / old_price;
    }
    
    void calculate_volatility(SymbolData& data) {
        if (data.returns.size() < 20) {
            // Calculate returns
            for (size_t i = 1; i < data.prices.size(); i++) {
                double ret = (data.prices[i] - data.prices[i-1]) / data.prices[i-1];
                data.returns.push_back(ret);
                if (data.returns.size() > 20) data.returns.pop_front();
            }
        }
        
        if (data.returns.size() >= 2) {
            double mean = std::accumulate(data.returns.begin(), data.returns.end(), 0.0) / data.returns.size();
            double sq_sum = 0;
            for (double r : data.returns) {
                sq_sum += (r - mean) * (r - mean);
            }
            data.volatility = std::sqrt(sq_sum / data.returns.size()) * std::sqrt(252 * 390); // Annualized
        }
    }
};