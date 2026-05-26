#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

class Config {
private:
    std::unordered_map<std::string, std::string> settings;
    std::vector<std::string> trading_symbols;
    std::vector<std::string> active_strategies;
    
public:
    Config() = default;
    
    bool load_from_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open " << filename << ", using defaults" << std::endl;
            load_defaults();
            return false;
        }
        
        std::string line;
        std::string current_section;
        
        while (std::getline(file, line)) {
            // Remove whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (line.empty() || line[0] == '#') continue;
            
            // Check for section headers
            if (line[0] == '[' && line.back() == ']') {
                current_section = line.substr(1, line.length() - 2);
                continue;
            }
            
            // Parse key-value pairs
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Remove quotes if present
                if (value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
                
                // Store with section prefix
                std::string full_key = current_section.empty() ? key : current_section + "." + key;
                settings[full_key] = value;
            }
            
            // Parse list for symbols
            if (current_section == "trading" && line.find("symbols:") != std::string::npos) {
                size_t bracket_start = line.find('[');
                size_t bracket_end = line.find(']');
                if (bracket_start != std::string::npos && bracket_end != std::string::npos) {
                    std::string list_str = line.substr(bracket_start + 1, bracket_end - bracket_start - 1);
                    std::stringstream ss(list_str);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        item.erase(0, item.find_first_not_of(" \t"));
                        item.erase(item.find_last_not_of(" \t") + 1);
                        if (!item.empty()) {
                            trading_symbols.push_back(item);
                        }
                    }
                }
            }
            
            // Parse list for strategies
            if (current_section == "strategies" && line.find("active:") != std::string::npos) {
                size_t bracket_start = line.find('[');
                size_t bracket_end = line.find(']');
                if (bracket_start != std::string::npos && bracket_end != std::string::npos) {
                    std::string list_str = line.substr(bracket_start + 1, bracket_end - bracket_start - 1);
                    std::stringstream ss(list_str);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        item.erase(0, item.find_first_not_of(" \t"));
                        item.erase(item.find_last_not_of(" \t") + 1);
                        if (!item.empty()) {
                            active_strategies.push_back(item);
                        }
                    }
                }
            }
        }
        
        file.close();
        
        // Set defaults for any missing values
        if (settings.find("trading.mode") == settings.end()) settings["trading.mode"] = "paper";
        if (settings.find("trading.exchange") == settings.end()) settings["trading.exchange"] = "alpaca";
        if (settings.find("trading.initial_capital") == settings.end()) settings["trading.initial_capital"] = "100000";
        if (trading_symbols.empty()) trading_symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
        if (active_strategies.empty()) active_strategies = {"momentum"};
        
        return true;
    }
    
    void load_defaults() {
        settings["trading.mode"] = "paper";
        settings["trading.exchange"] = "alpaca";
        settings["trading.initial_capital"] = "100000";
        trading_symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
        active_strategies = {"momentum"};
    }
    
    // Getters
    std::string get(const std::string& key, const std::string& default_val = "") const {
        auto it = settings.find(key);
        return it != settings.end() ? it->second : default_val;
    }
    
    double get_double(const std::string& key, double default_val = 0.0) const {
        try {
            return std::stod(get(key, std::to_string(default_val)));
        } catch (...) {
            return default_val;
        }
    }
    
    int get_int(const std::string& key, int default_val = 0) const {
        try {
            return std::stoi(get(key, std::to_string(default_val)));
        } catch (...) {
            return default_val;
        }
    }
    
    bool get_bool(const std::string& key, bool default_val = false) const {
        std::string val = get(key);
        if (val == "true" || val == "1" || val == "yes" || val == "on") return true;
        if (val == "false" || val == "0" || val == "no" || val == "off") return false;
        return default_val;
    }
    
    std::vector<std::string> get_symbols() const {
        return trading_symbols;
    }
    
    std::vector<std::string> get_active_strategies() const {
        return active_strategies;
    }
    
    // Strategy parameters
    double get_strategy_param(const std::string& strategy, const std::string& param, double default_val) const {
        std::string key = "strategies." + strategy + "." + param;
        return get_double(key, default_val);
    }
    
    int get_strategy_param_int(const std::string& strategy, const std::string& param, int default_val) const {
        std::string key = "strategies." + strategy + "." + param;
        return get_int(key, default_val);
    }
    
    bool is_strategy_enabled(const std::string& strategy) const {
        auto it = std::find(active_strategies.begin(), active_strategies.end(), strategy);
        return it != active_strategies.end();
    }
    
    void print_config() const {
        std::cout << "\n📋 Current Configuration:\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "  Mode:           " << get("trading.mode") << "\n";
        std::cout << "  Exchange:       " << get("trading.exchange") << "\n";
        std::cout << "  Initial Capital: $" << get_double("trading.initial_capital", 100000) << "\n";
        
        std::cout << "  Trading Symbols: ";
        for (size_t i = 0; i < trading_symbols.size(); i++) {
            std::cout << trading_symbols[i];
            if (i < trading_symbols.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
        
        std::cout << "  Active Strategies: ";
        for (size_t i = 0; i < active_strategies.size(); i++) {
            std::cout << active_strategies[i];
            if (i < active_strategies.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
        
        std::cout << "═══════════════════════════════════════════════════════════\n\n";
    }
};