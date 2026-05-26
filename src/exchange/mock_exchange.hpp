#pragma once
#include "adapter.hpp"
#include "../core/types.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <atomic>
#include <functional>

class MockExchange : public ExchangeAdapter {
private:
    std::atomic<bool> m_running{true};
    std::atomic<double> m_balance{100000.0};
    std::function<void(const Tick&)> m_callback;
    std::string m_symbol;
    
public:
    // Connection
    bool connect() override { 
        std::cout << "[MOCK] Connected\n"; 
        return true; 
    }
    void disconnect() override { 
        m_running = false; 
        std::cout << "[MOCK] Disconnected\n";
    }
    bool is_connected() const override { return true; }
    
    // Orders
    std::string place_order(const Order& order) override {
        double price = 150.0;
        double cost = price * order.quantity;
        double current = m_balance.load();
        
        if (order.is_buy) {
            if (current >= cost) {
                m_balance.store(current - cost);
                std::cout << "[MOCK] BUY " << order.quantity << " " << order.symbol << " @ $" << price << "\n";
                return "mock_order_" + std::to_string(rand());
            }
        } else {
            m_balance.store(current + cost);
            std::cout << "[MOCK] SELL " << order.quantity << " " << order.symbol << " @ $" << price << "\n";
            return "mock_order_" + std::to_string(rand());
        }
        return "";
    }
    
    bool cancel_order(const std::string& order_id) override { 
        std::cout << "[MOCK] Cancel order: " << order_id << "\n";
        return true; 
    }
    
    std::vector<Order> get_open_orders() override { return {}; }
    
    // Account & Positions
    double get_balance() const override { return m_balance.load(); }
    
    std::vector<Position> get_positions() override { return {}; }
    
    AccountInfo get_account_info() override { 
        AccountInfo info;
        info.id = "MOCK_ACCOUNT";
        info.cash = m_balance.load();
        info.buying_power = m_balance.load() * 3;
        info.equity = m_balance.load();
        return info;
    }
    
    // Market Data
    Tick get_current_tick(const std::string& symbol) override { 
        Tick tick;
        tick.symbol = symbol;
        tick.price = 150.0;
        return tick;
    }
    
    void subscribe_symbol(const std::string& symbol) override { 
        m_symbol = symbol;
        std::cout << "[MOCK] Subscribed to: " << symbol << "\n";
    }
    
    void set_tick_callback(std::function<void(const Tick&)> callback) override { 
        m_callback = callback; 
    }
    
    // Main event loop
    void run() override {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-0.5, 0.5);
        double price = 150.0;
        
        std::cout << "[MOCK] Starting data stream...\n";
        
        while (m_running) {
            price += dis(gen) * 0.1;
            price = std::max(100.0, std::min(200.0, price));
            Tick tick;
            tick.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            tick.symbol = m_symbol.empty() ? "AAPL" : m_symbol;
            tick.price = price;
            tick.volume = 100 + (rand() % 900);
            tick.bid = price - 0.01;
            tick.ask = price + 0.01;
            tick.bid_size = 100;
            tick.ask_size = 100;
            
            if (m_callback) m_callback(tick);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void stop() override { 
        m_running = false; 
        std::cout << "[MOCK] Stopping...\n";
    }
};
