// src/strategies/enhanced_intraday.hpp
#pragma once
#include "strategy.hpp"
#include "../strategies/regime_detector.hpp"
#include "../risk/kelly_sizer.hpp"
#include "../live/circuit_breaker.hpp"
#include <deque>

class EnhancedIntradayStrategy : public Strategy {
private:
    RegimeDetector regime;
    KellyPositionSizer kelly;
    CircuitBreaker breaker;
    std::deque<double> price_history;
    
    double position_size{100};
    double rsi_period{14};
    double ema_fast{9};
    double ema_slow{21};
    
    double calculate_ema(const std::deque<double>& prices, int period) {
        if (prices.size() < period) return prices.back();
        
        double multiplier = 2.0 / (period + 1);
        double ema = prices[0];
        
        for (size_t i = 1; i < prices.size(); i++) {
            ema = (prices[i] - ema) * multiplier + ema;
        }
        return ema;
    }
    
    double calculate_rsi() {
        if (price_history.size() < rsi_period + 1) return 50;
        
        double gain = 0, loss = 0;
        size_t start = price_history.size() - rsi_period - 1;
        
        for (size_t i = start; i < price_history.size() - 1; i++) {
            double change = price_history[i + 1] - price_history[i];
            if (change > 0) gain += change;
            else loss -= change;
        }
        
        gain /= rsi_period;
        loss /= rsi_period;
        
        if (loss == 0) return 100;
        double rs = gain / loss;
        return 100 - (100 / (1 + rs));
    }
    
public:
    EnhancedIntradayStrategy(double size = 100) : position_size(size) {
        breaker = CircuitBreaker(100000);
    }
    
    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid = false;
        
        // Update data
        price_history.push_back(tick.price);
        if (price_history.size() > 100) price_history.pop_front();
        
        regime.add_data(tick.price, tick.volume);
        
        // Check circuit breaker
        if (!breaker.can_trade()) return signal;
        
        if (price_history.size() < 50) return signal;
        
        // Calculate indicators
        double ema_fast_val = calculate_ema(price_history, ema_fast);
        double ema_slow_val = calculate_ema(price_history, ema_slow);
        double rsi = calculate_rsi();
        
        // Get regime multiplier
        double regime_mult = regime.get_position_multiplier();
        if (regime_mult == 0) return signal;  // Crash mode
        
        // Calculate Kelly position size
        double capital = 100000;  // Get from account
        double kelly_size = kelly.calculate_position_size(capital, tick.price);
        double final_size = position_size * regime_mult;
        final_size = std::min(final_size, kelly_size);
        
        // Entry conditions
        bool buy_signal = false;
        std::string reason;
        
        if (ema_fast_val > ema_slow_val && rsi < 70 && rsi > 30) {
            buy_signal = true;
            reason = "trend_up";
        }
        else if (rsi < 30 && tick.price < ema_slow_val * 0.99) {
            buy_signal = true;
            reason = "rsi_oversold";
        }
        
        if (buy_signal && !in_position) {
            Order order;
            order.symbol = tick.symbol;
            order.price = tick.ask;
            order.quantity = final_size;
            order.is_buy = true;
            order.type = "market";
            order.tif = "day";
            
            signal.orders.push_back(order);
            signal.valid = true;
            signal.reason = reason;
            signal.confidence = rsi / 100;
            in_position = true;
            
            std::cout << "\n📈 ENTRY | Regime: " << regime.get_regime_name()
                      << " | Size: " << final_size
                      << " | Kelly: " << (kelly_size / position_size) * 100 << "%"
                      << " | RSI: " << rsi << std::endl;
        }
        
        return signal;
    }
    
    void on_order_filled(const Order& order) override {
        double commission = order.price * order.quantity * 0.0005;
        pnl -= commission;
        update_position(order.symbol, order.is_buy ? order.quantity : -order.quantity, order.price);
        
        // Update Kelly with trade result
        // In real implementation, track actual P&L percentage
    }
    
    void on_order_rejected(const Order& order) override {
        std::cout << "[ENHANCED] Order rejected: " << order.symbol << std::endl;
        in_position = false;
    }
    
    void update_pnl(double pnl_amount) {
        pnl += pnl_amount;
        breaker.record_trade(pnl_amount);
        
        // Update Kelly with return percentage
        double return_pct = pnl_amount / 100000;  // Assuming 100k capital
        kelly.add_return(return_pct);
    }
    
    bool in_position{false};
};