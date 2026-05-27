#pragma once
#include "strategy.hpp"
#include <deque>

class VWAPStrategy : public Strategy {
private:
    struct VWAPData {
        double cumulative_pv{0};
        double cumulative_v{0};
        double vwap{0};
        
        void update(double price, double volume) {
            cumulative_pv += price * volume;
            cumulative_v += volume;
            vwap = cumulative_v > 0 ? cumulative_pv / cumulative_v : 0;
        }
        
        void reset() {
            cumulative_pv = 0;
            cumulative_v = 0;
            vwap = 0;
        }
    };
    
    VWAPData vwap;
    double position_size;
    double entry_threshold{-0.002};  // 0.2% below VWAP
    double exit_threshold{0.005};    // 0.5% above VWAP
    bool in_position{false};
    
public:
    VWAPStrategy(double size = 100) : position_size(size) {}
    
    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid = false;
        
        vwap.update(tick.price, tick.volume);
        
        if (vwap.vwap == 0) return signal;
        
        double price_vwap_ratio = (tick.price - vwap.vwap) / vwap.vwap;
        
        // Entry: Price below VWAP (discount)
        if (!in_position && price_vwap_ratio < entry_threshold) {
            Order order;
            order.symbol = tick.symbol;
            order.price = tick.ask;
            order.quantity = position_size;
            order.is_buy = true;
            order.type = "limit";
            order.tif = "day";
            
            signal.orders.push_back(order);
            signal.valid = true;
            signal.reason = "vwap_discount";
            in_position = true;
        }
        // Exit: Price above VWAP (profit)
        else if (in_position && price_vwap_ratio > exit_threshold) {
            Order order;
            order.symbol = tick.symbol;
            order.price = tick.bid;
            order.quantity = position_size;
            order.is_buy = false;
            order.type = "limit";
            order.tif = "day";
            
            signal.orders.push_back(order);
            signal.valid = true;
            signal.reason = "vwap_profit";
            in_position = false;
            
            double pnl_change = price_vwap_ratio * position_size * tick.price;
            pnl += pnl_change;
        }
        
        return signal;
    }
    
    void on_order_filled(const Order& order) override {
        double commission = order.price * order.quantity * 0.0005;
        pnl -= commission;
        update_position(order.symbol, order.is_buy ? order.quantity : -order.quantity, order.price);
    }
    
    void on_order_rejected(const Order& order) override {
        std::cout << "[VWAP] Order rejected: " << order.symbol << std::endl;
        in_position = false;
    }
    
    void reset_vwap() { vwap.reset(); }
};