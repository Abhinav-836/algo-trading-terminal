#pragma once
#include "../core/types.hpp"
#include <vector>
#include <string>
#include <algorithm>

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
        for (auto& pos : positions) {
            if (pos.symbol == symbol) {
                double total_qty = pos.quantity + quantity;
                if (total_qty != 0) {
                    pos.avg_entry_price = (pos.avg_entry_price * pos.quantity + price * quantity) / total_qty;
                }
                pos.quantity = total_qty;
                pos.current_price = price;
                return;
            }
        }
        
        if (quantity != 0) {
            positions.push_back(Position{symbol, quantity, price, price, 0, 0, 0});
        } else {
            auto it = std::remove_if(positions.begin(), positions.end(),
                [&symbol](const Position& p) { return p.symbol == symbol; });
            positions.erase(it, positions.end());
        }
    }
    
    double get_pnl() const { return pnl; }
    const std::vector<Position>& get_positions() const { return positions; }
    void reset_pnl() { pnl = 0.0; }
};