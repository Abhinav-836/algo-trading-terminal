#pragma once
#include "../core/types.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <map>

class Dashboard {
private:
    std::atomic<bool> running{false};
    std::thread display_thread;
    
    struct AccountInfo {
        double balance{0.0};
        double buying_power{0.0};
        double total_pnl{0.0};
        double daily_pnl{0.0};
        int active_positions{0};
        int today_trades{0};
        double win_rate{0.0};
    };
    
    AccountInfo account;
    std::vector<Position> positions;
    std::vector<std::string> recent_trades;
    mutable std::mutex mutex;
    
public:
    Dashboard() = default;
    
    ~Dashboard() {
        stop();
    }
    
    void start() {
        running = true;
        display_thread = std::thread(&Dashboard::render_loop, this);
    }
    
    void stop() {
        running = false;
        if (display_thread.joinable()) {
            display_thread.join();
        }
    }
    
    void update_account(double balance, double buying_power, double total_pnl, double daily_pnl) {
        std::lock_guard<std::mutex> lock(mutex);
        account.balance = balance;
        account.buying_power = buying_power;
        account.total_pnl = total_pnl;
        account.daily_pnl = daily_pnl;
    }
    
    void update_positions(const std::vector<Position>& pos) {
        std::lock_guard<std::mutex> lock(mutex);
        positions = pos;
        account.active_positions = static_cast<int>(pos.size());
    }
    
    void add_trade(const std::string& trade) {
        std::lock_guard<std::mutex> lock(mutex);
        recent_trades.insert(recent_trades.begin(), trade);
        if (recent_trades.size() > 10) {
            recent_trades.pop_back();
        }
        account.today_trades++;
    }
    
    void update_win_rate(double rate) {
        std::lock_guard<std::mutex> lock(mutex);
        account.win_rate = rate;
    }
    
private:
    void render_loop() {
        while (running) {
            render();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    void clear_screen() {
        std::cout << "\033[2J\033[1;1H";
    }
    
    void render() {
        std::lock_guard<std::mutex> lock(mutex);
        
        clear_screen();
        
        std::cout << "╔════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                       FAST TRADING TERMINAL                        ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════════╝\n\n";
        
        // Account Summary
        std::cout << "┌────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ ACCOUNT SUMMARY                                                    │\n";
        std::cout << "├────────────────────────────────────────────────────────────────────┤\n";
        std::cout << "│ Balance:      $" << std::setw(12) << std::fixed << std::setprecision(2) << account.balance << "                    │\n";
        std::cout << "│ Buying Power: $" << std::setw(12) << account.buying_power << "                    │\n";
        std::cout << "│ Total P&L:    $" << std::setw(12) << account.total_pnl << "   ";
        if (account.total_pnl >= 0) std::cout << "↑ UP     │\n";
        else std::cout << "↓ DOWN   │\n";
        std::cout << "│ Daily P&L:    $" << std::setw(12) << account.daily_pnl << "   ";
        if (account.daily_pnl >= 0) std::cout << "↑ UP     │\n";
        else std::cout << "↓ DOWN   │\n";
        std::cout << "├────────────────────────────────────────────────────────────────────┤\n";
        std::cout << "│ Active Pos:   " << std::setw(5) << account.active_positions << "        Trades: " << std::setw(5) << account.today_trades << "        │\n";
        std::cout << "│ Win Rate:     " << std::setw(5) << std::setprecision(1) << account.win_rate << "%                                      │\n";
        std::cout << "└────────────────────────────────────────────────────────────────────┘\n\n";
        
        // Positions
        if (!positions.empty()) {
            std::cout << "┌────────────────────────────────────────────────────────────────────┐\n";
            std::cout << "│ CURRENT POSITIONS                                                  │\n";
            std::cout << "├──────────┬────────────┬────────────┬────────────┬─────────────────┤\n";
            std::cout << "│ SYMBOL   │ QTY        │ AVG PRICE  │ CUR PRICE  │ P&L             │\n";
            std::cout << "├──────────┼────────────┼────────────┼────────────┼─────────────────┤\n";
            
            for (const auto& pos : positions) {
                double pnl = (pos.current_price - pos.avg_price) * pos.quantity;
                std::cout << "│ " << std::setw(8) << pos.symbol << " │ "
                          << std::setw(10) << std::fixed << std::setprecision(2) << pos.quantity << " │ "
                          << std::setw(10) << pos.avg_price << " │ "
                          << std::setw(10) << pos.current_price << " │ "
                          << (pnl >= 0 ? "+" : "") << std::setw(14) << pnl << " │\n";
            }
            std::cout << "└──────────┴────────────┴────────────┴────────────┴─────────────────┘\n\n";
        }
        
        // Recent Trades
        if (!recent_trades.empty()) {
            std::cout << "┌────────────────────────────────────────────────────────────────────┐\n";
            std::cout << "│ RECENT TRADES                                                     │\n";
            std::cout << "├────────────────────────────────────────────────────────────────────┤\n";
            for (size_t i = 0; i < std::min(recent_trades.size(), size_t(5)); i++) {
                std::cout << "│ " << std::left << std::setw(66) << recent_trades[i] << " │\n";
            }
            std::cout << "└────────────────────────────────────────────────────────────────────┘\n";
        }
        
        std::cout << "\nPress Ctrl+C to exit...\n";
    }
};