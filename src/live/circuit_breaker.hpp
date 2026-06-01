#pragma once
#include <chrono>
#include <atomic>
#include <mutex>
#include <iostream>

// FIX #6: CircuitBreaker was defined but never called from OrderManager or main.
// It is now designed to be injected into OrderManager (see order_manager.hpp).
// All mutable state is protected by a mutex so it is safe to call from the
// order processing thread.

class CircuitBreaker {
private:
    double daily_loss_limit{0.05};      // 5% daily max loss
    double daily_pnl{0.0};
    double initial_capital{100000.0};
    int trade_count{0};
    int max_daily_trades{50};
    std::chrono::system_clock::time_point last_reset;
    std::atomic<bool> tripped{false};
    mutable std::mutex mtx;

public:
    explicit CircuitBreaker(double capital = 100000.0)
        : initial_capital(capital)
    {
        last_reset = std::chrono::system_clock::now();
    }

    // Check whether a new day has started and reset counters if so.
    void check_and_reset_daily() {
        std::lock_guard<std::mutex> lock(mtx);
        auto now   = std::chrono::system_clock::now();
        auto last  = std::chrono::system_clock::to_time_t(last_reset);
        auto now_t = std::chrono::system_clock::to_time_t(now);

        if (std::difftime(now_t, last) >= 86400) {
            daily_pnl  = 0.0;
            trade_count = 0;
            tripped    = false;
            last_reset = now;
            std::cout << "[CircuitBreaker] Daily counters reset for new session.\n";
        }
    }

    // Returns false and logs the reason whenever trading should halt.
    bool can_trade() {
        check_and_reset_daily();

        if (tripped.load()) return false;

        std::lock_guard<std::mutex> lock(mtx);

        double loss_percent = -daily_pnl / initial_capital;

        if (loss_percent >= daily_loss_limit) {
            tripped = true;
            std::cout << "\n🔴 CIRCUIT BREAKER TRIPPED! Daily loss limit ("
                      << (daily_loss_limit * 100) << "%) reached.\n";
            return false;
        }

        if (trade_count >= max_daily_trades) {
            std::cout << "\n🟡 Daily trade limit (" << max_daily_trades << ") reached.\n";
            return false;
        }

        return true;
    }

    // Call after every confirmed fill with the trade's realised P&L.
    void record_trade(double pnl) {
        std::lock_guard<std::mutex> lock(mtx);
        daily_pnl += pnl;
        trade_count++;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mtx);
        daily_pnl  = 0.0;
        trade_count = 0;
        tripped    = false;
    }

    double get_daily_pnl() const {
        std::lock_guard<std::mutex> lock(mtx);
        return daily_pnl;
    }

    int get_trade_count() const {
        std::lock_guard<std::mutex> lock(mtx);
        return trade_count;
    }

    bool is_tripped() const { return tripped.load(); }
};