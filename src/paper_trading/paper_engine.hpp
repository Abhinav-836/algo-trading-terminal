#pragma once
#include "../core/types.hpp"
#include <unordered_map>
#include <string>
#include <iostream>
#include <mutex>
#include <atomic>
#include <chrono>

class PaperEngine {
private:
    struct PaperOrder {
        Order order;
        double filled{0.0};
        std::string status{"pending"};
        uint64_t timestamp;
    };
    
    std::unordered_map<uint64_t, PaperOrder> orders;
    std::unordered_map<std::string, double> positions;
    std::atomic<double> balance{100000.0};
    std::atomic<uint64_t> next_order_id{1};
    mutable std::mutex mutex;
    
    double slippage_rate{0.0001};   // 0.01% slippage
    double commission_rate{0.001};  // 0.1% commission
    
public:
    PaperEngine(double initial_balance = 100000.0) : balance(initial_balance) {}
    
    uint64_t submit_order(const Order& order) {
        std::lock_guard<std::mutex> lock(mutex);
        
        uint64_t order_id = next_order_id++;
        
        PaperOrder paper_order;
        paper_order.order = order;
        paper_order.filled = 0.0;
        paper_order.status = "pending";
        paper_order.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        orders[order_id] = paper_order;
        
        // Process immediately for market orders
        if (order.is_market()) {
            process_market_order(order_id);
        }
        
        return order_id;
    }
    
    bool cancel_order(uint64_t order_id) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = orders.find(order_id);
        if (it == orders.end()) {
            return false;
        }
        
        it->second.status = "cancelled";
        return true;
    }
    
    double get_balance() const {
        return balance.load();
    }
    
    double get_position(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = positions.find(symbol);
        return it != positions.end() ? it->second : 0.0;
    }
    
    std::unordered_map<std::string, double> get_all_positions() const {
        std::lock_guard<std::mutex> lock(mutex);
        return positions;
    }
    
    double get_portfolio_value(double current_price = 150.0) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        double total = balance.load();
        for (const auto& pair : positions) {
            total += pair.second * current_price;
        }
        
        return total;
    }
    
    void set_slippage(double rate) {
        slippage_rate = rate;
    }
    
    void set_commission(double rate) {
        commission_rate = rate;
    }
    
    void reset(double new_balance = 100000.0) {
        std::lock_guard<std::mutex> lock(mutex);
        balance = new_balance;
        positions.clear();
        orders.clear();
        next_order_id = 1;
    }
    
private:
    void process_market_order(uint64_t order_id) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = orders.find(order_id);
        if (it == orders.end()) {
            return;
        }
        
        PaperOrder& paper_order = it->second;
        const Order& order = paper_order.order;
        
        // Apply slippage
        double fill_price = order.price;
        if (fill_price == 0.0) {  // Market order with no limit price
            fill_price = 150.0;   // Example price
        }
        
        double slippage = fill_price * slippage_rate;
        fill_price += order.is_buy ? slippage : -slippage;
        
        // Calculate commission
        double commission = fill_price * order.quantity * commission_rate;
        
        // Check balance for buy orders
        if (order.is_buy) {
            double cost = fill_price * order.quantity + commission;
            if (balance < cost) {
                paper_order.status = "rejected";
                std::cout << "[PAPER] BUY REJECTED: Insufficient balance" << std::endl;
                return;
            }
            balance -= cost;  // FIXED: was 'balance - = cost'
            positions[order.symbol] += order.quantity;
        } else {
            // Check position for sell orders
            if (positions[order.symbol] < order.quantity) {
                paper_order.status = "rejected";
                std::cout << "[PAPER] SELL REJECTED: Insufficient position" << std::endl;
                return;
            }
            double revenue = fill_price * order.quantity - commission;
            balance += revenue;  // FIXED: was 'balance + = revenue'
            positions[order.symbol] -= order.quantity;
            
            // Clean up zero positions
            if (positions[order.symbol] <= 0) {
                positions.erase(order.symbol);
            }
        }
        
        paper_order.filled = order.quantity;
        paper_order.status = "filled";
        
        std::cout << "[PAPER] " << (order.is_buy ? "BUY" : "SELL") 
                  << " " << order.quantity << " " << order.symbol
                  << " @ " << fill_price 
                  << " | Commission: $" << commission
                  << " | Balance: $" << balance.load() << std::endl;
    }
};