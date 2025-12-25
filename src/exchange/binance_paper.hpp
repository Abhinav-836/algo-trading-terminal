#pragma once
#include "adapter.hpp"
#include <string>
#include <atomic>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <openssl/hmac.h>
#include <curl/curl.h>

class BinancePaperAdapter : public ExchangeAdapter {
private:
    std::string api_key;
    std::string secret_key;
    std::string base_url{"https://testnet.binance.vision"};
    std::atomic<bool> connected{false};
    CURL* curl{nullptr};
    
    double balance{10000.0};  // Paper trading balance in USDT
    
    // For signature generation
    std::string generate_signature(const std::string& query_string) {
        unsigned char* digest = HMAC(EVP_sha256(), 
                                     secret_key.c_str(), secret_key.length(),
                                     (unsigned char*)query_string.c_str(), query_string.length(),
                                     NULL, NULL);
        
        std::stringstream ss;
        for(int i = 0; i < 32; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
        }
        return ss.str();
    }
    
    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        return std::to_string(ms);
    }
    
public:
    BinancePaperAdapter(const std::string& key, const std::string& secret)
        : api_key(key), secret_key(secret) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
    }
    
    ~BinancePaperAdapter() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }
    
    bool connect() override {
        std::cout << "Connecting to Binance Testnet..." << std::endl;
        
        if (api_key.empty() || secret_key.empty()) {
            std::cerr << "Binance API keys not set" << std::endl;
            return false;
        }
        
        // Test connection by getting account info
        std::string endpoint = "/api/v3/account";
        std::string timestamp = get_timestamp();
        std::string query_string = "timestamp=" + timestamp;
        std::string signature = generate_signature(query_string);
        
        std::string url = base_url + endpoint + "?" + query_string + "&signature=" + signature;
        
        // In real implementation, would make HTTP request
        // For paper trading, just simulate success
        connected = true;
        std::cout << "Connected to Binance Testnet" << std::endl;
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
        
        std::string side = "BUY";
        std::string type = price > 0 ? "LIMIT" : "MARKET";
        
        // For paper trading, simulate order execution
        double executed_price = price > 0 ? price : get_current_price(symbol);
        double cost = executed_price * quantity;
        
        if (balance < cost) {
            std::cerr << "Insufficient balance: " << balance << " < " << cost << std::endl;
            return false;
        }
        
        balance -= cost;
        
        std::cout << "[BINANCE PAPER] BUY " << quantity << " " << symbol 
                  << " @ " << executed_price 
                  << " | Balance: " << balance << " USDT" << std::endl;
        
        // In real implementation, would send order to Binance API
        return true;
    }
    
    bool sell(const std::string& symbol, double quantity, double price = 0.0) override {
        if (!connected) return false;
        
        std::string side = "SELL";
        std::string type = price > 0 ? "LIMIT" : "MARKET";
        
        // For paper trading, simulate order execution
        double executed_price = price > 0 ? price : get_current_price(symbol);
        double revenue = executed_price * quantity;
        
        balance += revenue;
        
        std::cout << "[BINANCE PAPER] SELL " << quantity << " " << symbol 
                  << " @ " << executed_price 
                  << " | Balance: " << balance << " USDT" << std::endl;
        
        return true;
    }
    
    bool cancel_order(const std::string& order_id) override {
        std::cout << "[BINANCE PAPER] Cancel order: " << order_id << std::endl;
        return true;
    }
    
    double get_balance() const override {
        return balance;
    }
    
    std::vector<Position> get_positions() const override {
        // Binance doesn't have positions in spot trading, only balances
        return {};
    }
    
    void poll(std::function<void(const Tick&)> callback) override {
        if (!connected) return;
        
        // Generate simulated ticks for crypto pairs
        static int counter = 0;
        if (++counter % 50 == 0) {
            std::vector<std::string> symbols = {"BTCUSDT", "ETHUSDT", "BNBUSDT"};
            std::string symbol = symbols[std::rand() % symbols.size()];
            
            Tick tick{
                .timestamp = std::chrono::system_clock::now().time_since_epoch().count(),
                .symbol = symbol,
                .price = get_current_price(symbol),
                .volume = 1.0 + std::rand() % 10,
                .bid = get_current_price(symbol) * 0.999,
                .ask = get_current_price(symbol) * 1.001,
                .bid_size = 1.0,
                .ask_size = 1.0
            };
            callback(tick);
        }
    }
    
private:
    double get_current_price(const std::string& symbol) {
        // Mock prices for common crypto pairs
        static std::unordered_map<std::string, double> prices = {
            {"BTCUSDT", 45000.0},
            {"ETHUSDT", 3000.0},
            {"BNBUSDT", 350.0},
            {"ADAUSDT", 0.5},
            {"SOLUSDT", 100.0}
        };
        
        auto it = prices.find(symbol);
        if (it != prices.end()) {
            // Add some random variation
            return it->second * (1.0 + (std::rand() % 200 - 100) * 0.0001);
        }
        
        return 100.0; // Default price
    }
};