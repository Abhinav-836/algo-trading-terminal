#pragma once
#include "../core/types.hpp"
#include <unordered_map>
#include <mutex>
#include <deque>
#include <numeric>

class PnLTracker {
private:
    struct PositionTracker {
        double quantity{0.0};
        double avg_price{0.0};
        std::deque<std::pair<double, double>> trade_history; // price, quantity
    };
    
    std::unordered_map<std::string, PositionTracker> positions;
    std::mutex mutex;
    
    double total_realized_pnl{0.0};
    double total_fees{0.0};
    
public:
    void update_market_price(const std::string& symbol, double current_price) {
        std::lock_guard lock(mutex);
        auto it = positions.find(symbol);
        if (it != positions.end()) {
            // Just update for unrealized P&L calculation
        }
    }
    
    void record_trade(const Order& order, double fill_price, double fee = 0.0) {
        std::lock_guard lock(mutex);
        
        total_fees += fee;
        
        auto& pos = positions[order.symbol];
        
        if (order.is_buy) {
            // New average price calculation
            double total_cost = pos.avg_price * pos.quantity + fill_price * order.quantity;
            pos.quantity += order.quantity;
            pos.avg_price = pos.quantity > 0 ? total_cost / pos.quantity : 0.0;
            pos.trade_history.push_back({fill_price, order.quantity});
        } else {
            // Selling - realize P&L
            double remaining = order.quantity;
            double realized_pnl = 0.0;
            
            // FIFO accounting
            while (remaining > 0 && !pos.trade_history.empty()) {
                auto& [buy_price, buy_qty] = pos.trade_history.front();
                double sell_qty = std::min(remaining, buy_qty);
                
                realized_pnl += (fill_price - buy_price) * sell_qty;
                
                buy_qty -= sell_qty;
                remaining -= sell_qty;
                
                if (buy_qty <= 0) {
                    pos.trade_history.pop_front();
                }
            }
            
            total_realized_pnl += realized_pnl;
            pos.quantity -= order.quantity;
        }
        
        // Clean up empty positions
        if (pos.quantity <= 0) {
            positions.erase(order.symbol);
        }
    }
    
    double get_unrealized_pnl(const std::string& symbol, double current_price) const {
        std::lock_guard lock(mutex);
        auto it = positions.find(symbol);
        if (it == positions.end()) return 0.0;
        return (current_price - it->second.avg_price) * it->second.quantity;
    }
    
    double get_total_unrealized_pnl(const std::unordered_map<std::string, double>& current_prices) const {
        std::lock_guard lock(mutex);
        double total = 0.0;
        for (const auto& [symbol, pos] : positions) {
            auto price_it = current_prices.find(symbol);
            if (price_it != current_prices.end()) {
                total += (price_it->second - pos.avg_price) * pos.quantity;
            }
        }
        return total;
    }
    
    double get_total_realized_pnl() const {
        std::lock_guard lock(mutex);
        return total_realized_pnl;
    }
    
    double get_total_pnl(const std::unordered_map<std::string, double>& current_prices) const {
        return get_total_realized_pnl() + get_total_unrealized_pnl(current_prices);
    }
    
    double get_total_fees() const {
        std::lock_guard lock(mutex);
        return total_fees;
    }
    
    void reset() {
        std::lock_guard lock(mutex);
        positions.clear();
        total_realized_pnl = 0.0;
        total_fees = 0.0;
    }
};