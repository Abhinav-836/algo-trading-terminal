#pragma once
#include "strategy.hpp"
#include "regime_detector.hpp"
#include "../risk/kelly_sizer.hpp"
#include "../live/circuit_breaker.hpp"
#include <deque>
#include <iostream>
#include <algorithm>

// FIX: in_position was a public data member — moved to private.
// FIX: CircuitBreaker is now taken by reference (injected), not constructed
//      internally with a hardcoded capital value. Caller owns the breaker.
// FIX: pending_exit guard added (same bug #8 as other strategies).
// FIX: regime_detector static last_price bug is fixed in regime_detector.hpp.

class EnhancedIntradayStrategy : public Strategy {
private:
    RegimeDetector   regime;
    KellyPositionSizer kelly;
    CircuitBreaker&  breaker;   // Injected — NOT owned here

    std::deque<double> price_history;

    double position_size{100};
    int    rsi_period{14};
    int    ema_fast_period{9};
    int    ema_slow_period{21};

    bool in_position{false};
    bool pending_exit{false};   // FIX #8

    double entry_price{0.0};
    double capital{100000.0};   // Ideally injected from account at runtime

    double calculate_ema(const std::deque<double>& prices, int period) const {
        if (static_cast<int>(prices.size()) < period) return prices.back();
        double multiplier = 2.0 / (period + 1);
        double ema = prices[0];
        for (size_t i = 1; i < prices.size(); i++)
            ema = (prices[i] - ema) * multiplier + ema;
        return ema;
    }

    double calculate_rsi() const {
        if (static_cast<int>(price_history.size()) < rsi_period + 1) return 50;

        double gain = 0, loss = 0;
        size_t start = price_history.size() - rsi_period - 1;

        for (size_t i = start; i < price_history.size() - 1; i++) {
            double change = price_history[i + 1] - price_history[i];
            if (change > 0) gain += change;
            else            loss -= change;
        }

        gain /= rsi_period;
        loss /= rsi_period;

        if (loss == 0) return 100;
        return 100.0 - (100.0 / (1.0 + gain / loss));
    }

public:
    // Requires a CircuitBreaker reference so the same instance can be shared
    // with OrderManager, ensuring consistent daily-loss tracking.
    explicit EnhancedIntradayStrategy(CircuitBreaker& cb, double size = 100)
        : breaker(cb), position_size(size) {}

    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid  = false;

        price_history.push_back(tick.price);
        if (price_history.size() > 100) price_history.pop_front();

        regime.add_data(tick.price, tick.volume);

        if (!breaker.can_trade()) return signal;
        if (static_cast<int>(price_history.size()) < 50) return signal;

        double ema_fast_val = calculate_ema(price_history, ema_fast_period);
        double ema_slow_val = calculate_ema(price_history, ema_slow_period);
        double rsi          = calculate_rsi();

        double regime_mult = regime.get_position_multiplier();
        if (regime_mult == 0) return signal;  // Crash mode

        double kelly_size  = kelly.calculate_position_size(capital, tick.price);
        double final_size  = std::min(position_size * regime_mult, kelly_size);

        // --- Exit ---
        if (in_position && !pending_exit) {
            double price_change = entry_price > 0
                                      ? (tick.price - entry_price) / entry_price
                                      : 0.0;
            bool should_exit = rsi > 70                     // overbought
                            || price_change <= -0.02        // 2% stop loss
                            || price_change >=  0.05;       // 5% take profit

            if (should_exit) {
                Order order;
                order.symbol   = tick.symbol;
                order.price    = tick.bid;
                order.quantity = final_size;
                order.is_buy   = false;
                order.type     = "market";
                order.tif      = "day";

                signal.orders.push_back(order);
                signal.valid   = true;
                signal.reason  = "exit";
                pending_exit   = true;

                // FIX #2: Correct P&L
                double pnl_change = (tick.price - entry_price) * final_size;
                pnl += pnl_change;
            }
        }
        // --- Entry ---
        else if (!in_position && !pending_exit) {
            bool buy_signal = false;
            std::string reason;

            if (ema_fast_val > ema_slow_val && rsi < 70 && rsi > 30) {
                buy_signal = true;
                reason     = "trend_up";
            } else if (rsi < 30 && tick.price < ema_slow_val * 0.99) {
                buy_signal = true;
                reason     = "rsi_oversold";
            }

            if (buy_signal) {
                Order order;
                order.symbol   = tick.symbol;
                order.price    = tick.ask;
                order.quantity = final_size;
                order.is_buy   = true;
                order.type     = "market";
                order.tif      = "day";

                signal.orders.push_back(order);
                signal.valid      = true;
                signal.reason     = reason;
                signal.confidence = rsi / 100.0;
                in_position       = true;
                entry_price       = tick.price;

                std::cout << "\n📈 ENTRY | Regime: " << regime.get_regime_name()
                          << " | Size: "   << final_size
                          << " | Kelly%: " << (kelly_size / position_size) * 100
                          << " | RSI: "    << rsi << "\n";
            }
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
            // FIX #8: Clear state only on confirmed sell fill.
            in_position  = false;
            pending_exit = false;
            entry_price  = 0.0;
        }
    }

    void on_order_rejected(const Order& order) override {
        std::cout << "[ENHANCED] Order rejected: " << order.symbol << "\n";
        if (order.is_buy) {
            in_position = false;
            entry_price = 0.0;
        } else {
            // FIX #8: Still long — allow retry.
            pending_exit = false;
        }
    }

    void update_pnl(double pnl_amount) {
        pnl += pnl_amount;
        breaker.record_trade(pnl_amount);
        double return_pct = capital > 0 ? pnl_amount / capital : 0.0;
        kelly.add_return(return_pct);
    }

    void set_capital(double c) { capital = c; }
};