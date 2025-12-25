#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Tick {
    uint64_t timestamp;
    std::string symbol;
    double price;
    double volume;
    double bid;
    double ask;
    double bid_size;
    double ask_size;
    
    double mid_price() const { return (bid + ask) / 2.0; }
    double spread() const { return ask - bid; }
};

struct Order {
    uint64_t id;
    std::string symbol;
    double price;
    double quantity;
    bool is_buy;
    std::string type;  // "market", "limit"
    std::string tif;   // "day", "gtc", "ioc"
    
    bool is_market() const { return type == "market"; }
    bool is_limit() const { return type == "limit"; }
};

struct Signal {
    std::string symbol;
    std::vector<Order> orders;
    bool valid{false};
};

struct Position {
    std::string symbol;
    double quantity;
    double avg_price;
    double current_price;
    
    double market_value() const { return quantity * current_price; }
    double pnl() const { return (current_price - avg_price) * quantity; }
};