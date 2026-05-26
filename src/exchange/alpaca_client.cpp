#include "alpaca_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <regex>
#include <algorithm>

namespace alpaca {

// Helper for WebSocket handshake
static std::string base64_encode(const std::string& input) {
    static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (char c : input) {
        char_array_3[i++] = c;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (int j = 0; j < 4; j++)
                output += base64_chars[char_array_4[j]];
            i = 0;
        }
    }
    
    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (int j = 0; j < i + 1; j++)
            output += base64_chars[char_array_4[j]];
        
        while (i++ < 3)
            output += '=';
    }
    
    return output;
}

static std::string sha1_hash(const std::string& input) {
    unsigned char hash[20];
    // Simplified - in production use OpenSSL's SHA1
    // For now, return a placeholder
    return std::string(reinterpret_cast<char*>(hash), 20);
}

AlpacaClient::AlpacaClient(const std::string& api_key, const std::string& secret_key, Mode mode)
    : m_api_key(api_key), m_secret_key(secret_key), m_mode(mode) {
    
    if (mode == Mode::PAPER) {
        m_base_url = "https://paper-api.alpaca.markets";
        m_data_url = "https://data.alpaca.markets";
        m_ws_url = "wss://stream.data.alpaca.markets/v2/iex";
    } else {
        m_base_url = "https://api.alpaca.markets";
        m_data_url = "https://data.alpaca.markets";
        m_ws_url = "wss://stream.data.alpaca.markets/v2/sip";
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_curl = curl_easy_init();
}

AlpacaClient::~AlpacaClient() {
    disconnect();
    if (m_curl) curl_easy_cleanup(m_curl);
    curl_global_cleanup();
}

size_t AlpacaClient::curl_write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total = size * nmemb;
    response->append((char*)contents, total);
    return total;
}

json AlpacaClient::authenticated_request(const std::string& endpoint, const std::string& method, const std::string& body) {
    std::string response = send_request(endpoint, method, body);
    if (response.empty()) return json::object();
    
    try {
        return json::parse(response);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
        std::cerr << "Response: " << response << std::endl;
        return json::object();
    }
}

std::string AlpacaClient::send_request(const std::string& endpoint, const std::string& method, const std::string& body) {
    if (!m_curl) return "";
    
    std::string url = m_base_url + endpoint;
    std::string response;
    
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    // Alpaca headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + m_api_key).c_str());
    headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + m_secret_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
    
    if (method == "POST") {
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "DELETE") {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "PUT") {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, body.c_str());
    } else {
        curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
    }
    
    CURLcode res = curl_easy_perform(m_curl);
    
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        return "";
    }
    
    long http_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (http_code < 200 || http_code >= 300) {
        std::cerr << "HTTP error: " << http_code << " - " << response << std::endl;
        return "";
    }
    
    return response;
}

bool AlpacaClient::connect() {
    std::cout << "Connecting to Alpaca " << (m_mode == Mode::PAPER ? "PAPER" : "LIVE") << " API..." << std::endl;
    
    // Test connection by getting account
    AccountInfo account = get_account();
    if (account.id.empty()) {
        std::cerr << "Failed to connect to Alpaca API. Check your API keys." << std::endl;
        return false;
    }
    
    m_connected = true;
    
    std::cout << "✓ Connected to Alpaca" << std::endl;
    std::cout << "  Account ID: " << account.id << std::endl;
    std::cout << "  Cash: $" << std::fixed << std::setprecision(2) << account.cash << std::endl;
    std::cout << "  Buying Power: $" << account.buying_power << std::endl;
    std::cout << "  Portfolio Value: $" << account.portfolio_value << std::endl;
    
    // Start order polling thread
    m_poll_thread = std::thread(&AlpacaClient::poll_order_updates, this);
    
    // Connect WebSocket for real-time data
    if (connect_websocket()) {
        std::cout << "✓ WebSocket connected for real-time data" << std::endl;
    }
    
    return true;
}

void AlpacaClient::disconnect() {
    m_connected = false;
    m_streaming = false;
    
    if (m_ws_socket != -1) {
#ifdef _WIN32
        closesocket(m_ws_socket);
#else
        close(m_ws_socket);
#endif
        m_ws_socket = -1;
    }
    
    if (m_ws_thread.joinable()) m_ws_thread.join();
    if (m_poll_thread.joinable()) m_poll_thread.join();
}

AccountInfo AlpacaClient::get_account() {
    json resp = authenticated_request("/v2/account");
    if (resp.empty()) return AccountInfo{};
    
    AccountInfo account;
    account.id = resp.value("id", "");
    account.status = resp.value("status", "");
    account.cash = std::stod(resp.value("cash", "0"));
    account.buying_power = std::stod(resp.value("buying_power", "0"));
    account.portfolio_value = std::stod(resp.value("portfolio_value", "0"));
    account.long_market_value = std::stod(resp.value("long_market_value", "0"));
    account.short_market_value = std::stod(resp.value("short_market_value", "0"));
    account.equity = std::stod(resp.value("equity", "0"));
    account.last_equity = std::stod(resp.value("last_equity", "0"));
    account.initial_margin = std::stod(resp.value("initial_margin", "0"));
    account.maintenance_margin = std::stod(resp.value("maintenance_margin", "0"));
    account.day_trade_count = resp.value("day_trade_count", 0);
    account.pattern_day_trader = resp.value("pattern_day_trader", false);
    
    return account;
}

double AlpacaClient::get_cash_balance() {
    return get_account().cash;
}

double AlpacaClient::get_buying_power() {
    return get_account().buying_power;
}

std::string AlpacaClient::place_order(const Order& order) {
    json order_json;
    order_json["symbol"] = order.symbol;
    order_json["qty"] = order.quantity;
    order_json["side"] = order.is_buy ? "buy" : "sell";
    order_json["type"] = order.type;
    order_json["time_in_force"] = order.tif;
    
    if (order.is_limit()) {
        order_json["limit_price"] = order.price;
    }
    
    if (!order.client_order_id.empty()) {
        order_json["client_order_id"] = order.client_order_id;
    }
    
    json resp = authenticated_request("/v2/orders", "POST", order_json.dump());
    
    if (!resp.empty() && resp.contains("id")) {
        std::string order_id = resp["id"];
        std::cout << "✓ Order placed: " << order_id 
                  << " | " << (order.is_buy ? "BUY" : "SELL")
                  << " " << order.quantity << " " << order.symbol << std::endl;
        return order_id;
    }
    
    std::cerr << "Failed to place order" << std::endl;
    return "";
}

bool AlpacaClient::cancel_order(const std::string& order_id) {
    json resp = authenticated_request("/v2/orders/" + order_id, "DELETE");
    return !resp.empty() || resp.is_null();
}

std::vector<Order> AlpacaClient::get_orders(bool open_only) {
    std::string endpoint = "/v2/orders";
    if (open_only) endpoint += "?status=open";
    
    json resp = authenticated_request(endpoint);
    if (!resp.is_array()) return {};
    
    std::vector<Order> orders;
    for (const auto& item : resp) {
        Order order;
        order.alpaca_id = item.value("id", "");
        order.client_order_id = item.value("client_order_id", "");
        order.symbol = item.value("symbol", "");
        order.quantity = std::stod(item.value("qty", "0"));
        order.filled_quantity = std::stod(item.value("filled_qty", "0"));
        order.is_buy = item.value("side", "") == "buy";
        order.type = item.value("type", "market");
        order.tif = item.value("time_in_force", "day");
        order.status = item.value("status", "");
        
        if (item.contains("limit_price") && !item["limit_price"].is_null()) {
            order.price = std::stod(item["limit_price"].get<std::string>());
        }
        
        orders.push_back(order);
    }
    
    return orders;
}

Order AlpacaClient::get_order(const std::string& order_id) {
    json resp = authenticated_request("/v2/orders/" + order_id);
    if (resp.empty()) return Order{};
    
    Order order;
    order.alpaca_id = resp.value("id", "");
    order.client_order_id = resp.value("client_order_id", "");
    order.symbol = resp.value("symbol", "");
    order.quantity = std::stod(resp.value("qty", "0"));
    order.filled_quantity = std::stod(resp.value("filled_qty", "0"));
    order.is_buy = resp.value("side", "") == "buy";
    order.type = resp.value("type", "market");
    order.tif = resp.value("time_in_force", "day");
    order.status = resp.value("status", "");
    
    if (resp.contains("limit_price") && !resp["limit_price"].is_null()) {
        order.price = std::stod(resp["limit_price"].get<std::string>());
    }
    
    return order;
}

std::vector<Position> AlpacaClient::get_positions() {
    json resp = authenticated_request("/v2/positions");
    if (!resp.is_array()) return {};
    
    std::vector<Position> positions;
    for (const auto& item : resp) {
        Position pos;
        pos.symbol = item.value("symbol", "");
        pos.quantity = std::stod(item.value("qty", "0"));
        pos.avg_entry_price = std::stod(item.value("avg_entry_price", "0"));
        pos.current_price = std::stod(item.value("current_price", "0"));
        pos.unrealized_pl = std::stod(item.value("unrealized_pl", "0"));
        pos.realized_pl = std::stod(item.value("realized_pl", "0"));
        pos.market_value = std::stod(item.value("market_value", "0"));
        positions.push_back(pos);
    }
    
    return positions;
}

Position AlpacaClient::get_position(const std::string& symbol) {
    json resp = authenticated_request("/v2/positions/" + symbol);
    if (resp.empty()) return Position{};
    
    Position pos;
    pos.symbol = resp.value("symbol", "");
    pos.quantity = std::stod(resp.value("qty", "0"));
    pos.avg_entry_price = std::stod(resp.value("avg_entry_price", "0"));
    pos.current_price = std::stod(resp.value("current_price", "0"));
    pos.unrealized_pl = std::stod(resp.value("unrealized_pl", "0"));
    pos.realized_pl = std::stod(resp.value("realized_pl", "0"));
    pos.market_value = std::stod(resp.value("market_value", "0"));
    
    return pos;
}

bool AlpacaClient::close_position(const std::string& symbol) {
    json resp = authenticated_request("/v2/positions/" + symbol, "DELETE");
    return !resp.empty();
}

Tick AlpacaClient::get_latest_tick(const std::string& symbol) {
    std::string url = m_data_url + "/v2/stocks/" + symbol + "/trades/latest";
    
    // Need to use data URL with different auth
    std::string response = send_request(url);
    if (response.empty()) return Tick{};
    
    try {
        json data = json::parse(response);
        if (data.contains("trade")) {
            auto& trade = data["trade"];
            Tick tick;
            tick.symbol = symbol;
            tick.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            tick.price = trade["p"];
            tick.volume = trade["s"];
            tick.bid = tick.price * 0.9999;
            tick.ask = tick.price * 1.0001;
            return tick;
        }
    } catch (...) {}
    
    return Tick{};
}

std::vector<Bar> AlpacaClient::get_bars(const std::string& symbol, const std::string& timeframe, int limit) {
    std::string url = m_data_url + "/v2/stocks/" + symbol + "/bars?timeframe=" + timeframe + "&limit=" + std::to_string(limit);
    
    std::string response = send_request(url);
    if (response.empty()) return {};
    
    try {
        json data = json::parse(response);
        std::vector<Bar> bars;
        
        if (data.contains("bars")) {
            for (const auto& bar_json : data["bars"]) {
                Bar bar;
                bar.symbol = symbol;
                bar.timestamp = std::stoull(bar_json.value("t", "0"));
                bar.open = bar_json["o"];
                bar.high = bar_json["h"];
                bar.low = bar_json["l"];
                bar.close = bar_json["c"];
                bar.volume = bar_json["v"];
                bar.vwap = bar_json["vw"];
                bar.trade_count = bar_json["n"];
                bars.push_back(bar);
            }
        }
        
        return bars;
    } catch (...) {}
    
    return {};
}

bool AlpacaClient::connect_websocket() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
    
    m_ws_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_ws_socket < 0) return false;
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(443);
    
    // Parse hostname
    std::string host = m_ws_url.substr(m_ws_url.find("://") + 3);
    host = host.substr(0, host.find(":"));
    
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) return false;
    
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (::connect(m_ws_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return false;
    }
    
    // WebSocket handshake
    std::string key = "dGhlIHNhbXBsZSBub25jZQ=="; // Base64 encoded random
    std::string handshake = 
        "GET /v2/iex HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + key + "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    
    send(m_ws_socket, handshake.c_str(), handshake.length(), 0);
    
    char buffer[4096];
    recv(m_ws_socket, buffer, sizeof(buffer) - 1, 0);
    
    std::string response(buffer);
    if (response.find("101") == std::string::npos) {
        return false;
    }
    
    m_streaming = true;
    m_ws_thread = std::thread(&AlpacaClient::websocket_thread, this);
    
    // Authenticate with Alpaca WebSocket
    json auth_msg;
    auth_msg["action"] = "auth";
    auth_msg["key"] = m_api_key;
    auth_msg["secret"] = m_secret_key;
    send_websocket_message(auth_msg.dump());
    
    return true;
}

void AlpacaClient::websocket_thread() {
    char buffer[65536];
    
    while (m_streaming && m_ws_socket != -1) {
        int bytes = recv(m_ws_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        
        buffer[bytes] = '\0';
        handle_websocket_message(std::string(buffer, bytes));
    }
}

void AlpacaClient::send_websocket_message(const std::string& message) {
    if (m_ws_socket == -1) return;
    
    // WebSocket frame header
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // Text frame
    
    size_t len = message.length();
    if (len <= 125) {
        frame.push_back(0x80 | len);
    } else if (len <= 65535) {
        frame.push_back(0x80 | 126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    }
    
    // Masking key (client to server must be masked)
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    frame.push_back(mask[0]);
    frame.push_back(mask[1]);
    frame.push_back(mask[2]);
    frame.push_back(mask[3]);
    
    // Masked payload
    for (size_t i = 0; i < len; i++) {
        frame.push_back(message[i] ^ mask[i % 4]);
    }
    
    send(m_ws_socket, (char*)frame.data(), frame.size(), 0);
}

void AlpacaClient::handle_websocket_message(const std::string& message) {
    // Parse WebSocket frame
    if (message.empty() || message[0] != 0x81) return;
    
    size_t pos = 2;
    size_t payload_len = message[1] & 0x7F;
    
    if (payload_len == 126) {
        payload_len = (static_cast<unsigned char>(message[2]) << 8) | static_cast<unsigned char>(message[3]);
        pos = 4;
    }
    
    // Unmask payload
    std::string unmasked;
    for (size_t i = 0; i < payload_len; i++) {
        unmasked += message[pos + i] ^ message[pos - 4 + (i % 4)];
    }
    
    try {
        json data = json::parse(unmasked);
        
        if (data.is_array()) {
            for (const auto& msg : data) {
                std::string stream = msg.value("T", "");
                
                if (stream == "t" && m_trade_callback) {
                    // Trade message
                    Tick tick;
                    tick.symbol = msg["S"];
                    tick.price = msg["p"];
                    tick.volume = msg["s"];
                    tick.timestamp = msg["t"];
                    tick.bid = tick.price * 0.9999;
                    tick.ask = tick.price * 1.0001;
                    
                    std::lock_guard<std::mutex> lock(m_callback_mutex);
                    m_trade_callback(tick);
                }
                else if (stream == "q" && m_quote_callback) {
                    // Quote message
                    Tick tick;
                    tick.symbol = msg["S"];
                    tick.bid = msg["bp"];
                    tick.ask = msg["ap"];
                    tick.bid_size = msg["bs"];
                    tick.ask_size = msg["as"];
                    tick.price = (tick.bid + tick.ask) / 2;
                    tick.timestamp = msg["t"];
                    
                    std::lock_guard<std::mutex> lock(m_callback_mutex);
                    m_quote_callback(tick);
                }
                else if (stream == "subscription") {
                    std::cout << "WebSocket subscribed to: ";
                    for (const auto& sub : msg["trades"]) std::cout << sub << " ";
                    std::cout << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "WebSocket parse error: " << e.what() << std::endl;
    }
}

void AlpacaClient::subscribe_trades(const std::vector<std::string>& symbols) {
    json msg;
    msg["action"] = "subscribe";
    msg["trades"] = symbols;
    send_websocket_message(msg.dump());
}

void AlpacaClient::subscribe_quotes(const std::vector<std::string>& symbols) {
    json msg;
    msg["action"] = "subscribe";
    msg["quotes"] = symbols;
    send_websocket_message(msg.dump());
}

void AlpacaClient::subscribe_bars(const std::vector<std::string>& symbols) {
    json msg;
    msg["action"] = "subscribe";
    msg["bars"] = symbols;
    send_websocket_message(msg.dump());
}

void AlpacaClient::set_trade_callback(std::function<void(const Tick&)> callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_trade_callback = callback;
}

void AlpacaClient::set_quote_callback(std::function<void(const Tick&)> callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_quote_callback = callback;
}

void AlpacaClient::poll_order_updates() {
    while (m_connected) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        
        if (elapsed - m_last_order_check >= 1) {  // Check every second
            auto orders = get_orders(true);
            
            std::lock_guard<std::mutex> lock(m_order_mutex);
            for (const auto& order : orders) {
                auto it = m_active_orders.find(order.alpaca_id);
                if (it == m_active_orders.end() || it->second.status != order.status) {
                    m_active_orders[order.alpaca_id] = order;
                    
                    if (order.is_filled()) {
                        std::cout << "✓ ORDER FILLED: " << order.symbol 
                                  << " " << (order.is_buy ? "BUY" : "SELL")
                                  << " Qty: " << order.filled_quantity
                                  << " @ $" << order.price << std::endl;
                    }
                }
            }
            
            m_last_order_check = elapsed;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void AlpacaClient::poll(std::function<void(const Tick&)> callback) {
    // Set up WebSocket callback
    set_trade_callback(callback);
    
    // Also subscribe to some default symbols if not already
    std::vector<std::string> default_symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
    subscribe_trades(default_symbols);
    subscribe_quotes(default_symbols);
    
    // Keep thread alive
    while (m_connected) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace alpaca