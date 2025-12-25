#pragma once
#include <memory>
#include <string>
#include "adapter.hpp"
#include "alpaca_paper.hpp"
#include "mock_exchange.hpp"

class ExchangeFactory {
public:
    static std::unique_ptr<ExchangeAdapter> create(
        const std::string& exchange_name,
        const std::string& mode = "paper") {
        
        if (mode == "paper" || mode == "paper_alpaca") {
            if (exchange_name == "alpaca") {
                // Get API keys from environment
                const char* api_key = std::getenv("ALPACA_API_KEY");
                const char* secret_key = std::getenv("ALPACA_SECRET_KEY");
                
                if (!api_key || !secret_key) {
                    std::cerr << "ALPACA_API_KEY and ALPACA_SECRET_KEY must be set in environment" 
                              << std::endl;
                    return std::make_unique<MockExchange>();
                }
                
                return std::make_unique<AlpacaPaperAdapter>(api_key, secret_key);
            }
        }
        
        // Default to mock exchange
        return std::make_unique<MockExchange>();
    }
};