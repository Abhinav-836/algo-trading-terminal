#pragma once
#include "strategy.hpp"
#include <deque>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <iostream>

class MomentumStrategy : public Strategy {
private:
    struct SymbolData {
        std::deque<double> prices;
        std::deque<double> volumes;

        // FIX #7: Keep a rolling returns deque updated incrementally
        // (one new value per tick) instead of recalculating the whole
        // series from scratch on every tick until size reaches 20.
        std::deque<double> returns;
        double prev_price{0.0};   // last price seen, for incremental return

        double sma_20{0.0};
        double sma_50{0.0};
        double rsi{50.0};
        double momentum{0.0};
        double volatility{0.0};

        bool     in_position{false};
        bool     pending_exit{false};   // FIX #8: tracks outstanding exit orders
        double   entry_price{0.0};
        uint64_t entry_time{0};
        double   trailing_stop{0.0};
        double   highest_price{0.0};

        static constexpr size_t WINDOW_20 = 20;
        static constexpr size_t WINDOW_50 = 50;
        static constexpr size_t RETURN_WINDOW = 20;
    };

    std::unordered_map<std::string, SymbolData> m_data;

    // Strategy parameters
    double m_position_size{100.0};
    double m_rsi_upper{70.0};
    double m_rsi_lower{30.0};
    double m_trailing_stop_pct{0.02};   // 2% trailing stop
    double m_take_profit_pct{0.05};     // 5% take profit
    double m_max_hold_seconds{300};     // 5 minutes max hold

public:
    MomentumStrategy() = default;

    MomentumStrategy(double position_size, double rsi_upper, double rsi_lower)
        : m_position_size(position_size)
        , m_rsi_upper(rsi_upper)
        , m_rsi_lower(rsi_lower) {}

    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid  = false;

        auto& data = m_data[tick.symbol];

        // --- Update price history ---
        data.prices.push_back(tick.price);
        data.volumes.push_back(tick.volume);

        while (data.prices.size() > SymbolData::WINDOW_50) {
            data.prices.pop_front();
            data.volumes.pop_front();
        }

        // FIX #7: Append exactly one incremental return per tick.
        if (data.prev_price > 0.0) {
            double r = (tick.price - data.prev_price) / data.prev_price;
            data.returns.push_back(r);
            if (data.returns.size() > SymbolData::RETURN_WINDOW)
                data.returns.pop_front();
        }
        data.prev_price = tick.price;

        // Need enough data before trading
        if (data.prices.size() < SymbolData::WINDOW_50) return signal;

        calculate_sma(data);
        calculate_rsi(data);
        calculate_momentum(data);
        calculate_volatility(data);

        // --- Exit logic ---
        if (data.in_position) {
            uint64_t now = static_cast<uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count()) / 1000000000ULL;
            double hold_time   = static_cast<double>(now - data.entry_time);
            double price_change = (tick.price - data.entry_price) / data.entry_price;

            // Update trailing stop
            if (tick.price > data.highest_price) {
                data.highest_price = tick.price;
                data.trailing_stop = data.highest_price * (1.0 - m_trailing_stop_pct);
            }

            bool        should_exit = false;
            std::string exit_reason;

            if (price_change >= m_take_profit_pct) {
                should_exit = true;
                exit_reason = "take_profit";
            } else if (tick.price <= data.trailing_stop) {
                should_exit = true;
                exit_reason = "trailing_stop";
            } else if ((price_change > 0 && data.rsi > 80) ||
                       (price_change < 0 && data.rsi < 20)) {
                should_exit = true;
                exit_reason = "rsi_reversal";
            } else if (hold_time > m_max_hold_seconds) {
                should_exit = true;
                exit_reason = "max_hold";
            }

            if (should_exit && !data.pending_exit) {
                Order exit_order;
                exit_order.symbol   = tick.symbol;
                exit_order.price    = tick.bid;
                exit_order.quantity = m_position_size;
                exit_order.is_buy   = false;
                exit_order.type     = "market";
                exit_order.tif      = "day";

                signal.orders.push_back(exit_order);
                signal.valid      = true;
                signal.reason     = exit_reason;
                signal.confidence = 0.9;

                // FIX #8: Mark pending_exit instead of immediately clearing
                // in_position.  in_position is cleared only on confirmed fill
                // or confirmed rejection of the SELL order in on_order_filled /
                // on_order_rejected.
                data.pending_exit = true;

                // FIX #2: Correct P&L: (exit_price - entry_price) * shares
                double pnl_change = (tick.price - data.entry_price) * m_position_size;
                pnl += pnl_change;

                std::cout << "📉 EXIT " << tick.symbol
                          << " | P&L: " << (pnl_change >= 0 ? "+" : "")
                          << "$" << pnl_change
                          << " | Reason: " << exit_reason << "\n";
            }
        }
        // --- Entry logic ---
        else if (!data.pending_exit) {
            bool        should_buy   = false;
            std::string entry_reason;
            double      confidence   = 0.0;

            if (tick.price > data.sma_20 && data.sma_20 > data.sma_50 && data.momentum > 0.01) {
                should_buy   = true;
                entry_reason = "golden_cross";
                confidence   = 0.7;
            } else if (data.rsi < m_rsi_lower && data.momentum > 0) {
                should_buy   = true;
                entry_reason = "rsi_oversold";
                confidence   = 0.6;
            } else if (data.momentum > 0.02 && data.volatility > 0.005) {
                should_buy   = true;
                entry_reason = "strong_momentum";
                confidence   = 0.5;
            }

            if (should_buy) {
                Order order;
                order.symbol   = tick.symbol;
                order.price    = tick.ask;
                order.quantity = m_position_size;
                order.is_buy   = true;
                order.type     = "market";
                order.tif      = "day";

                signal.orders.push_back(order);
                signal.valid      = true;
                signal.reason     = entry_reason;
                signal.confidence = confidence;

                data.in_position   = true;
                data.entry_price   = tick.price;
                data.entry_time    = static_cast<uint64_t>(
                    std::chrono::system_clock::now().time_since_epoch().count()) / 1000000000ULL;
                data.highest_price = tick.price;
                data.trailing_stop = tick.price * (1.0 - m_trailing_stop_pct);

                std::cout << "📈 ENTRY " << tick.symbol
                          << " | Reason: "     << entry_reason
                          << " | Confidence: " << (confidence * 100) << "%\n";
            }
        }

        return signal;
    }

    void on_order_filled(const Order& order) override {
        double commission = order.price * order.quantity * 0.0005;
        pnl -= commission;
        update_position(order.symbol, order.is_buy ? order.quantity : -order.quantity, order.price);

        if (!order.is_buy) {
            // FIX #8: Sell confirmed — now safe to clear position state.
            auto& data        = m_data[order.symbol];
            data.in_position  = false;
            data.pending_exit = false;
        }
    }

    void on_order_rejected(const Order& order) override {
        std::cout << "❌ ORDER REJECTED: " << order.symbol << " "
                  << (order.is_buy ? "BUY" : "SELL") << "\n";

        auto& data = m_data[order.symbol];

        if (order.is_buy) {
            // FIX #8: Only clear in_position when a BUY is rejected
            // (we never actually entered).
            data.in_position = false;
        } else {
            // SELL rejected — we are still long; clear the pending flag so
            // the next tick will try to exit again.
            data.pending_exit = false;
        }
    }

private:
    void calculate_sma(SymbolData& data) {
        {
            double sum = 0;
            for (size_t i = data.prices.size() - 20; i < data.prices.size(); i++)
                sum += data.prices[i];
            data.sma_20 = sum / 20.0;
        }
        {
            double sum = 0;
            for (size_t i = 0; i < data.prices.size(); i++)
                sum += data.prices[i];
            data.sma_50 = sum / static_cast<double>(data.prices.size());
        }
    }

    void calculate_rsi(SymbolData& data) {
        if (data.prices.size() < 15) return;

        double avg_gain = 0, avg_loss = 0;
        size_t start = data.prices.size() - 15;

        for (size_t i = start; i < data.prices.size() - 1; i++) {
            double change = data.prices[i + 1] - data.prices[i];
            if (change > 0) avg_gain += change;
            else            avg_loss -= change;
        }

        avg_gain /= 14.0;
        avg_loss /= 14.0;

        if (avg_loss == 0.0) {
            data.rsi = 100.0;
        } else {
            double rs = avg_gain / avg_loss;
            data.rsi  = 100.0 - (100.0 / (1.0 + rs));
        }
    }

    void calculate_momentum(SymbolData& data) {
        if (data.prices.size() < 20) return;
        double old_price = data.prices[data.prices.size() - 20];
        double new_price = data.prices.back();
        data.momentum    = (new_price - old_price) / old_price;
    }

    // FIX #7: Volatility now uses the incrementally-maintained returns deque.
    void calculate_volatility(SymbolData& data) {
        if (data.returns.size() < 2) return;

        double mean = std::accumulate(data.returns.begin(), data.returns.end(), 0.0)
                      / static_cast<double>(data.returns.size());
        double sq_sum = 0;
        for (double r : data.returns)
            sq_sum += (r - mean) * (r - mean);

        double variance  = sq_sum / static_cast<double>(data.returns.size());
        data.volatility  = std::sqrt(variance) * std::sqrt(252.0 * 390.0); // annualised
    }
};