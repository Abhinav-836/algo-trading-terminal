#pragma once

#include "strategy.hpp"

#include <unordered_map>
#include <string>
#include <iostream>
#include <sstream>
#include <atomic>

class MarketMakerStrategy : public Strategy {
private:
    struct Quote {
        double bid;
        double ask;
        double bid_size;
        double ask_size;
    };

    std::unordered_map<std::string, Quote> quotes;
    std::unordered_map<std::string, double> inventory;

    double target_spread{0.001};   // 0.1%
    double order_size{100.0};
    double max_inventory{1000.0};

    // Simple order id generator
    inline uint64_t make_order_id(const std::string&, bool) {
    static std::atomic<uint64_t> counter{1};
    return counter++;
}

public:
    MarketMakerStrategy() = default;

    MarketMakerStrategy(double spread, double size)
        : target_spread(spread), order_size(size) {}

    Signal generate_signal(const Tick& tick) override {
        const std::string& symbol = tick.symbol;

        // Mid price
        double mid = tick.mid_price();

        // Base quotes
        double our_bid = mid * (1.0 - target_spread / 2.0);
        double our_ask = mid * (1.0 + target_spread / 2.0);

        // Inventory skew
        double inv = inventory[symbol];
        double skew = -inv / max_inventory * target_spread * 0.5;

        our_bid *= (1.0 + skew);
        our_ask *= (1.0 + skew);

        // Store quote
        quotes[symbol] = Quote{
            our_bid,
            our_ask,
            order_size,
            order_size
        };

        Signal signal;
        signal.symbol = symbol;
        signal.valid = true;

        // BID order
        signal.orders.push_back(Order{
            .id = make_order_id(symbol, true),
            .symbol = symbol,
            .price = our_bid,
            .quantity = order_size,
            .is_buy = true,
            .type = "limit",
            .tif = "day"
        });

        // ASK order
        signal.orders.push_back(Order{
            .id = make_order_id(symbol, false),
            .symbol = symbol,
            .price = our_ask,
            .quantity = order_size,
            .is_buy = false,
            .type = "limit",
            .tif = "day"
        });

        return signal;
    }

    void on_order_filled(const Order& order) override {
        double delta = order.is_buy ? order.quantity : -order.quantity;
        inventory[order.symbol] += delta;

        // Simplified cost model (0.1%)
        pnl -= std::abs(delta * order.price) * 0.001;

        update_position(order.symbol, delta, order.price);
    }

    void on_order_rejected(const Order& order) override {
        std::cout << "[MM] Order rejected: "
                  << order.symbol << " "
                  << (order.is_buy ? "BUY" : "SELL")
                  << " qty=" << order.quantity
                  << std::endl;
    }
};
