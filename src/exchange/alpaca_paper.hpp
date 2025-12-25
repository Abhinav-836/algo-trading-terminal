#pragma once
#include "adapter.hpp"
#include <string>
#include <atomic>

class AlpacaPaperAdapter : public ExchangeAdapter {
private:
    std::string api_key;
    std::string secret_key;
    std::string base_url;
    std::atomic<bool> connected{false};
    double balance{100000.0};  // Paper trading balance
    
public:
    AlpacaPaperAdapter(const std::string& key, const std::string& secret)
        : api_key(key), secret_key(secret), 
          base_url("https://paper-api.alpaca.markets") {}
    
    bool connect() override {
        // Simulate connection to Alpaca paper trading
        std::cout << "Connecting to Alpaca Paper Trading..." << std::endl;
        
        // In real implementation, would validate API keys
        if (api_key.empty() || secret_key.empty()) {
            std::cerr << "API keys not set" << std::endl;
            return false;
        }
        
        connected = true;
        std::cout << "Connected to Alpaca Paper Trading" << std::endl;
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
        if (!connected) return false;
        
        double cost = price > 0 ? price * quantity : 150.0 * quantity;  // Example price
        if (balance < cost) {
            std::cerr << "Insufficient balance: " << balance << " < " << cost << std::endl;
            return false;
        }
        
        balance -= cost;
        std::cout << "BUY: " << quantity << " " << symbol << " @ " 
                  << (price > 0 ? std::to_string(price) : "MARKET") 
                  << " | Balance: " << balance << std::endl;
        
        return true;
    }
    
    bool sell(const std::string& symbol, double quantity, double price = 0.0) override {
        if (!connected) return false;
        
        double revenue = price > 0 ? price * quantity : 150.0 * quantity;
        balance += revenue;
        
        std::cout << "SELL: " << quantity << " " << symbol << " @ " 
                  << (price > 0 ? std::to_string(price) : "MARKET") 
                  << " | Balance: " << balance << std::endl;
        
        return true;
    }
    
    bool cancel_order(const std::string& order_id) override {
        std::cout << "Cancel order: " << order_id << std::endl;
        return true;
    }
    
    double get_balance() const override {
        return balance;
    }
    
    std::vector<Position> get_positions() const override {
        // Return empty positions for now
        return {};
    }
    
    void poll(std::function<void(const Tick&)> callback) override {
        if (!connected) return;
        
        // Generate simulated ticks
        static int counter = 0;
        if (++counter % 100 == 0) {
            Tick tick{
                std::chrono::system_clock::now().time_since_epoch().count(),
                "AAPL",
                150.0 + (std::rand() % 100 - 50) * 0.01,
                100.0,
                149.99,
                150.01,
                100.0,
                100.0
            };
            callback(tick);
        }
    }
};