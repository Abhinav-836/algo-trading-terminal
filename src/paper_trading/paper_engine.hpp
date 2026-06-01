#pragma once
#include "../core/types.hpp"
#include <unordered_map>
#include <string>
#include <iostream>
#include <mutex>
#include <atomic>
#include <chrono>

// FIX: Two bugs corrected:
//
// 1. DEADLOCK — process_market_order() calls std::lock_guard at the top,
//    but submit_order() already holds the lock when it calls
//    process_market_order() for market orders.  This is a recursive
//    lock attempt on a non-recursive std::mutex → undefined behaviour
//    (deadlock on most implementations).
//    Fixed by making process_market_order() a private helper that
//    assumes the lock is already held (no lock inside).
//
// 2. std::atomic<double> — atomic<double> is not lock-free on all platforms
//    before C++20, and arithmetic (+=/-=) isn't available atomically anyway.
//    balance is now a plain double protected by the existing mutex.

class PaperEngine {
private:
    struct PaperOrder {
        Order       order;
        double      filled{0.0};
        std::string status{"pending"};
        uint64_t    timestamp;
    };

    std::unordered_map<uint64_t, PaperOrder>    orders;
    std::unordered_map<std::string, double>     positions;
    double                                       balance;          // FIX: plain double, guarded by mutex
    std::atomic<uint64_t>                        next_order_id{1};
    mutable std::mutex                           mutex;

    double slippage_rate{0.0001};   // 0.01%
    double commission_rate{0.001};  // 0.10%

    // FIX: Called with mutex already held — no lock inside.
    void process_market_order_locked(uint64_t order_id) {
        auto it = orders.find(order_id);
        if (it == orders.end()) return;

        PaperOrder&  paper_order = it->second;
        const Order& order       = paper_order.order;

        double fill_price = order.price > 0 ? order.price : 150.0;
        double slippage   = fill_price * slippage_rate;
        fill_price += order.is_buy ? slippage : -slippage;

        double commission = fill_price * order.quantity * commission_rate;

        if (order.is_buy) {
            double cost = fill_price * order.quantity + commission;
            if (balance < cost) {
                paper_order.status = "rejected";
                std::cout << "[PAPER] BUY REJECTED: Insufficient balance\n";
                return;
            }
            balance              -= cost;
            positions[order.symbol] += order.quantity;
        } else {
            if (positions[order.symbol] < order.quantity) {
                paper_order.status = "rejected";
                std::cout << "[PAPER] SELL REJECTED: Insufficient position\n";
                return;
            }
            double revenue = fill_price * order.quantity - commission;
            balance              += revenue;
            positions[order.symbol] -= order.quantity;

            if (positions[order.symbol] <= 0)
                positions.erase(order.symbol);
        }

        paper_order.filled = order.quantity;
        paper_order.status = "filled";

        std::cout << "[PAPER] " << (order.is_buy ? "BUY" : "SELL")
                  << " "  << order.quantity << " " << order.symbol
                  << " @ $" << fill_price
                  << " | Commission: $" << commission
                  << " | Balance: $"    << balance << "\n";
    }

public:
    explicit PaperEngine(double initial_balance = 100000.0)
        : balance(initial_balance) {}

    uint64_t submit_order(const Order& order) {
        std::lock_guard<std::mutex> lock(mutex);

        uint64_t order_id = next_order_id++;

        PaperOrder paper_order;
        paper_order.order     = order;
        paper_order.filled    = 0.0;
        paper_order.status    = "pending";
        paper_order.timestamp = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());

        orders[order_id] = paper_order;

        // FIX: call the unlocked helper — mutex is already held here.
        if (order.is_market())
            process_market_order_locked(order_id);

        return order_id;
    }

    bool cancel_order(uint64_t order_id) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = orders.find(order_id);
        if (it == orders.end()) return false;
        it->second.status = "cancelled";
        return true;
    }

    double get_balance() const {
        std::lock_guard<std::mutex> lock(mutex);
        return balance;
    }

    double get_position(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = positions.find(symbol);
        return it != positions.end() ? it->second : 0.0;
    }

    std::unordered_map<std::string, double> get_all_positions() const {
        std::lock_guard<std::mutex> lock(mutex);
        return positions;
    }

    // FIX: get_portfolio_value() originally accepted a single default
    // current_price (150.0) for ALL positions — obviously wrong for a
    // multi-symbol portfolio.  Accept a symbol→price map instead.
    double get_portfolio_value(
        const std::unordered_map<std::string, double>& current_prices = {}) const {
        std::lock_guard<std::mutex> lock(mutex);
        double total = balance;
        for (const auto& [sym, qty] : positions) {
            auto it = current_prices.find(sym);
            double price = it != current_prices.end() ? it->second : 0.0;
            total += qty * price;
        }
        return total;
    }

    void set_slippage(double rate)   { slippage_rate   = rate; }
    void set_commission(double rate) { commission_rate = rate; }

    void reset(double new_balance = 100000.0) {
        std::lock_guard<std::mutex> lock(mutex);
        balance       = new_balance;
        positions.clear();
        orders.clear();
        next_order_id = 1;
    }
};