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
#include "live/risk.hpp"           // FIX #5
#include "live/circuit_breaker.hpp"// FIX #6

std::unique_ptr<ExchangeAdapter> g_exchange;
std::unique_ptr<RiskManager>     g_risk;        // FIX #5
std::unique_ptr<CircuitBreaker>  g_breaker;     // FIX #6
std::unique_ptr<OrderManager>    g_order_manager;
std::vector<std::unique_ptr<Strategy>> g_strategies;
std::atomic<bool>     g_running{true};
std::atomic<uint64_t> g_tick_count{0};
std::atomic<uint64_t> g_signal_count{0};

// Performance tracking
std::chrono::high_resolution_clock::time_point g_start_time;
std::atomic<uint64_t> g_max_latency_us{0};     // FIX #11: tracked via CAS in tick callback
std::atomic<uint64_t> g_total_latency_us{0};
Config g_config;

void signal_handler(int /*signal*/) {
    std::cout << "\n⚠️ Shutting down..." << std::endl;
    g_running = false;
    if (g_exchange)      g_exchange->stop();
    if (g_order_manager) g_order_manager->stop();
}

void print_performance_stats() {
    auto now     = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - g_start_time).count();

    double ticks_per_sec   = elapsed > 0 ? (double)g_tick_count.load()   / elapsed : 0;
    double signals_per_sec = elapsed > 0 ? (double)g_signal_count.load() / elapsed : 0;

    uint64_t signal_count = g_signal_count.load();
    uint64_t avg_latency  = signal_count > 0 ? g_total_latency_us.load() / signal_count : 0;

    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    PERFORMANCE STATISTICS                       ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Ticks Processed:   " << std::setw(10) << g_tick_count.load()        << "                           ║\n";
    std::cout << "║  Signals Generated: " << std::setw(10) << signal_count               << "                           ║\n";
    std::cout << "║  Ticks/sec:         " << std::setw(10) << std::fixed << std::setprecision(0) << ticks_per_sec    << "                           ║\n";
    std::cout << "║  Signals/sec:       " << std::setw(10) << signals_per_sec            << "                           ║\n";
    std::cout << "║  Max Latency:       " << std::setw(10) << g_max_latency_us.load()    << " μs                         ║\n";
    std::cout << "║  Avg Latency:       " << std::setw(10) << avg_latency                << " μs                         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
}

void create_strategies() {
    auto active_strategies = g_config.get_active_strategies();

    for (const auto& strategy_name : active_strategies) {
        if (strategy_name == "momentum" && g_config.is_strategy_enabled("momentum")) {
            double position_size = g_config.get_strategy_param("momentum", "position_size", 10);
            double rsi_upper     = g_config.get_strategy_param("momentum", "rsi_upper",     70);
            double rsi_lower     = g_config.get_strategy_param("momentum", "rsi_lower",     30);

            auto strategy = std::make_unique<MomentumStrategy>(position_size, rsi_upper, rsi_lower);
            std::cout << "  ✓ Momentum Strategy (size=" << position_size << " shares)\n";
            g_strategies.push_back(std::move(strategy));
        }
        else if (strategy_name == "weighted" && g_config.is_strategy_enabled("weighted")) {
            double position_size    = g_config.get_strategy_param("weighted", "position_size",    10);
            double entry_threshold  = g_config.get_strategy_param("weighted", "entry_threshold",  0.60);
            double exit_threshold   = g_config.get_strategy_param("weighted", "exit_threshold",   0.30);

            auto strategy = std::make_unique<WeightedStrategy>(position_size, entry_threshold, exit_threshold);

            double momentum_w = g_config.get_strategy_param("weighted.weights", "momentum_weight", 0.30);
            double rsi_w      = g_config.get_strategy_param("weighted.weights", "rsi_weight",      0.20);
            double volume_w   = g_config.get_strategy_param("weighted.weights", "volume_weight",   0.15);
            double vwap_w     = g_config.get_strategy_param("weighted.weights", "vwap_weight",     0.15);
            double sma_w      = g_config.get_strategy_param("weighted.weights", "sma_weight",      0.20);
            // FIX #12: pass sma_w so it is included in the weighted score
            strategy->set_weights(momentum_w, rsi_w, volume_w, vwap_w, sma_w);

            std::cout << "  ✓ Weighted Strategy (size=" << position_size
                      << " shares, entry=" << entry_threshold << ")\n";
            g_strategies.push_back(std::move(strategy));
        }
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << R"(
╔════════════════════════════════════════════════════════════════════════╗
║                     FAST TRADING TERMINAL v3.0                         ║
║                  Configurable | Real-time | Production                  ║
╚════════════════════════════════════════════════════════════════════════╝
)" << std::endl;

    std::string config_file = "config.yaml";
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if      (arg == "--config" && i + 1 < argc) config_file = argv[++i];
        else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [--config config.yaml]\n\n";
            std::cout << "Edit config.yaml to change:\n";
            std::cout << "  - Trading symbols\n";
            std::cout << "  - Active strategies\n";
            std::cout << "  - Strategy parameters\n";
            std::cout << "  - Risk limits\n";
            return 0;
        }
    }

    std::cout << "📁 Loading configuration from: " << config_file << "\n";
    g_config.load_from_file(config_file);
    g_config.print_config();

    std::string mode     = g_config.get("trading.mode",     "paper");
    std::string exchange = g_config.get("trading.exchange", "alpaca");

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if      (arg == "--mode"     && i + 1 < argc) mode     = argv[++i];
        else if (arg == "--exchange" && i + 1 < argc) exchange = argv[++i];
    }

    if (exchange != "binance" && !MarketHours::is_market_open()) {
        std::cout << "⚠️ Market is currently CLOSED\n";
        std::cout << "   Next open in: " << MarketHours::seconds_until_open() << " seconds\n";
        std::cout << "   Waiting for market open...\n\n";

        while (g_running && !MarketHours::is_market_open())
            std::this_thread::sleep_for(std::chrono::seconds(30));

        if (!g_running) return 0;
        std::cout << "✅ Market OPEN! Starting trading...\n\n";
    }

    g_start_time = std::chrono::high_resolution_clock::now();

    double initial_capital = g_config.get_double("trading.initial_capital", 100000.0);

    try {
        // FIX #5 & #6: Instantiate and configure RiskManager and CircuitBreaker
        g_risk    = std::make_unique<RiskManager>();
        g_breaker = std::make_unique<CircuitBreaker>(initial_capital);

        std::cout << "🔌 Connecting to " << exchange << " (" << mode << " mode)...\n";
        g_exchange = ExchangeFactory::create(exchange, mode);

        if (!g_exchange || !g_exchange->connect())
            throw std::runtime_error("Failed to connect to exchange");

        auto account = g_exchange->get_account_info();
        std::cout << "✅ Connected!\n";
        std::cout << "   Account: "      << account.id << "\n";
        std::cout << "   Cash: $"        << std::fixed << std::setprecision(2) << account.cash << "\n";
        std::cout << "   Buying Power: $"<< account.buying_power << "\n";
        std::cout << "   Equity: $"      << account.equity << "\n\n";

        // FIX #5 & #6: Pass risk and breaker into OrderManager
        g_order_manager = std::make_unique<OrderManager>(
            *g_exchange, *g_risk, *g_breaker);

        std::cout << "📈 Loading strategies:\n";
        create_strategies();
        std::cout << "\n";

        if (g_strategies.empty()) {
            std::cout << "⚠️ No strategies enabled. Add to config.yaml [strategies] active list\n";
            return 0;
        }

        auto symbols = g_config.get_symbols();
        std::cout << "📊 Subscribing to symbols: ";
        for (size_t i = 0; i < symbols.size(); i++) {
            std::cout << symbols[i];
            if (i < symbols.size() - 1) std::cout << ", ";
            g_exchange->subscribe_symbol(symbols[i]);
        }
        std::cout << "\n\n";

        g_exchange->set_tick_callback([&](const Tick& tick) {
            auto tick_start = std::chrono::high_resolution_clock::now();
            g_tick_count++;

            for (auto& strategy : g_strategies) {
                auto signal = strategy->generate_signal(tick);

                auto     signal_end = std::chrono::high_resolution_clock::now();
                uint64_t latency    = std::chrono::duration_cast<std::chrono::microseconds>(
                                          signal_end - tick_start).count();

                g_total_latency_us += latency;

                // FIX #11: Atomic CAS loop for max latency — no read-modify-write race.
                uint64_t prev = g_max_latency_us.load(std::memory_order_relaxed);
                while (latency > prev &&
                       !g_max_latency_us.compare_exchange_weak(
                           prev, latency,
                           std::memory_order_relaxed,
                           std::memory_order_relaxed)) {}

                if (signal.valid) {
                    g_signal_count++;
                    g_order_manager->process_signal(signal);
                }
            }
        });

        g_order_manager->start();
        g_exchange->run();

        std::cout << "🚀 TRADING SYSTEM ACTIVE - Press Ctrl+C to stop\n";
        std::cout << "   Processing " << symbols.size() << " symbols with "
                  << g_strategies.size() << " strategies\n\n";

        auto last_status = std::chrono::steady_clock::now();

        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto now = std::chrono::steady_clock::now();
            int  update_interval = g_config.get_int("performance.status_update_interval", 10);

            if (std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_status).count() >= update_interval) {

                auto account_info = g_exchange->get_account_info();

                // FIX #9: Use equity (cash + open positions) for return calculation,
                // not raw cash which drops whenever a position is open.
                double equity     = account_info.equity > 0
                                      ? account_info.equity
                                      : account_info.cash;
                double return_pct = (equity - initial_capital) / initial_capital * 100.0;

                auto      now_sys    = std::chrono::system_clock::now();
                std::time_t now_time_t = std::chrono::system_clock::to_time_t(now_sys);

                // FIX #10: thread-safe localtime
                struct tm tm_buf{};
#ifdef _WIN32
                localtime_s(&tm_buf, &now_time_t);
#else
                localtime_r(&now_time_t, &tm_buf);
#endif
                std::cout << "\r📊 [" << std::put_time(&tm_buf, "%H:%M:%S")
                          << "] Ticks: "   << g_tick_count.load()
                          << " | Signals: "<< g_signal_count.load()
                          << " | Equity: $"<< std::fixed << std::setprecision(2) << equity
                          << " | Return: " << (return_pct >= 0 ? "+" : "")
                          << std::fixed << std::setprecision(2) << return_pct << "%"
                          << " | MaxLat: " << g_max_latency_us.load() << "μs    ";
                std::cout.flush();
                last_status = now;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "\n❌ FATAL: " << e.what() << "\n";
        return 1;
    }

    print_performance_stats();

    auto   final_account = g_exchange->get_account_info();
    // FIX #9: Use equity for final P&L summary too.
    double final_equity  = final_account.equity > 0
                             ? final_account.equity
                             : final_account.cash;
    double total_return  = final_equity - initial_capital;
    double return_pct    = (total_return / initial_capital) * 100.0;

    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                      FINAL P&L SUMMARY                          ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Initial Capital:  $" << std::setw(10) << std::fixed << std::setprecision(2) << initial_capital << "                           ║\n";
    std::cout << "║  Final Equity:     $" << std::setw(10) << final_equity                                          << "                           ║\n";
    std::cout << "║  Total P&L:        $" << std::setw(10) << total_return                                          << "                           ║\n";
    std::cout << "║  Return (%):       "  << std::setw(10) << return_pct                                            << "%                           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";

    if (g_order_manager) g_order_manager->print_metrics();

    std::cout << "\n✅ Trading terminal stopped gracefully.\n";
    return 0;
}