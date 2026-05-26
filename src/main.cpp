#include <iostream>
#include <memory>
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <vector>
#include <ctime>

#include "core/config.hpp"
#include "exchange/exchange_factory.hpp"
#include "strategies/momentum_strategy.hpp"
#include "strategies/weighted_strategy.hpp"
#include "live/order_manager.hpp"
#include "live/market_hours.hpp"

std::unique_ptr<ExchangeAdapter> g_exchange;
std::unique_ptr<OrderManager> g_order_manager;
std::vector<std::unique_ptr<Strategy>> g_strategies;
std::atomic<bool> g_running{true};
std::atomic<uint64_t> g_tick_count{0};
std::atomic<uint64_t> g_signal_count{0};

// Performance tracking
std::chrono::high_resolution_clock::time_point g_start_time;
std::atomic<uint64_t> g_max_latency_us{0};
std::atomic<uint64_t> g_total_latency_us{0};
Config g_config;

void signal_handler(int signal) {
    std::cout << "\n⚠️ Shutting down..." << std::endl;
    g_running = false;
    if (g_exchange) g_exchange->stop();
    if (g_order_manager) g_order_manager->stop();
}

void print_performance_stats() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_start_time).count();
    
    double ticks_per_sec = elapsed > 0 ? (double)g_tick_count.load() / elapsed : 0;
    double signals_per_sec = elapsed > 0 ? (double)g_signal_count.load() / elapsed : 0;
    
    uint64_t signal_count = g_signal_count.load();
    uint64_t avg_latency = signal_count > 0 ? g_total_latency_us.load() / signal_count : 0;
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    PERFORMANCE STATISTICS                       ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Ticks Processed:   " << std::setw(10) << g_tick_count.load() << "                           ║\n";
    std::cout << "║  Signals Generated: " << std::setw(10) << signal_count << "                           ║\n";
    std::cout << "║  Ticks/sec:         " << std::setw(10) << std::fixed << std::setprecision(0) << ticks_per_sec << "                           ║\n";
    std::cout << "║  Signals/sec:       " << std::setw(10) << signals_per_sec << "                           ║\n";
    std::cout << "║  Max Latency:       " << std::setw(10) << g_max_latency_us.load() << " μs                         ║\n";
    std::cout << "║  Avg Latency:       " << std::setw(10) << avg_latency << " μs                         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
}

void create_strategies() {
    auto active_strategies = g_config.get_active_strategies();
    
    for (const auto& strategy_name : active_strategies) {
        if (strategy_name == "momentum" && g_config.is_strategy_enabled("momentum")) {
            double position_size = g_config.get_strategy_param("momentum", "position_size", 10);
            double rsi_upper = g_config.get_strategy_param("momentum", "rsi_upper", 70);
            double rsi_lower = g_config.get_strategy_param("momentum", "rsi_lower", 30);
            
            auto strategy = std::make_unique<MomentumStrategy>(position_size, rsi_upper, rsi_lower);
            std::cout << "  ✓ Momentum Strategy (size=" << position_size << " shares)" << std::endl;
            g_strategies.push_back(std::move(strategy));
        }
        else if (strategy_name == "weighted" && g_config.is_strategy_enabled("weighted")) {
            double position_size = g_config.get_strategy_param("weighted", "position_size", 10);
            double entry_threshold = g_config.get_strategy_param("weighted", "entry_threshold", 0.60);
            double exit_threshold = g_config.get_strategy_param("weighted", "exit_threshold", 0.30);
            
            auto strategy = std::make_unique<WeightedStrategy>(position_size, entry_threshold, exit_threshold);
            
            // Set weights from config
            double momentum_w = g_config.get_strategy_param("weighted.weights", "momentum_weight", 0.35);
            double rsi_w = g_config.get_strategy_param("weighted.weights", "rsi_weight", 0.25);
            double volume_w = g_config.get_strategy_param("weighted.weights", "volume_weight", 0.20);
            double vwap_w = g_config.get_strategy_param("weighted.weights", "vwap_weight", 0.20);
            strategy->set_weights(momentum_w, rsi_w, volume_w, vwap_w);
            
            std::cout << "  ✓ Weighted Strategy (size=" << position_size << " shares, entry=" << entry_threshold << ")" << std::endl;
            g_strategies.push_back(std::move(strategy));
        }
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << R"(
╔════════════════════════════════════════════════════════════════════════╗
║                     FAST TRADING TERMINAL v3.0                         ║
║                  Configurable | Real-time | Production                  ║
╚════════════════════════════════════════════════════════════════════════╝
)" << std::endl;
    
    // Load configuration
    std::string config_file = "config.yaml";
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) config_file = argv[++i];
        else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [--config config.yaml]\n";
            std::cout << "\nEdit config.yaml to change:\n";
            std::cout << "  - Trading symbols\n";
            std::cout << "  - Active strategies\n";
            std::cout << "  - Strategy parameters\n";
            std::cout << "  - Risk limits\n";
            return 0;
        }
    }
    
    std::cout << "📁 Loading configuration from: " << config_file << std::endl;
    g_config.load_from_file(config_file);
    g_config.print_config();
    
    // Parse command line overrides
    std::string mode = g_config.get("trading.mode", "paper");
    std::string exchange = g_config.get("trading.exchange", "alpaca");
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
        else if (arg == "--exchange" && i + 1 < argc) exchange = argv[++i];
    }
    
    // Check market hours (skip for crypto)
    if (exchange != "binance" && !MarketHours::is_market_open()) {
        std::cout << "⚠️ Market is currently CLOSED\n";
        std::cout << "   Next open in: " << MarketHours::seconds_until_open() << " seconds\n";
        std::cout << "   Waiting for market open...\n" << std::endl;
        
        while (g_running && !MarketHours::is_market_open()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        
        if (!g_running) return 0;
        std::cout << "✅ Market OPEN! Starting trading...\n" << std::endl;
    }
    
    g_start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Connect to exchange
        std::cout << "🔌 Connecting to " << exchange << " (" << mode << " mode)..." << std::endl;
        g_exchange = ExchangeFactory::create(exchange, mode);
        
        if (!g_exchange || !g_exchange->connect()) {
            throw std::runtime_error("Failed to connect to exchange");
        }
        
        auto account = g_exchange->get_account_info();
        std::cout << "✅ Connected!\n";
        std::cout << "   Account: " << account.id << "\n";
        std::cout << "   Cash: $" << std::fixed << std::setprecision(2) << account.cash << "\n";
        std::cout << "   Buying Power: $" << account.buying_power << "\n\n";
        
        // Initialize order manager
        g_order_manager = std::make_unique<OrderManager>(*g_exchange);
        
        // Create strategies from config
        std::cout << "📈 Loading strategies:\n";
        create_strategies();
        std::cout << std::endl;
        
        if (g_strategies.empty()) {
            std::cout << "⚠️ No strategies enabled. Add to config.yaml [strategies] active list\n";
            return 0;
        }
        
        // Get symbols from config
        auto symbols = g_config.get_symbols();
        std::cout << "📊 Subscribing to symbols: ";
        for (size_t i = 0; i < symbols.size(); i++) {
            std::cout << symbols[i];
            if (i < symbols.size() - 1) std::cout << ", ";
            g_exchange->subscribe_symbol(symbols[i]);
        }
        std::cout << "\n\n";
        
        // Set up tick callback for all strategies
        g_exchange->set_tick_callback([&](const Tick& tick) {
            auto tick_start = std::chrono::high_resolution_clock::now();
            g_tick_count++;
            
            // Process each strategy
            for (auto& strategy : g_strategies) {
                auto signal = strategy->generate_signal(tick);
                
                auto signal_end = std::chrono::high_resolution_clock::now();
                uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    signal_end - tick_start).count();
                
                g_total_latency_us += latency;
                if (latency > g_max_latency_us) g_max_latency_us = latency;
                
                if (signal.valid) {
                    g_signal_count++;
                    g_order_manager->process_signal(signal);
                }
            }
        });
        
        // Start systems
        g_order_manager->start();
        g_exchange->run();
        
        std::cout << "🚀 TRADING SYSTEM ACTIVE - Press Ctrl+C to stop\n";
        std::cout << "   Processing " << symbols.size() << " symbols with " << g_strategies.size() << " strategies\n\n";
        
        // Status updates
        auto last_status = std::chrono::steady_clock::now();
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto now = std::chrono::steady_clock::now();
            int update_interval = g_config.get_int("performance.status_update_interval", 10);
            
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status).count() >= update_interval) {
                auto account_info = g_exchange->get_account_info();
                double return_pct = ((account_info.cash - g_config.get_double("trading.initial_capital", 100000)) 
                                    / g_config.get_double("trading.initial_capital", 100000)) * 100;
                
                // Fix: Convert steady_clock::time_point to time_t for localtime
                auto now_time = std::chrono::system_clock::now();
                std::time_t now_time_t = std::chrono::system_clock::to_time_t(now_time);
                
                std::cout << "\r📊 [" << std::put_time(std::localtime(&now_time_t), "%H:%M:%S")
                          << "] Ticks: " << g_tick_count.load()
                          << " | Signals: " << g_signal_count.load()
                          << " | Balance: $" << std::fixed << std::setprecision(2) << account_info.cash
                          << " | Return: " << (return_pct >= 0 ? "+" : "") << std::fixed << std::setprecision(2) << return_pct << "%"
                          << " | Latency: " << g_max_latency_us.load() << "μs    ";
                std::cout.flush();
                last_status = now;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ FATAL: " << e.what() << std::endl;
        return 1;
    }
    
    // Final summary
    print_performance_stats();
    
    auto final_account = g_exchange->get_account_info();
    double initial_capital = g_config.get_double("trading.initial_capital", 100000);
    double total_return = final_account.cash - initial_capital;
    double return_pct = (total_return / initial_capital) * 100;
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                      FINAL P&L SUMMARY                          ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Initial Capital:  $" << std::setw(10) << std::fixed << std::setprecision(2) << initial_capital << "                           ║\n";
    std::cout << "║  Final Balance:    $" << std::setw(10) << final_account.cash << "                           ║\n";
    std::cout << "║  Total P&L:        $" << std::setw(10) << total_return << "                           ║\n";
    std::cout << "║  Return (%):       " << std::setw(10) << return_pct << "%                           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    if (g_order_manager) g_order_manager->print_metrics();
    
    std::cout << "\n✅ Trading terminal stopped gracefully.\n";
    
    return 0;
}