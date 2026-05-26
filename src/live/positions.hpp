#pragma once
#include "../core/types.hpp"
#include <unordered_map>
#include <mutex>
#include <string>
#include <deque>

class PositionManager {
private:
    struct PositionDetail {
        double quantity{0.0};
        double avg_price{0.0};
        double realized_pnl{0.0};
        std::deque<std::pair<double, double>> lots; // price, quantity (FIFO)
    };
    
    std::unordered_map<std::string, PositionDetail> positions;
    mutable std::mutex mutex;
    
public:
    void add_position(const std::string& symbol, double quantity, double price) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto& pos = positions[symbol];
        
        if (quantity > 0) {  // Buy
            // Add to FIFO lots
            pos.lots.push_back({price, quantity});
            
            // Update average
            double total_cost = pos.avg_price * pos.quantity + price * quantity;
            pos.quantity += quantity;
            pos.avg_price = pos.quantity > 0 ? total_cost / pos.quantity : 0.0;
        } else {  // Sell
            double remaining = -quantity;
            
            while (remaining > 0 && !pos.lots.empty()) {
                auto& [lot_price, lot_qty] = pos.lots.front();
                double sell_qty = std::min(remaining, lot_qty);
                
                double lot_pnl = (price - lot_price) * sell_qty;
                pos.realized_pnl += lot_pnl;
                
                lot_qty -= sell_qty;
                remaining -= sell_qty;
                
                if (lot_qty <= 0) {
                    pos.lots.pop_front();
                }
            }
            
            pos.quantity += quantity;  // quantity is negative here
            if (pos.quantity <= 0) {
                positions.erase(symbol);
            } else {
                // Recalculate average for remaining position
                double total_cost = 0.0;
                double total_qty = 0.0;
                for (const auto& [lot_price, lot_qty] : pos.lots) {
                    total_cost += lot_price * lot_qty;
                    total_qty += lot_qty;
                }
                pos.avg_price = total_qty > 0 ? total_cost / total_qty : 0.0;
            }
        }
    }
    
    double get_position(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = positions.find(symbol);
        return it != positions.end() ? it->second.quantity : 0.0;
    }
    
    double get_avg_price(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = positions.find(symbol);
        return it != positions.end() ? it->second.avg_price : 0.0;
    }
    
    double get_realized_pnl(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = positions.find(symbol);
        return it != positions.end() ? it->second.realized_pnl : 0.0;
    }
    
    double get_unrealized_pnl(const std::string& symbol, double current_price) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = positions.find(symbol);
        if (it == positions.end()) return 0.0;
        return (current_price - it->second.avg_price) * it->second.quantity;
    }
    
    double get_total_realized_pnl() const {
        std::lock_guard<std::mutex> lock(mutex);
        double total = 0.0;
        for (const auto& [symbol, pos] : positions) {
            total += pos.realized_pnl;
        }
        return total;
    }
    
    std::vector<Position> get_all_positions(double current_price = 0.0) const {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<Position> result;
        for (const auto& [symbol, pos] : positions) {
            result.push_back({symbol, pos.quantity, pos.avg_price, current_price});
        }
        return result;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        positions.clear();
    }
};