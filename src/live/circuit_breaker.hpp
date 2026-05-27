#pragma once
#include <chrono>
#include <atomic>

class CircuitBreaker {
private:
    double daily_loss_limit{0.05};      // 5% daily max loss
    double daily_pnl{0.0};
    double initial_capital{100000.0};
    int trade_count{0};
    int max_daily_trades{50};
    std::chrono::system_clock::time_point last_reset;
    std::atomic<bool> is_tripped{false};
    
public:
    CircuitBreaker(double capital = 100000.0) : initial_capital(capital) {
        last_reset = std::chrono::system_clock::now();
    }
    
    void check_and_reset_daily() {
        auto now = std::chrono::system_clock::now();
        auto last = std::chrono::system_clock::to_time_t(last_reset);
        auto now_t = std::chrono::system_clock::to_time_t(now);
        
        if (std::difftime(now_t, last) >= 86400) {  // New day
            daily_pnl = 0.0;
            trade_count = 0;
            is_tripped = false;
            last_reset = now;
        }
    }
    
    bool can_trade() {
        check_and_reset_daily();
        
        if (is_tripped) return false;
        
        double loss_percent = -daily_pnl / initial_capital;
        
        if (loss_percent >= daily_loss_limit) {
            is_tripped = true;
            std::cout << "\n🔴 CIRCUIT BREAKER TRIPPED! Daily loss limit reached.\n";
            return false;
        }
        
        if (trade_count >= max_daily_trades) {
            std::cout << "\n🟡 Daily trade limit reached.\n";
            return false;
        }
        
        return true;
    }
    
    void record_trade(double pnl) {
        daily_pnl += pnl;
        trade_count++;
    }
    
    void reset() {
        daily_pnl = 0.0;
        trade_count = 0;
        is_tripped = false;
    }
    
    double get_daily_pnl() const { return daily_pnl; }
    int get_trade_count() const { return trade_count; }
    bool is_tripped_flag() const { return is_tripped; }
};