#pragma once
#include "../core/types.hpp"   // needs Order definition
#include <string>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <iostream>

// FIX #5: RiskManager was defined but never called.
// It is now injected into OrderManager and consulted on every order.
// The Limits struct is also made public so callers can configure it via
// set_limits() without needing to know internal field names by magic string.

class RiskManager {
public:
    struct Limits {
        double max_position_size{10000.0};      // Max notional per symbol
        double max_portfolio_exposure{50000.0}; // Total long exposure limit
        double max_daily_loss{5000.0};          // Daily realised loss limit
        double max_order_size{1000.0};          // Max single order notional
        double min_balance{1000.0};             // Minimum cash balance to keep
        int    max_daily_trades{100};           // Daily trade count limit
    };

private:
    Limits limits;
    std::unordered_map<std::string, double> positions;  // qty per symbol
    mutable std::mutex mutex;

    double total_exposure{0.0};
    double daily_realized_pnl{0.0};
    int    today_trades{0};

public:
    RiskManager() = default;

    bool can_submit_order(const Order& order, double current_balance, double current_price) {
        std::lock_guard<std::mutex> lock(mutex);

        // Guard: avoid divide-by-zero on bad price
        if (current_price <= 0.0) {
            std::cerr << "[RISK] Invalid current_price (" << current_price << ")\n";
            return false;
        }

        // Daily trade limit
        if (today_trades >= limits.max_daily_trades) {
            std::cerr << "[RISK] Daily trade limit reached\n";
            return false;
        }

        // Daily loss limit
        if (daily_realized_pnl < -limits.max_daily_loss) {
            std::cerr << "[RISK] Daily loss limit reached\n";
            return false;
        }

        double order_notional = order.quantity * current_price;

        // Single order size limit
        if (order_notional > limits.max_order_size) {
            std::cerr << "[RISK] Order size exceeds limit: "
                      << order_notional << " > " << limits.max_order_size << "\n";
            return false;
        }

        if (order.is_buy) {
            // Sufficient cash check
            if (current_balance - order_notional < limits.min_balance) {
                std::cerr << "[RISK] Insufficient balance (would breach min_balance)\n";
                return false;
            }

            // Per-symbol position size check
            double current_qty = positions.count(order.symbol) ? positions.at(order.symbol) : 0.0;
            double new_notional = (current_qty + order.quantity) * current_price;
            if (new_notional > limits.max_position_size) {
                std::cerr << "[RISK] Position size limit exceeded for " << order.symbol << "\n";
                return false;
            }

            // Total portfolio exposure
            if (total_exposure + order_notional > limits.max_portfolio_exposure) {
                std::cerr << "[RISK] Portfolio exposure limit exceeded\n";
                return false;
            }
        } else {
            // Short-sell guard: must have sufficient quantity
            double current_qty = positions.count(order.symbol) ? positions.at(order.symbol) : 0.0;
            if (current_qty < order.quantity) {
                std::cerr << "[RISK] Insufficient position to sell ("
                          << current_qty << " < " << order.quantity << ")\n";
                return false;
            }
        }

        return true;
    }

    void on_order_filled(const Order& order, double fill_price) {
        std::lock_guard<std::mutex> lock(mutex);

        double notional = order.quantity * fill_price;

        if (order.is_buy) {
            positions[order.symbol] += order.quantity;
            total_exposure += notional;
        } else {
            positions[order.symbol] -= order.quantity;
            total_exposure = std::max(0.0, total_exposure - notional);

            if (positions[order.symbol] <= 0.0) {
                positions.erase(order.symbol);
            }
        }

        today_trades++;
    }

    void update_daily_pnl(double realized_pnl) {
        std::lock_guard<std::mutex> lock(mutex);
        daily_realized_pnl += realized_pnl;
    }

    void reset_daily() {
        std::lock_guard<std::mutex> lock(mutex);
        daily_realized_pnl = 0.0;
        today_trades       = 0;
    }

    void set_limits(const Limits& new_limits) {
        std::lock_guard<std::mutex> lock(mutex);
        limits = new_limits;
    }

    double get_total_exposure() const {
        std::lock_guard<std::mutex> lock(mutex);
        return total_exposure;
    }

    double get_daily_pnl() const {
        std::lock_guard<std::mutex> lock(mutex);
        return daily_realized_pnl;
    }

    int get_today_trades() const {
        std::lock_guard<std::mutex> lock(mutex);
        return today_trades;
    }
};