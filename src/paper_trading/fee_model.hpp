#pragma once

class FeeModel {
private:
    double maker_fee{0.0000};  // 0% for makers
    double taker_fee{0.0001};  // 0.01% for takers
    double min_fee{0.01};      // $0.01 minimum
    
public:
    FeeModel() = default;
    
    FeeModel(double maker, double taker, double min = 0.01)
        : maker_fee(maker), taker_fee(taker), min_fee(min) {}
    
    double calculate_fee(double notional, bool is_maker = false) {
        double rate = is_maker ? maker_fee : taker_fee;
        double fee = notional * rate;
        
        // Apply minimum fee
        if (fee < min_fee) {
            fee = min_fee;
        }
        
        return fee;
    }
    
    double get_maker_fee() const { return maker_fee; }
    double get_taker_fee() const { return taker_fee; }
    double get_min_fee() const { return min_fee; }
    
    void set_maker_fee(double fee) { maker_fee = fee; }
    void set_taker_fee(double fee) { taker_fee = fee; }
    void set_min_fee(double fee) { min_fee = fee; }
    
    // Common exchange fee schedules
    static FeeModel alpaca() {
        return FeeModel(0.0000, 0.0000);  // Free for paper trading
    }
    
    static FeeModel binance() {
        return FeeModel(0.0001, 0.0001, 0.10);  // 0.01% with $0.10 min
    }
    
    static FeeModel interactive_brokers() {
        return FeeModel(0.0005, 0.0035, 1.00);  // Variable with $1 min
    }
};