#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

struct Tick {
    uint64_t timestamp{0};
    std::string symbol;
    double price{0.0};
    double volume{0.0};
    double bid{0.0};
    double ask{0.0};
    double bid_size{0.0};
    double ask_size{0.0};
    
    Tick() = default;
    
    Tick(uint64_t ts, const std::string& sym, double pr, double vol, 
         double b, double a, double bs = 0, double as = 0)
        : timestamp(ts), symbol(sym), price(pr), volume(vol), 
          bid(b), ask(a), bid_size(bs), ask_size(as) {}
    
    double mid_price() const { return (bid + ask) / 2.0; }
    double spread() const { return ask - bid; }
    double spread_bps() const { return (ask - bid) / mid_price() * 10000; }
    
    std::string time_str() const {
        time_t t = static_cast<time_t>(timestamp / 1000000000);
        struct tm tm_result;
        // FIX #10: Use thread-safe localtime_r (POSIX) / localtime_s (MSVC)
        // instead of std::localtime() which shares a static internal buffer.
#ifdef _WIN32
        localtime_s(&tm_result, &t);
#else
        localtime_r(&t, &tm_result);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm_result, "%H:%M:%S");
        return ss.str();
    }
};

struct Order {
    uint64_t id{0};
    std::string client_order_id;
    std::string symbol;
    double price{0.0};
    double quantity{0.0};
    double filled_quantity{0.0};
    bool is_buy{true};
    std::string type{"market"};
    std::string tif{"day"};
    std::string status{"new"};
    std::string alpaca_id;
    
    bool is_market() const { return type == "market"; }
    bool is_limit() const { return type == "limit"; }
    bool is_filled() const { return status == "filled"; }
    bool is_active() const { 
        return status == "new" || status == "accepted" || status == "partially_filled";
    }
};

struct Signal {
    std::string symbol;
    std::vector<Order> orders;
    double confidence{0.0};
    std::string reason;
    bool valid{false};
};

struct Position {
    std::string symbol;
    double quantity{0.0};
    double avg_entry_price{0.0};
    double current_price{0.0};
    double unrealized_pl{0.0};
    double realized_pl{0.0};
    double market_value{0.0};
    
    double pnl() const { return unrealized_pl + realized_pl; }
    double pnl_percent() const { 
        return avg_entry_price > 0 ? (current_price - avg_entry_price) / avg_entry_price * 100 : 0;
    }
};

struct Bar {
    std::string symbol;
    uint64_t timestamp;
    double open{0.0};
    double high{0.0};
    double low{0.0};
    double close{0.0};
    double volume{0.0};
    double vwap{0.0};
    int trade_count{0};
};

struct AccountInfo {
    std::string id;
    std::string status;
    double cash{0.0};
    double buying_power{0.0};
    double portfolio_value{0.0};
    double long_market_value{0.0};
    double short_market_value{0.0};
    double equity{0.0};
    double last_equity{0.0};
    double initial_margin{0.0};
    double maintenance_margin{0.0};
    double day_trade_count{0.0};
    bool pattern_day_trader{false};
};