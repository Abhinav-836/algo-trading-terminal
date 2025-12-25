#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>

#include "core/trading_core.hpp"
#include "exchange/alpaca_paper.hpp"
#include "strategies/scalper.hpp"
#include "strategies/market_maker.hpp"
#include "core/config.hpp"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "Starting Algo Trading Terminal..." << std::endl;

    try {
        // Load configuration (stub for now)
        Config config;

        // Create trading core
        TradingCore core;

        // Create strategies
        auto scalper = std::make_shared<ScalperStrategy>(
            100.0,   // order size
            0.001,   // take profit
            0.0005   // stop loss
        );

        auto market_maker = std::make_shared<MarketMakerStrategy>(
            0.0005,  // spread
            50.0     // inventory limit
        );

        // Load API keys from environment
        const char* key = std::getenv("ALPACA_API_KEY");
        const char* secret = std::getenv("ALPACA_API_SECRET");

        if (!key || !secret) {
            throw std::runtime_error("Missing Alpaca API credentials");
        }

        // Create exchange adapter (CORRECT)
        std::shared_ptr<ExchangeAdapter> exchange =
            std::make_shared<AlpacaPaperAdapter>(
                std::string(key),
                std::string(secret)
            );

        // Start trading engine
        core.start();

        // Market data loop
        exchange->poll([&core](const Tick& tick) {
            core.process_tick(tick);
        });

        std::cout << "Press Enter to stop trading..." << std::endl;
        std::cin.get();

        core.stop();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Algo Trading Terminal stopped" << std::endl;
    return 0;
}
