#include "mock_exchange.hpp"
#pragma once
#include <memory>
#include <string>
#include "adapter.hpp"
#include "alpaca_client.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <iomanip>

class AlpacaAdapter : public ExchangeAdapter {
private:
    std::unique_ptr<alpaca::AlpacaClient> m_client;
    std::function<void(const Tick&)> m_tick_callback;
    std::atomic<bool> m_running{false};
    std::thread m_poll_thread;
    std::string m_symbol;
    std::atomic<bool> m_connected{false};
    
public:
    AlpacaAdapter(const std::string& api_key, const std::string& secret_key, 
                  alpaca::AlpacaClient::Mode mode)
        : m_client(std::make_unique<alpaca::AlpacaClient>(api_key, secret_key, mode)) {
        m_connected = false;
    }
    
    ~AlpacaAdapter() { stop(); }
    
    bool connect() override {
        bool result = m_client->connect();
        if (result) m_connected = true;
        return result;
    }
    
    void disconnect() override {
        m_connected = false;
        m_client->disconnect();
    }
    
    bool is_connected() const override {
        return m_connected && m_client->is_connected();
    }
    
    std::string place_order(const Order& order) override {
        return m_client->place_order(order);
    }
    
    bool cancel_order(const std::string& order_id) override {
        return m_client->cancel_order(order_id);
    }
    
    std::vector<Order> get_open_orders() override {
        return m_client->get_orders(true);
    }
    
    double get_balance() const override {
        return m_client->get_cash_balance();
    }
    
    std::vector<Position> get_positions() override {
        return m_client->get_positions();
    }
    
    AccountInfo get_account_info() override {
        return m_client->get_account();
    }
    
    Tick get_current_tick(const std::string& symbol) override {
        return m_client->get_latest_tick(symbol);
    }
    
    void subscribe_symbol(const std::string& symbol) override {
        m_symbol = symbol;
        m_client->subscribe_trades({symbol});
        m_client->subscribe_quotes({symbol});
    }
    
    void set_tick_callback(std::function<void(const Tick&)> callback) override {
        m_tick_callback = callback;
        m_client->set_trade_callback(callback);
    }
    
    void run() override {
        if (!m_connected) return;
        
        m_running = true;
        m_poll_thread = std::thread([this]() {
            while (m_running && m_connected) {
                auto positions = get_positions();
                auto account = get_account_info();
                
                static auto last_log = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 10) {
                    std::cout << "\n📊 STATUS: Balance: $" << std::fixed << std::setprecision(2) 
                              << account.cash << " | Equity: $" << account.equity
                              << " | Positions: " << positions.size() << std::endl;
                    last_log = now;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
    
    void stop() override {
        m_running = false;
        if (m_poll_thread.joinable()) m_poll_thread.join();
        disconnect();
    }
};

class ExchangeFactory {
public:
    enum class Mode { PAPER, LIVE, MOCK };
    
    static std::unique_ptr<ExchangeAdapter> create_alpaca(const std::string& api_key, 
                                                           const std::string& secret_key,
                                                           Mode mode) {
        alpaca::AlpacaClient::Mode client_mode = (mode == Mode::PAPER) ? 
            alpaca::AlpacaClient::Mode::PAPER : alpaca::AlpacaClient::Mode::LIVE;
        
        return std::make_unique<AlpacaAdapter>(api_key, secret_key, client_mode);
    }
    
    static std::unique_ptr<ExchangeAdapter> create(const std::string& exchange,
                                                    const std::string& mode_str) {
        Mode mode = (mode_str == "live") ? Mode::LIVE : Mode::PAPER;
        
        if (exchange == "alpaca") {
            const char* api_key = std::getenv("ALPACA_API_KEY");
            const char* secret_key = std::getenv("ALPACA_SECRET_KEY");
            
            if (!api_key || !secret_key) {
                std::cerr << "ERROR: Missing Alpaca API keys. Set environment variables:" << std::endl;
                std::cerr << "  export ALPACA_API_KEY=your_key" << std::endl;
                std::cerr << "  export ALPACA_SECRET_KEY=your_secret" << std::endl;
                return nullptr;
            }
            
            return create_alpaca(api_key, secret_key, mode);
        }
        
        if (exchange == "mock") {
            return std::make_unique<MockExchange>();
        }
        
        std::cerr << "Unknown exchange: " << exchange << std::endl;
        return nullptr;
    }
};