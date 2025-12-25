#pragma once
#include <random>
#include <chrono>

class SlippageModel {
private:
    std::default_random_engine generator;
    std::normal_distribution<double> distribution;
    
    double mean_slippage{0.0001};  // 0.01% mean slippage
    double stddev_slippage{0.00005}; // 0.005% standard deviation
    
public:
    SlippageModel() : generator(std::chrono::system_clock::now().time_since_epoch().count()),
                      distribution(mean_slippage, stddev_slippage) {}
    
    SlippageModel(double mean, double stddev) 
        : mean_slippage(mean), stddev_slippage(stddev),
          generator(std::chrono::system_clock::now().time_since_epoch().count()),
          distribution(mean, stddev) {}
    
    double apply_slippage(double price, double quantity, bool is_buy) {
        // Slippage increases with order size
        double size_factor = std::min(1.0, quantity / 1000.0);  // Normalize
        
        // Random slippage component
        double slippage = distribution(generator) * size_factor;
        
        // Apply slippage: worse price for the trader
        return is_buy ? price * (1 + slippage) : price * (1 - slippage);
    }
    
    double get_effective_spread(double bid, double ask, double quantity, bool is_buy) {
        double mid = (bid + ask) / 2.0;
        double quoted_price = is_buy ? ask : bid;
        
        double executed_price = apply_slippage(quoted_price, quantity, is_buy);
        double effective_spread = is_buy ? executed_price - mid : mid - executed_price;
        
        return effective_spread;
    }
    
    void set_parameters(double mean, double stddev) {
        mean_slippage = mean;
        stddev_slippage = stddev;
        distribution = std::normal_distribution<double>(mean, stddev);
    }
};