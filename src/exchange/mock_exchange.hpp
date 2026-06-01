#pragma once
#include "adapter.hpp"
#include "../core/types.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <atomic>
#include <functional>
#include <mutex>

// FIX: Original used std::rand() which is not thread-safe and has poor
// distribution. Replaced with <random> throughout.
// FIX: balance was std::atomic<double> — doubles don't have atomic fetch_add
// on all platforms (only trivially-copyable integers are guaranteed lock-free
// atomics). Replaced with a double protected by a mutex.
// FIX: MockExchange::run() only simulated ONE symbol (m_symbol). If multiple
// symbols were subscribed, only the last one received ticks. Now ticks are
// emitted for every subscribed symbol.

class MockExchange : public ExchangeAdapter {
private:
    std::atomic<bool>  m_running{false};
    double             m_balance{100000.0};
    mutable std::mutex m_balance_mutex;

    std::function<void(const Tick&)> m_callback;
    std::vector<std::string>         m_symbols;
    mutable std::mutex               m_symbols_mutex;

    // Thread-safe RNG
    std::mt19937                          m_rng{std::random_device{}()};
    std::uniform_real_distribution<double> m_price_jitter{-0.1, 0.1};
    std::uniform_int_distribution<int>    m_volume_dist{100, 999};

    double m_mock_price{150.0};

public:
    bool connect() override {
        std::cout << "[MOCK] Connected\n";
        return true;
    }

    void disconnect() override {
        m_running = false;
        std::cout << "[MOCK] Disconnected\n";
    }

    bool is_connected() const override { return true; }

    std::string place_order(const Order& order) override {
        double fill_price = m_mock_price;
        double cost       = fill_price * order.quantity;

        std::lock_guard<std::mutex> lock(m_balance_mutex);
        if (order.is_buy) {
            if (m_balance < cost) {
                std::cout << "[MOCK] BUY REJECTED: insufficient balance\n";
                return "";
            }
            m_balance -= cost;
            std::cout << "[MOCK] BUY  " << order.quantity << " "
                      << order.symbol << " @ $" << fill_price << "\n";
        } else {
            m_balance += cost;
            std::cout << "[MOCK] SELL " << order.quantity << " "
                      << order.symbol << " @ $" << fill_price << "\n";
        }
        return "mock_order_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    bool cancel_order(const std::string& id) override {
        std::cout << "[MOCK] Cancel: " << id << "\n";
        return true;
    }

    std::vector<Order> get_open_orders() override { return {}; }

    double get_balance() const override {
        std::lock_guard<std::mutex> lock(m_balance_mutex);
        return m_balance;
    }

    std::vector<Position> get_positions() override { return {}; }

    AccountInfo get_account_info() override {
        std::lock_guard<std::mutex> lock(m_balance_mutex);
        AccountInfo info;
        info.id           = "MOCK_ACCOUNT";
        info.cash         = m_balance;
        info.buying_power = m_balance * 3;
        info.equity       = m_balance;
        info.portfolio_value = m_balance;
        return info;
    }

    Tick get_current_tick(const std::string& symbol) override {
        Tick tick;
        tick.symbol = symbol;
        tick.price  = m_mock_price;
        tick.bid    = m_mock_price - 0.01;
        tick.ask    = m_mock_price + 0.01;
        return tick;
    }

    void subscribe_symbol(const std::string& symbol) override {
        std::lock_guard<std::mutex> lock(m_symbols_mutex);
        if (std::find(m_symbols.begin(), m_symbols.end(), symbol) == m_symbols.end()) {
            m_symbols.push_back(symbol);
            std::cout << "[MOCK] Subscribed to: " << symbol << "\n";
        }
    }

    void set_tick_callback(std::function<void(const Tick&)> callback) override {
        m_callback = callback;
    }

    void run() override {
        m_running = true;
        std::cout << "[MOCK] Starting data stream...\n";

        while (m_running) {
            // Brownian motion price walk
            m_mock_price += m_price_jitter(m_rng);
            m_mock_price  = std::max(50.0, std::min(250.0, m_mock_price));

            if (m_callback) {
                // Emit a tick for EVERY subscribed symbol
                std::lock_guard<std::mutex> lock(m_symbols_mutex);
                for (const auto& sym : m_symbols) {
                    Tick tick;
                    tick.timestamp = static_cast<uint64_t>(
                        std::chrono::system_clock::now().time_since_epoch().count());
                    tick.symbol    = sym;
                    tick.price     = m_mock_price;
                    tick.volume    = static_cast<double>(m_volume_dist(m_rng));
                    tick.bid       = m_mock_price - 0.01;
                    tick.ask       = m_mock_price + 0.01;
                    tick.bid_size  = 100;
                    tick.ask_size  = 100;
                    m_callback(tick);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void stop() override {
        m_running = false;
        std::cout << "[MOCK] Stopping...\n";
    }
};