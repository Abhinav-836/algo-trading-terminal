#pragma once
#include "../core/types.hpp"
#include <functional>
#include <memory>
#include <string>

class ExchangeAdapter {
public:
    virtual ~ExchangeAdapter() = default;
    
    // Connection
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Orders
    virtual std::string place_order(const Order& order) = 0;
    virtual bool cancel_order(const std::string& order_id) = 0;
    virtual std::vector<Order> get_open_orders() = 0;
    
    // Account & Positions
    virtual double get_balance() const = 0;
    virtual std::vector<Position> get_positions() = 0;
    virtual AccountInfo get_account_info() = 0;
    
    // Market Data
    virtual Tick get_current_tick(const std::string& symbol) = 0;
    
    // Real-time data stream
    virtual void subscribe_symbol(const std::string& symbol) = 0;
    virtual void set_tick_callback(std::function<void(const Tick&)> callback) = 0;
    
    // Main event loop
    virtual void run() = 0;
    virtual void stop() = 0;
};