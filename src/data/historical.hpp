#pragma once
#include "../core/types.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>

class Tick {

public:

    uint64_t timestamp;

    std::string symbol;

    double price;

    double volume;

    double bid;

    double ask;



    Tick(uint64_t ts, const std::string& sym, double pr, double vol, double b, double a)

        : timestamp(ts), symbol(sym), price(pr), volume(vol), bid(b), ask(a) {}

};

class HistoricalData {
private:
    std::string data_dir{"data/"};
    
public:
    HistoricalData() {
        // Create data directory if it doesn't exist
        system("mkdir -p data");
    }
    
    bool save_tick(const Tick& tick, const std::string& filename = "ticks.csv") {
        std::ofstream file(data_dir + filename, std::ios::app);
        if (!file.is_open()) {
            return false;
        }
        
        file << tick.timestamp << ","
             << tick.symbol << ","
             << tick.price << ","
             << tick.volume << ","
             << tick.bid << ","
             << tick.ask << "\n";
        
        file.close();
        return true;
    }
    
    bool save_trade(const Order& order, double fill_price, const std::string& filename = "trades.csv") {
        std::ofstream file(data_dir + filename, std::ios::app);
        if (!file.is_open()) {
            return false;
        }
        
        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        file << timestamp << ","
             << order.symbol << ","
             << (order.is_buy ? "BUY" : "SELL") << ","
             << order.quantity << ","
             << fill_price << ","
             << order.price << "\n";
        
        file.close();
        return true;
    }
    
    std::vector<Tick> load_ticks(const std::string& symbol, const std::string& filename = "ticks.csv") {
        std::vector<Tick> ticks;
        std::ifstream file(data_dir + filename);
        
        if (!file.is_open()) {
            return ticks;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (std::getline(ss, token, ',')) {
                tokens.push_back(token);
            }
            
            if (tokens.size() >= 6 && tokens[1] == symbol) {
                Tick tick(
                    std::stoull(tokens[0]),
                    tokens[1],
                    std::stod(tokens[2]),
                    std::stod(tokens[3]),
                    std::stod(tokens[4]),
                    std::stod(tokens[5])
                );
                ticks.push_back(tick);
            }
        }
        
        file.close();
        return ticks;
    }
    
    std::vector<std::string> get_available_symbols(const std::string& filename = "ticks.csv") {
        std::vector<std::string> symbols;
        std::ifstream file(data_dir + filename);
        
        if (!file.is_open()) {
            return symbols;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ','); // Skip timestamp
            std::getline(ss, token, ','); // Get symbol
            
            if (std::find(symbols.begin(), symbols.end(), token) == symbols.end()) {
                symbols.push_back(token);
            }
        }
        
        file.close();
        return symbols;
    }
    
    void set_data_dir(const std::string& dir) {
        data_dir = dir;
        if (!data_dir.empty() && data_dir.back() != '/') {
            data_dir += '/';
        }
        system(("mkdir -p " + data_dir).c_str());
    }
};