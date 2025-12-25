#pragma once
#include "../core/types.hpp"
#include <vector>
#include <string>

class Strategy {
protected:
    double pnl{0.0};
    std::vector<Position> positions;
    
public:
    virtual ~Strategy() = default;
    
    virtual Signal generate_signal(const Tick& tick) = 0;
    virtual void on_order_filled(const Order& order) = 0;
    virtual void on_order_rejected(const Order& order) = 0;
    
    virtual void update_position(const std::string& symbol, double quantity, double price) {
        // Find or create position
        for (auto& pos : positions) {
            if (pos.symbol == symbol) {
                double total_qty = pos.quantity + quantity;
                pos.avg_price = (pos.avg_price * pos.quantity + price * quantity) / total_qty;
                pos.quantity = total_qty;
                return;
            }
        }
        
        // New position
        positions.push_back(Position{
            symbol,
            quantity,
            price,
            price
        });
    }
    
    double get_pnl() const { return pnl; }
    const std::vector<Position>& get_positions() const { return positions; }
};