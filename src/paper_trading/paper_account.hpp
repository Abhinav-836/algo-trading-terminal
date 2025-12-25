#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

class PaperAccount {
private:
    struct AccountInfo {
        double cash{100000.0};
        double buying_power{300000.0};  // 3x leverage
        double day_trading_bp{100000.0};
        bool pattern_day_trader{false};
    };
    
    AccountInfo account;
    std::unordered_map<std::string, double> balances;  // Per-symbol balance
    mutable std::mutex mutex;
    
public:
    PaperAccount(double initial_cash = 100000.0) {
        account.cash = initial_cash;
        account.buying_power = initial_cash * 3.0;
        account.day_trading_bp = initial_cash;
    }
    
    double get_cash() const {
        std::lock_guard lock(mutex);
        return account.cash;
    }
    
    double get_buying_power() const {
        std::lock_guard lock(mutex);
        return account.buying_power;
    }
    
    double get_equity() const {
        std::lock_guard lock(mutex);
        return account.cash;  // Simplified
    }
    
    bool can_buy(double amount) const {
        std::lock_guard lock(mutex);
        return amount <= account.buying_power;
    }
    
    bool can_sell(const std::string& symbol, double quantity) const {
        std::lock_guard lock(mutex);
        
        auto it = balances.find(symbol);
        if (it == balances.end()) {
            return false;  // Don't own this symbol
        }
        
        return quantity <= it->second;
    }
    
    void deduct_cash(double amount) {
        std::lock_guard lock(mutex);
        account.cash -= amount;
        update_buying_power();
    }
    
    void add_cash(double amount) {
        std::lock_guard lock(mutex);
        account.cash += amount;
        update_buying_power();
    }
    
    void update_position(const std::string& symbol, double quantity) {
        std::lock_guard lock(mutex);
        
        balances[symbol] += quantity;
        if (balances[symbol] == 0) {
            balances.erase(symbol);
        }
    }
    
    double get_position(const std::string& symbol) const {
        std::lock_guard lock(mutex);
        
        auto it = balances.find(symbol);
        return it != balances.end() ? it->second : 0.0;
    }
    
    std::unordered_map<std::string, double> get_all_positions() const {
        std::lock_guard lock(mutex);
        return balances;
    }
    
    void reset(double new_cash = 100000.0) {
        std::lock_guard lock(mutex);
        
        account.cash = new_cash;
        account.buying_power = new_cash * 3.0;
        account.day_trading_bp = new_cash;
        balances.clear();
    }
    
private:
    void update_buying_power() {
        // Simple buying power calculation
        account.buying_power = account.cash * 3.0;
        account.day_trading_bp = account.cash;
    }
};