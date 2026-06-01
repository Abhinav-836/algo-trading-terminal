#pragma once
#include "../core/types.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <filesystem>   // C++17

// FIX: The original code used system("mkdir ...") to create directories.
// system() is a shell injection risk — if data_dir ever contains user-supplied
// input, an attacker could append shell commands (e.g. "; rm -rf /").
// Replaced with std::filesystem::create_directories() which is safe, portable,
// and does not spawn a shell process.

class HistoricalData {
private:
    std::filesystem::path data_dir{"data/"};

    void ensure_dir() const {
        std::error_code ec;
        std::filesystem::create_directories(data_dir, ec);
        // Ignore ec: if it already exists that's fine; other errors surface
        // when we try to open files below.
    }

public:
    HistoricalData() { ensure_dir(); }

    bool save_tick(const Tick& tick, const std::string& filename = "ticks.csv") {
        ensure_dir();
        std::ofstream file(data_dir / filename, std::ios::app);
        if (!file.is_open()) return false;

        file << tick.timestamp  << ","
             << tick.symbol     << ","
             << tick.price      << ","
             << tick.volume     << ","
             << tick.bid        << ","
             << tick.ask        << "\n";

        return true;
    }

    bool save_trade(const Order& order, double fill_price,
                    const std::string& filename = "trades.csv") {
        ensure_dir();
        std::ofstream file(data_dir / filename, std::ios::app);
        if (!file.is_open()) return false;

        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        file << timestamp             << ","
             << order.symbol          << ","
             << (order.is_buy ? "BUY" : "SELL") << ","
             << order.quantity        << ","
             << fill_price            << ","
             << order.price           << "\n";

        return true;
    }

    std::vector<Tick> load_ticks(const std::string& symbol,
                                  const std::string& filename = "ticks.csv") {
        std::vector<Tick> ticks;
        std::ifstream file(data_dir / filename);
        if (!file.is_open()) return ticks;

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(ss, token, ','))
                tokens.push_back(token);

            if (tokens.size() >= 6 && tokens[1] == symbol) {
                try {
                    ticks.emplace_back(
                        std::stoull(tokens[0]),
                        tokens[1],
                        std::stod(tokens[2]),
                        std::stod(tokens[3]),
                        std::stod(tokens[4]),
                        std::stod(tokens[5])
                    );
                } catch (...) {
                    // Skip malformed rows
                }
            }
        }

        return ticks;
    }

    std::vector<std::string> get_available_symbols(
        const std::string& filename = "ticks.csv") {
        std::vector<std::string> symbols;
        std::ifstream file(data_dir / filename);
        if (!file.is_open()) return symbols;

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ',');  // skip timestamp
            std::getline(ss, token, ',');  // symbol

            if (!token.empty() &&
                std::find(symbols.begin(), symbols.end(), token) == symbols.end()) {
                symbols.push_back(token);
            }
        }

        return symbols;
    }

    void set_data_dir(const std::string& dir) {
        data_dir = dir;
        if (!data_dir.empty() &&
            data_dir.native().back() != std::filesystem::path::preferred_separator) {
            data_dir /= "";  // normalise trailing separator
        }
        ensure_dir();
    }
};