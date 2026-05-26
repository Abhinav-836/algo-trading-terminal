#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "../core/types.hpp"

using json = nlohmann::json;

// WebSocket includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "libcurl.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

namespace alpaca {

class AlpacaClient {
public:
    enum class Mode { PAPER, LIVE };
    
    AlpacaClient(const std::string& api_key, const std::string& secret_key, Mode mode = Mode::PAPER);
    ~AlpacaClient();
    
    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const { return m_connected; }
    
    // Account methods
    AccountInfo get_account();
    double get_cash_balance();
    double get_buying_power();
    
    // Order methods
    std::string place_order(const Order& order);
    bool cancel_order(const std::string& order_id);
    std::vector<Order> get_orders(bool open_only = true);
    Order get_order(const std::string& order_id);
    
    // Position methods
    std::vector<Position> get_positions();
    Position get_position(const std::string& symbol);
    bool close_position(const std::string& symbol);
    
    // Market data (REST)
    Tick get_latest_tick(const std::string& symbol);
    std::vector<Bar> get_bars(const std::string& symbol, const std::string& timeframe, int limit = 100);
    
    // Real-time market data (WebSocket)
    void subscribe_trades(const std::vector<std::string>& symbols);
    void subscribe_quotes(const std::vector<std::string>& symbols);
    void subscribe_bars(const std::vector<std::string>& symbols);
    void set_trade_callback(std::function<void(const Tick&)> callback);
    void set_quote_callback(std::function<void(const Tick&)> callback);
    
    // Main polling loop
    void poll(std::function<void(const Tick&)> callback);
    
private:
    std::string m_api_key;
    std::string m_secret_key;
    Mode m_mode;
    std::string m_base_url;
    std::string m_data_url;
    std::string m_ws_url;
    
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_streaming{false};
    CURL* m_curl{nullptr};
    
    // WebSocket
    int m_ws_socket{-1};
    std::thread m_ws_thread;
    std::thread m_poll_thread;
    std::mutex m_callback_mutex;
    std::function<void(const Tick&)> m_trade_callback;
    std::function<void(const Tick&)> m_quote_callback;
    
    // Order polling
    std::atomic<uint64_t> m_last_order_check{0};
    std::unordered_map<std::string, Order> m_active_orders;
    std::mutex m_order_mutex;
    
    // Helper methods
    std::string send_request(const std::string& endpoint, const std::string& method = "GET", 
                             const std::string& body = "");
    json authenticated_request(const std::string& endpoint, const std::string& method = "GET",
                               const std::string& body = "");
    
    // WebSocket methods
    bool connect_websocket();
    void websocket_thread();
    void send_websocket_message(const std::string& message);
    void handle_websocket_message(const std::string& message);
    
    // Order management
    void poll_order_updates();
    void update_order_status(const json& order_data);
    
    static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, std::string* response);
};

} // namespace alpaca