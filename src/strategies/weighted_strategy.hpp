#pragma once
#include "strategy.hpp"
#include <deque>
#include <cmath>
#include <numeric>
#include <unordered_map>
#include <chrono>
#include <iomanip>
#include <iostream>

class WeightedStrategy : public Strategy {
private:
    struct IndicatorWeights {
        double momentum_weight{0.30};
        double rsi_weight{0.20};
        double volume_weight{0.15};
        double vwap_weight{0.15};
        double sma_weight{0.20};   // FIX #12: sma_score now has its own weight slot
    };

    struct SignalData {
        std::deque<double> prices;
        std::deque<double> volumes;

        double sma_20{0}, sma_50{0};
        double rsi{50};
        double momentum{0};
        double volume_ratio{1.0};

        // FIX #3: VWAP accumulators moved into per-symbol SignalData so they
        // are independent across symbols and can be reset each session.
        double cumulative_pv{0.0};
        double cumulative_v{0.0};
        double vwap{0.0};
        double vwap_distance{0.0};

        double      weighted_score{0};
        std::string signal_reason;

        bool     in_position{false};
        bool     pending_exit{false};   // FIX #8
        double   entry_price{0};
        uint64_t entry_time{0};
        double   trailing_stop{0};
        double   highest_price{0};

        static constexpr size_t WINDOW = 50;
    };

    std::unordered_map<std::string, SignalData> m_data;

    IndicatorWeights m_weights;

    double m_position_size{100.0};
    double m_entry_threshold{0.6};
    double m_exit_threshold{0.3};
    double m_trailing_stop_pct{0.015};
    double m_take_profit_pct{0.03};
    double m_max_hold_seconds{180};

    double m_rsi_upper{70};
    double m_rsi_lower{30};

public:
    WeightedStrategy() = default;

    WeightedStrategy(double position_size, double entry_threshold, double exit_threshold)
        : m_position_size(position_size)
        , m_entry_threshold(entry_threshold)
        , m_exit_threshold(exit_threshold) {}

    // FIX #12: Updated signature to accept sma_weight.
    // Weights are automatically normalised to sum to 1.0 to prevent silent
    // misconfiguration.
    void set_weights(double momentum_w, double rsi_w, double volume_w,
                     double vwap_w, double sma_w = 0.20) {
        double total = momentum_w + rsi_w + volume_w + vwap_w + sma_w;
        if (total <= 0) return;
        m_weights.momentum_weight = momentum_w / total;
        m_weights.rsi_weight      = rsi_w      / total;
        m_weights.volume_weight   = volume_w   / total;
        m_weights.vwap_weight     = vwap_w     / total;
        m_weights.sma_weight      = sma_w      / total;
    }

    Signal generate_signal(const Tick& tick) override {
        Signal signal;
        signal.symbol = tick.symbol;
        signal.valid  = false;

        auto& data = m_data[tick.symbol];

        data.prices.push_back(tick.price);
        data.volumes.push_back(tick.volume);

        while (data.prices.size() > SignalData::WINDOW) {
            data.prices.pop_front();
            data.volumes.pop_front();
        }

        if (data.prices.size() < 30) return signal;

        calculate_sma(data);
        calculate_rsi(data);
        calculate_momentum(data);
        calculate_volume_ratio(data);
        calculate_vwap(data, tick);   // FIX #3: now uses per-symbol state

        double score       = calculate_weighted_score(data);
        data.weighted_score = score;

        // --- Exit logic ---
        if (data.in_position) {
            uint64_t now = static_cast<uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count()) / 1000000000ULL;
            double hold_time   = static_cast<double>(now - data.entry_time);
            double price_change = (tick.price - data.entry_price) / data.entry_price;

            if (tick.price > data.highest_price) {
                data.highest_price = tick.price;
                data.trailing_stop = data.highest_price * (1.0 - m_trailing_stop_pct);
            }

            bool        should_exit = false;
            std::string exit_reason;

            if (score < m_exit_threshold) {
                should_exit = true;
                exit_reason = "weight_score_low";
            } else if (price_change >= m_take_profit_pct) {
                should_exit = true;
                exit_reason = "take_profit";
            } else if (tick.price <= data.trailing_stop) {
                should_exit = true;
                exit_reason = "trailing_stop";
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
                signal.confidence = score;

                // FIX #8: Mark pending, don't clear in_position yet.
                data.pending_exit = true;

                // FIX #2: Correct P&L — price delta * shares, not fractional * shares * price.
                double pnl_change = (tick.price - data.entry_price) * m_position_size;
                pnl += pnl_change;

                std::cout << "📉 EXIT " << tick.symbol
                          << " | Score: "  << std::fixed << std::setprecision(2) << (score * 100) << "%"
                          << " | P&L: "   << (pnl_change >= 0 ? "+" : "") << "$" << pnl_change
                          << " | Reason: " << exit_reason << "\n";
            }
        }
        // --- Entry logic ---
        else if (!data.pending_exit && score >= m_entry_threshold) {
            Order order;
            order.symbol   = tick.symbol;
            order.price    = tick.ask;
            order.quantity = m_position_size;
            order.is_buy   = true;
            order.type     = "market";
            order.tif      = "day";

            signal.orders.push_back(order);
            signal.valid      = true;
            signal.reason     = data.signal_reason;
            signal.confidence = score;

            data.in_position   = true;
            data.entry_price   = tick.price;
            data.entry_time    = static_cast<uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count()) / 1000000000ULL;
            data.highest_price = tick.price;
            data.trailing_stop = tick.price * (1.0 - m_trailing_stop_pct);

            std::cout << "📈 ENTRY " << tick.symbol
                      << " | Score: "   << std::fixed << std::setprecision(2) << (score * 100) << "%"
                      << " | Signals: " << data.signal_reason << "\n";
        }

        return signal;
    }

    void on_order_filled(const Order& order) override {
        double commission = order.price * order.quantity * 0.0005;
        pnl -= commission;
        update_position(order.symbol, order.is_buy ? order.quantity : -order.quantity, order.price);

        if (!order.is_buy) {
            // FIX #8: Clear position state only on confirmed sell fill.
            auto& data        = m_data[order.symbol];
            data.in_position  = false;
            data.pending_exit = false;
        }
    }

    void on_order_rejected(const Order& order) override {
        std::cout << "❌ ORDER REJECTED: " << order.symbol << "\n";
        auto& data = m_data[order.symbol];
        if (order.is_buy) {
            // FIX #8: Buy rejected — never entered.
            data.in_position = false;
        } else {
            // FIX #8: Sell rejected — still long; allow retry next tick.
            data.pending_exit = false;
        }
    }

    // Reset per-symbol VWAP at the start of a new session.
    void reset_vwap(const std::string& symbol) {
        auto it = m_data.find(symbol);
        if (it != m_data.end()) {
            it->second.cumulative_pv = 0.0;
            it->second.cumulative_v  = 0.0;
            it->second.vwap          = 0.0;
        }
    }

private:
    void calculate_sma(SignalData& data) {
        if (data.prices.size() >= 20) {
            double sum = 0;
            for (size_t i = data.prices.size() - 20; i < data.prices.size(); i++)
                sum += data.prices[i];
            data.sma_20 = sum / 20.0;
        }
        if (data.prices.size() >= 50) {
            double sum = 0;
            for (size_t i = 0; i < data.prices.size(); i++)
                sum += data.prices[i];
            data.sma_50 = sum / static_cast<double>(data.prices.size());
        }
    }

    void calculate_rsi(SignalData& data) {
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

        if (avg_loss == 0.0) data.rsi = 100.0;
        else {
            double rs = avg_gain / avg_loss;
            data.rsi  = 100.0 - (100.0 / (1.0 + rs));
        }
    }

    void calculate_momentum(SignalData& data) {
        if (data.prices.size() < 20) return;
        double old_price = data.prices[data.prices.size() - 20];
        data.momentum    = (data.prices.back() - old_price) / old_price;
    }

    void calculate_volume_ratio(SignalData& data) {
        if (data.volumes.size() < 20) return;

        int    half    = static_cast<int>(data.volumes.size()) / 2;
        double sum_old = 0, sum_new = 0;

        for (int i = 0; i < static_cast<int>(data.volumes.size()); i++) {
            if (i < half) sum_old += data.volumes[i];
            else          sum_new += data.volumes[i];
        }

        double avg_old = sum_old / half;
        double avg_new = sum_new / (static_cast<int>(data.volumes.size()) - half);

        data.volume_ratio = (avg_old > 0) ? avg_new / avg_old : 1.0;
    }

    // FIX #3: Uses per-symbol cumulative state instead of static locals.
    void calculate_vwap(SignalData& data, const Tick& tick) {
        if (tick.volume > 0) {
            data.cumulative_pv += tick.price * tick.volume;
            data.cumulative_v  += tick.volume;
        }
        if (data.cumulative_v > 0) {
            data.vwap          = data.cumulative_pv / data.cumulative_v;
            data.vwap_distance = (tick.price - data.vwap) / data.vwap;
        }
    }

    double calculate_weighted_score(SignalData& data) {
        std::vector<std::string> active_signals;

        // 1. Momentum score
        double momentum_score = 0;
        if      (data.momentum >  0.01) momentum_score = std::min(1.0, data.momentum / 0.05);
        else if (data.momentum < -0.01) momentum_score = 0;
        else                            momentum_score = 0.5 + data.momentum * 25;

        if (momentum_score > 0.6) active_signals.push_back("momentum");

        // 2. SMA crossover score
        double sma_score = 0;
        if      (data.prices.back() > data.sma_20 && data.sma_20 > data.sma_50) sma_score = 0.8;
        else if (data.prices.back() > data.sma_20)                               sma_score = 0.6;
        else if (data.prices.back() > data.sma_50)                               sma_score = 0.4;
        else                                                                       sma_score = 0.2;

        if (sma_score > 0.6) active_signals.push_back("sma_cross");

        // 3. RSI score
        double rsi_score = 0;
        if      (data.rsi < m_rsi_lower) rsi_score = 1.0 - (data.rsi / m_rsi_lower);
        else if (data.rsi > m_rsi_upper) rsi_score = 0;
        else                             rsi_score = 0.5;

        if (rsi_score > 0.6) active_signals.push_back("rsi_oversold");

        // 4. Volume score
        double volume_score = std::max(0.0, std::min(1.0, (data.volume_ratio - 1.0) / 2.0));
        if (volume_score > 0.5) active_signals.push_back("high_volume");

        // 5. VWAP score
        double vwap_score = 0;
        if      (data.vwap_distance < -0.005) vwap_score = 0.8;
        else if (data.vwap_distance <  0)     vwap_score = 0.6;
        else if (data.vwap_distance >  0.005) vwap_score = 0.2;
        else                                   vwap_score = 0.4;

        if (vwap_score > 0.6) active_signals.push_back("below_vwap");

        // FIX #12: sma_score is now included in the weighted sum.
        double score = momentum_score * m_weights.momentum_weight
                     + rsi_score      * m_weights.rsi_weight
                     + volume_score   * m_weights.volume_weight
                     + vwap_score     * m_weights.vwap_weight
                     + sma_score      * m_weights.sma_weight;   // was missing

        // Bonus for multiple confirmations
        if (active_signals.size() >= 3)
            score = std::min(1.0, score * 1.2);

        // Build reason string
        data.signal_reason.clear();
        for (size_t i = 0; i < active_signals.size() && i < 3; i++) {
            if (i > 0) data.signal_reason += "+";
            data.signal_reason += active_signals[i];
        }
        if (active_signals.empty()) data.signal_reason = "no_signal";

        return score;
    }
};