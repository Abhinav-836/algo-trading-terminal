#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <algorithm>

class RiskManager {
private:
    struct Limits {
        double max_position_size{10000.0};      // Max position per symbol
        double max_portfolio_exposure{50000.0}; // Total exposure limit
        double max_daily_loss{5000.0};          // Daily loss limit
        double max_order_size{1000.0};          // Max single order
        double min_balance{1000.0};             // Minimum cash balance
        int max_daily_trades{100};              // Daily trade limit
    };
    
    Limits limits;
    std::unordered_map<std::string, double> positions;
    std::unordered_map<std::string, double> daily_pnl;
    std::mutex mutex;
    
    double total_exposure{0.0};
    double daily_realized_pnl{0.0};
    int today_trades{0};
    
public:
    RiskManager() = default;
    
    bool can_submit_order(const Order& order, double current_balance, double current_price) {
        std::lock_guard lock(mutex);
        
        // Check daily trade limit
        if (today_trades >= limits.max_daily_trades) {
            std::cerr << "[RISK] Daily trade limit reached" << std::endl;
            return false;
        }
        
        // Check order size
        double order_notional = order.quantity * current_price;
        if (order_notional > limits.max_order_size) {
            std::cerr << "[RISK] Order size exceeds limit: " << order_notional 
                      << " > " << limits.max_order_size << std::endl;
            return false;
        }
        
        // Check balance for buys
        if (order.is_buy) {
            if (current_balance < order_notional) {
                std::cerr << "[RISK] Insufficient balance" << std::endl;
                return false;
            }
            
            // Check position size limit
            double new_position = positions[order.symbol] + order.quantity;
            if (new_position * current_price > limits.max_position_size) {
                std::cerr << "[RISK] Position size limit exceeded for " << order.symbol << std::endl;
                return false;
            }
        } else {
            // Check we actually have the position to sell
            if (positions[order.symbol] < order.quantity) {
                std::cerr << "[RISK] Insufficient position to sell" << std::endl;
                return false;
            }
        }
        
        // Check total exposure
        double new_exposure = total_exposure + (order.is_buy ? order_notional : -order_notional);
        if (new_exposure > limits.max_portfolio_exposure) {
            std::cerr << "[RISK] Portfolio exposure limit exceeded" << std::endl;
            return false;
        }
        
        // Check daily loss limit
        if (daily_realized_pnl < -limits.max_daily_loss) {
            std::cerr << "[RISK] Daily loss limit reached" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void on_order_filled(const Order& order, double fill_price) {
        std::lock_guard lock(mutex);
        
        double notional = order.quantity * fill_price;
        
        if (order.is_buy) {
            positions[order.symbol] += order.quantity;
            total_exposure += notional;
        } else {
            positions[order.symbol] -= order.quantity;
            total_exposure -= notional;
            
            // Track realized P&L
            if (positions[order.symbol] <= 0) {
                positions.erase(order.symbol);
            }
        }
        
        today_trades++;
    }
    
    void update_daily_pnl(double realized_pnl) {
        std::lock_guard lock(mutex);
        daily_realized_pnl += realized_pnl;
    }
    
    void reset_daily() {
        std::lock_guard lock(mutex);
        daily_realized_pnl = 0.0;
        today_trades = 0;
    }
    
    void set_limits(const Limits& new_limits) {
        std::lock_guard lock(mutex);
        limits = new_limits;
    }
    
    double get_total_exposure() const {
        std::lock_guard lock(mutex);
        return total_exposure;
    }
    
    double get_daily_pnl() const {
        std::lock_guard lock(mutex);
        return daily_realized_pnl;
    }
};