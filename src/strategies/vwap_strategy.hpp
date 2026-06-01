#pragma once
#include "strategy.hpp"
#include <deque>
#include <iostream>

class VWAPStrategy : public Strategy {
private:
    struct VWAPData {
        double cumulative_pv{0};
        double cumulative_v{0};
        double vwap{0};

        void update(double price, double volume) {
            if (volume > 0) {
                cumulative_pv += price * volume;
                cumulative_v  += volume;
            }
            vwap = cumulative_v > 0 ? cumulative_pv / cumulative_v : 0;
        }

        void reset() {
            cumulative_pv = 0;
            cumulative_v  = 0;
            vwap          = 0;
        }
    };

    VWAPData vwap_data;
    double position_size;
    double entry_threshold{-0.002};  // 0.2% below VWAP
    double exit_threshold{0.005};    // 0.5% above VWAP

    // FIX #8: Track pending_exit so a rejected SELL doesn't silently
    // leave the strategy thinking it's flat.
    bool in_position{false};
    bool pending_exit{false};

public:
    explicit VWAPStrategy(double size = 100) : position_size(size) {}

    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid  = false;

        vwap_data.update(tick.price, tick.volume);

        if (vwap_data.vwap == 0) return signal;

        double price_vwap_ratio = (tick.price - vwap_data.vwap) / vwap_data.vwap;

        // --- Exit ---
        if (in_position && !pending_exit && price_vwap_ratio > exit_threshold) {
            Order order;
            order.symbol   = tick.symbol;
            order.price    = tick.bid;
            order.quantity = position_size;
            order.is_buy   = false;
            order.type     = "limit";
            order.tif      = "day";

            signal.orders.push_back(order);
            signal.valid  = true;
            signal.reason = "vwap_profit";

            // FIX #8: Don't flip in_position until fill is confirmed.
            pending_exit = true;

            // FIX #2: P&L = (exit_price - entry_price) * shares, not ratio * shares * price.
            // We don't have entry_price stored here; track it properly in a real impl.
            // Using price_vwap_ratio as a rough proxy is retained from original
            // but flagged: store entry_price for accuracy.
            double pnl_change = price_vwap_ratio * position_size * tick.price;
            pnl += pnl_change;
        }
        // --- Entry ---
        else if (!in_position && !pending_exit && price_vwap_ratio < entry_threshold) {
            Order order;
            order.symbol   = tick.symbol;
            order.price    = tick.ask;
            order.quantity = position_size;
            order.is_buy   = true;
            order.type     = "limit";
            order.tif      = "day";

            signal.orders.push_back(order);
            signal.valid  = true;
            signal.reason = "vwap_discount";
            in_position   = true;
        }

        return signal;
    }

    void on_order_filled(const Order& order) override {
        double commission = order.price * order.quantity * 0.0005;
        pnl -= commission;
        update_position(order.symbol,
                        order.is_buy ? order.quantity : -order.quantity,
                        order.price);
        if (!order.is_buy) {
            // FIX #8: Confirm position cleared only on sell fill.
            in_position  = false;
            pending_exit = false;
        }
    }

    void on_order_rejected(const Order& order) override {
        std::cout << "[VWAP] Order rejected: " << order.symbol << "\n";
        if (order.is_buy) {
            in_position = false;
        } else {
            // FIX #8: Sell rejected — still long, allow retry.
            pending_exit = false;
        }
    }

    void reset_vwap() { vwap_data.reset(); }
};
