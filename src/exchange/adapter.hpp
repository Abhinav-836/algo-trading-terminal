#pragma once
#include "../core/types.hpp"
#include <functional>
#include <string>

class ExchangeAdapter {
public:
    virtual ~ExchangeAdapter() = default;
    
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    virtual bool buy(const std::string& symbol, double quantity, double price = 0.0) = 0;
    virtual bool sell(const std::string& symbol, double quantity, double price = 0.0) = 0;
    virtual bool cancel_order(const std::string& order_id) = 0;
    
    virtual double get_balance() const = 0;
    virtual std::vector<Position> get_positions() const = 0;
    
    virtual void poll(std::function<void(const Tick&)> callback) = 0;
    
    virtual bool execute(const Signal& signal) {
        for (const auto& order : signal.orders) {
            if (order.is_buy) {
                if (!buy(order.symbol, order.quantity, order.price)) {
                    return false;
                }
            } else {
                if (!sell(order.symbol, order.quantity, order.price)) {
                    return false;
                }
            }
        }
        return true;
    }
};