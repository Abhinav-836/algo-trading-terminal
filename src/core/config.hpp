#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class Config {
private:
    std::unordered_map<std::string, std::string> settings;
    
public:
    Config() = default;
    
    void load_from_file(const std::string& filename) {
        // Simple config parser
        // In production, use YAML or JSON library
        settings["mode"] = "paper";
        settings["exchange"] = "alpaca";
        settings["initial_capital"] = "100000.0";
    }
    
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
    
    bool get_bool(const std::string& key, bool default_val = false) const {
        std::string val = get(key);
        if (val == "true" || val == "1") return true;
        if (val == "false" || val == "0") return false;
        return default_val;
    }
};