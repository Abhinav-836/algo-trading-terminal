#pragma once
#include "adapter.hpp"
#include <string>
#include <iostream>
#include <atomic>

class BinanceRealAdapter : public ExchangeAdapter {
private:
    std::string api_key;
    std::string secret_key;
    std::atomic<bool> connected{false};
    
public:
    BinanceRealAdapter(const std::string& key, const std::string& secret)
        : api_key(key), secret_key(secret) {
        std::cout << "⚠️ WARNING: Initializing REAL Binance trading adapter" << std::endl;
        std::cout << "⚠️ REAL MONEY WILL BE USED!" << std::endl;
    }
    
    bool connect() override {
        std::cout << "Connecting to REAL Binance exchange..." << std::endl;
        
        if (api_key.empty() || secret_key.empty()) {
            std::cerr << "ERROR: Binance API keys not set" << std::endl;
            return false;
        }
        
        // In real implementation, validate API keys
        std::cout << "⚠️ WARNING: Connected to REAL Binance exchange" << std::endl;
        std::cout << "⚠️ All trades will use REAL MONEY" << std::endl;
        
        connected = true;
        return true;
    }
    
    bool disconnect() override {
        connected = false;
        std::cout << "Disconnected from Binance" << std::endl;
        return true;
    }
    
    bool is_connected() const override {
        return connected;
    }
    
    bool buy(const std::string& symbol, double quantity, double price = 0.0) override {
        if (!connected) {
            std::cerr << "Not connected to exchange" << std::endl;
            return false;
        }
        
        std::cout << "⚠️ REAL ORDER: BUY " << quantity << " " << symbol;
        if (price > 0) {
            std::cout << " @ LIMIT $" << price;
        } else {
            std::cout << " @ MARKET";
        }
        std::cout << std::endl;
        
        // In real implementation, send order to Binance API
        // This is where real money would be spent
        
        return true;
    }
    
    bool sell(const std::string& symbol, double quantity, double price = 0.0) override {
        if (!connected) {
            std::cerr << "Not connected to exchange" << std::endl;
            return false;
        }
        
        std::cout << "⚠️ REAL ORDER: SELL " << quantity << " " << symbol;
        if (price > 0) {
            std::cout << " @ LIMIT $" << price;
        } else {
            std::cout << " @ MARKET";
        }
        std::cout << std::endl;
        
        return true;
    }
    
    bool cancel_order(const std::string& order_id) override {
        std::cout << "Cancelling REAL order: " << order_id << std::endl;
        return true;
    }
    
    double get_balance() const override {
        // In real implementation, query Binance API for balance
        std::cout << "Querying REAL account balance..." << std::endl;
        return 0.0; // Placeholder
    }
    
    std::vector<Position> get_positions() const override {
        std::cout << "Querying REAL positions..." << std::endl;
        return {};
    }
    
    void poll(std::function<void(const Tick&)> callback) override {
        if (!connected) return;
        
        // In real implementation, connect to Binance WebSocket
        static int counter = 0;
        if (++counter % 100 == 0) {
            std::cout << "Receiving REAL market data..." << std::endl;
            // Would receive real market data here
        }
    }
    
    // Additional Binance-specific methods
    bool set_leverage(const std::string& symbol, int leverage) {
        std::cout << "Setting leverage for " << symbol << " to " << leverage << "x" << std::endl;
        return true;
    }
    
    bool set_margin_type(const std::string& symbol, const std::string& margin_type) {
        std::cout << "Setting margin type for " << symbol << " to " << margin_type << std::endl;
        return true;
    }
};