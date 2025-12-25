#pragma once
#include "adapter.hpp"
#include <iostream>
#include <chrono>
#include <thread>

class MockExchange : public ExchangeAdapter {
private:
    bool connected{false};
    double balance{100000.0};
    
public:
    bool connect() override {
        std::cout << "Connected to Mock Exchange" << std::endl;
        connected = true;
        return true;
    }
    
    bool disconnect() override {
        connected = false;
        return true;
    }
    
    bool is_connected() const override {
        return connected;
    }
    
    bool buy(const std::string& symbol, double quantity, double price = 0.0) override {
        std::cout << "[MOCK] BUY " << quantity << " " << symbol 
                  << " @ " << (price > 0 ? std::to_string(price) : "MARKET") << std::endl;
        return true;
    }
    
    bool sell(const std::string& symbol, double quantity, double price = 0.0) override {
        std::cout << "[MOCK] SELL " << quantity << " " << symbol 
                  << " @ " << (price > 0 ? std::to_string(price) : "MARKET") << std::endl;
        return true;
    }
    
    bool cancel_order(const std::string& order_id) override {
        std::cout << "[MOCK] Cancel order: " << order_id << std::endl;
        return true;
    }
    
    double get_balance() const override {
        return balance;
    }
    
    std::vector<Position> get_positions() const override {
        return {};
    }
    
    void poll(std::function<void(const Tick&)> callback) override {
        // Generate mock ticks
        static int tick_count = 0;
        if (tick_count++ % 50 == 0) {
            Tick tick;
            tick.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            tick.symbol = "AAPL";
            tick.price = 150.0 + (std::rand() % 100 - 50) * 0.01;
            tick.volume = 100.0;
            tick.bid = 149.99;
            tick.ask = 150.01;
            tick.bid_size = 100.0;
            tick.ask_size = 100.0;
            callback(tick);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};