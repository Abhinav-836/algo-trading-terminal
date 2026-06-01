#include "alpaca_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>
// FIX: removed #include <regex> — it was unused and caused namespace pollution
//      that broke std::match_results and related types globally.

// FIX: OpenSSL 3.0 deprecated the low-level SHA1() function.
// Use the EVP (high-level) API which works on both OpenSSL 1.x and 3.x.
#include <openssl/evp.h>

namespace alpaca {

// ── SHA-1 via EVP (OpenSSL 1.x and 3.x compatible) ───────────────────────────
static std::string sha1_hash(const std::string& input) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int  hash_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";

    if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) &&
        EVP_DigestUpdate(ctx, input.data(), input.size()) &&
        EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        EVP_MD_CTX_free(ctx);
        return std::string(reinterpret_cast<char*>(hash), hash_len);
    }

    EVP_MD_CTX_free(ctx);
    return "";
}

// ── Base64 encode ─────────────────────────────────────────────────────────────
static std::string base64_encode(const std::string& input) {
    static const char* chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int i = 0;
    unsigned char a3[3], a4[4];

    for (char c : input) {
        a3[i++] = static_cast<unsigned char>(c);
        if (i == 3) {
            a4[0] = (a3[0] & 0xfc) >> 2;
            a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
            a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
            a4[3] =   a3[2] & 0x3f;
            for (int j = 0; j < 4; j++) out += chars[a4[j]];
            i = 0;
        }
    }
    if (i) {
        for (int j = i; j < 3; j++) a3[j] = '\0';
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        for (int j = 0; j < i + 1; j++) out += chars[a4[j]];
        while (i++ < 3) out += '=';
    }
    return out;
}

// RFC 6455 Sec-WebSocket-Accept derivation
static std::string websocket_accept_key(const std::string& client_key) {
    static const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    return base64_encode(sha1_hash(client_key + GUID));
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
AlpacaClient::AlpacaClient(const std::string& api_key,
                             const std::string& secret_key,
                             Mode mode)
    : m_api_key(api_key), m_secret_key(secret_key), m_mode(mode)
{
    if (mode == Mode::PAPER) {
        m_base_url = "https://paper-api.alpaca.markets";
        m_data_url = "https://data.alpaca.markets";
        m_ws_url   = "wss://stream.data.alpaca.markets/v2/iex";
    } else {
        m_base_url = "https://api.alpaca.markets";
        m_data_url = "https://data.alpaca.markets";
        m_ws_url   = "wss://stream.data.alpaca.markets/v2/sip";
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_curl = curl_easy_init();
}

AlpacaClient::~AlpacaClient() {
    disconnect();
    if (m_curl) curl_easy_cleanup(m_curl);
    curl_global_cleanup();
}

// ── CURL helpers ──────────────────────────────────────────────────────────────
size_t AlpacaClient::curl_write_callback(void* contents, size_t size,
                                          size_t nmemb, std::string* response) {
    size_t total = size * nmemb;
    response->append(static_cast<char*>(contents), total);
    return total;
}

std::string AlpacaClient::send_request(const std::string& endpoint,
                                        const std::string& method,
                                        const std::string& body) {
    if (!m_curl) return "";

    // Accept full URLs (e.g. data_url endpoints) or append to base_url
    std::string url = (endpoint.rfind("https://", 0) == 0)
                          ? endpoint
                          : m_base_url + endpoint;
    std::string response;

    curl_easy_setopt(m_curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION,  curl_write_callback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT,        10L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("APCA-API-KEY-ID: "     + m_api_key).c_str());
    headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + m_secret_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);

    if      (method == "POST")   { curl_easy_setopt(m_curl, CURLOPT_POST,            1L);
                                   curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS,      body.c_str()); }
    else if (method == "DELETE") { curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST,   "DELETE"); }
    else if (method == "PUT")    { curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST,   "PUT");
                                   curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS,      body.c_str()); }
    else                         { curl_easy_setopt(m_curl, CURLOPT_HTTPGET,         1L); }

    CURLcode res = curl_easy_perform(m_curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << "\n";
        return "";
    }

    long http_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 300) {
        std::cerr << "HTTP " << http_code << ": " << response << "\n";
        return "";
    }
    return response;
}

json AlpacaClient::authenticated_request(const std::string& endpoint,
                                          const std::string& method,
                                          const std::string& body) {
    std::string resp = send_request(endpoint, method, body);
    if (resp.empty()) return json::object();
    try   { return json::parse(resp); }
    catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return json::object();
    }
}

// ── Connection ────────────────────────────────────────────────────────────────
bool AlpacaClient::connect() {
    std::cout << "Connecting to Alpaca "
              << (m_mode == Mode::PAPER ? "PAPER" : "LIVE") << " API...\n";

    AccountInfo account = get_account();
    if (account.id.empty()) {
        std::cerr << "Failed to connect — check API keys.\n";
        return false;
    }

    m_connected = true;
    std::cout << "✓ Connected | Account: " << account.id
              << " | Cash: $" << std::fixed << std::setprecision(2) << account.cash
              << " | Equity: $" << account.equity << "\n";

    m_poll_thread = std::thread(&AlpacaClient::poll_order_updates, this);

    if (connect_websocket())
        std::cout << "✓ WebSocket connected for real-time data\n";
    else
        std::cerr << "⚠ WebSocket failed — continuing with REST polling only.\n";

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

    if (m_ws_thread.joinable())   m_ws_thread.join();
    if (m_poll_thread.joinable()) m_poll_thread.join();
}

// ── Account ───────────────────────────────────────────────────────────────────
AccountInfo AlpacaClient::get_account() {
    json resp = authenticated_request("/v2/account");
    if (resp.empty()) return AccountInfo{};

    AccountInfo a;
    a.id                 = resp.value("id",                 "");
    a.status             = resp.value("status",             "");
    a.cash               = std::stod(resp.value("cash",               "0"));
    a.buying_power       = std::stod(resp.value("buying_power",       "0"));
    a.portfolio_value    = std::stod(resp.value("portfolio_value",    "0"));
    a.long_market_value  = std::stod(resp.value("long_market_value",  "0"));
    a.short_market_value = std::stod(resp.value("short_market_value", "0"));
    a.equity             = std::stod(resp.value("equity",             "0"));
    a.last_equity        = std::stod(resp.value("last_equity",        "0"));
    a.initial_margin     = std::stod(resp.value("initial_margin",     "0"));
    a.maintenance_margin = std::stod(resp.value("maintenance_margin", "0"));
    return a;
}

double AlpacaClient::get_cash_balance()  { return get_account().cash;          }
double AlpacaClient::get_buying_power()  { return get_account().buying_power;  }

// ── Orders ────────────────────────────────────────────────────────────────────
std::string AlpacaClient::place_order(const Order& order) {
    json body;
    body["symbol"]        = order.symbol;
    body["qty"]           = std::to_string(static_cast<int>(order.quantity));
    body["side"]          = order.is_buy ? "buy" : "sell";
    body["type"]          = order.type;
    body["time_in_force"] = order.tif;

    if (order.is_limit() && order.price > 0)
        body["limit_price"] = std::to_string(order.price);
    if (!order.client_order_id.empty())
        body["client_order_id"] = order.client_order_id;

    json resp = authenticated_request("/v2/orders", "POST", body.dump());
    return resp.value("id", "");
}

bool AlpacaClient::cancel_order(const std::string& order_id) {
    json resp = authenticated_request("/v2/orders/" + order_id, "DELETE");
    return !resp.empty() || resp.is_null();
}

std::vector<Order> AlpacaClient::get_orders(bool open_only) {
    std::string ep = "/v2/orders";
    if (open_only) ep += "?status=open";

    json resp = authenticated_request(ep);
    if (!resp.is_array()) return {};

    std::vector<Order> orders;
    for (const auto& item : resp) {
        Order o;
        o.alpaca_id       = item.value("id",               "");
        o.client_order_id = item.value("client_order_id",  "");
        o.symbol          = item.value("symbol",           "");
        o.quantity        = std::stod(item.value("qty",         "0"));
        o.filled_quantity = std::stod(item.value("filled_qty",  "0"));
        o.is_buy          = item.value("side", "") == "buy";
        o.type            = item.value("type",             "market");
        o.tif             = item.value("time_in_force",    "day");
        o.status          = item.value("status",           "");
        if (item.contains("limit_price") && !item["limit_price"].is_null())
            o.price = std::stod(item["limit_price"].get<std::string>());
        orders.push_back(o);
    }
    return orders;
}

Order AlpacaClient::get_order(const std::string& order_id) {
    json resp = authenticated_request("/v2/orders/" + order_id);
    if (resp.empty()) return Order{};

    Order o;
    o.alpaca_id       = resp.value("id",              "");
    o.client_order_id = resp.value("client_order_id", "");
    o.symbol          = resp.value("symbol",          "");
    o.quantity        = std::stod(resp.value("qty",        "0"));
    o.filled_quantity = std::stod(resp.value("filled_qty", "0"));
    o.is_buy          = resp.value("side", "") == "buy";
    o.type            = resp.value("type",             "market");
    o.tif             = resp.value("time_in_force",    "day");
    o.status          = resp.value("status",           "");
    if (resp.contains("limit_price") && !resp["limit_price"].is_null())
        o.price = std::stod(resp["limit_price"].get<std::string>());
    return o;
}

// ── Positions ─────────────────────────────────────────────────────────────────
std::vector<Position> AlpacaClient::get_positions() {
    json resp = authenticated_request("/v2/positions");
    if (!resp.is_array()) return {};

    std::vector<Position> positions;
    for (const auto& item : resp) {
        Position p;
        p.symbol          = item.value("symbol",         "");
        p.quantity        = std::stod(item.value("qty",             "0"));
        p.avg_entry_price = std::stod(item.value("avg_entry_price", "0"));
        p.current_price   = std::stod(item.value("current_price",   "0"));
        p.unrealized_pl   = std::stod(item.value("unrealized_pl",   "0"));
        p.realized_pl     = std::stod(item.value("realized_pl",     "0"));
        p.market_value    = std::stod(item.value("market_value",    "0"));
        positions.push_back(p);
    }
    return positions;
}

Position AlpacaClient::get_position(const std::string& symbol) {
    json resp = authenticated_request("/v2/positions/" + symbol);
    if (resp.empty()) return Position{};

    Position p;
    p.symbol          = resp.value("symbol",         "");
    p.quantity        = std::stod(resp.value("qty",             "0"));
    p.avg_entry_price = std::stod(resp.value("avg_entry_price", "0"));
    p.current_price   = std::stod(resp.value("current_price",   "0"));
    p.unrealized_pl   = std::stod(resp.value("unrealized_pl",   "0"));
    p.realized_pl     = std::stod(resp.value("realized_pl",     "0"));
    p.market_value    = std::stod(resp.value("market_value",    "0"));
    return p;
}

bool AlpacaClient::close_position(const std::string& symbol) {
    json resp = authenticated_request("/v2/positions/" + symbol, "DELETE");
    return !resp.empty();
}

// ── Market data (REST) ────────────────────────────────────────────────────────
Tick AlpacaClient::get_latest_tick(const std::string& symbol) {
    std::string url = m_data_url + "/v2/stocks/" + symbol + "/trades/latest";
    std::string response = send_request(url);
    if (response.empty()) return Tick{};

    try {
        json j = json::parse(response);
        if (j.contains("trade")) {
            auto& t = j["trade"];
            Tick tick;
            tick.symbol    = symbol;
            tick.timestamp = static_cast<uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count());
            tick.price  = t["p"];
            tick.volume = t["s"];
            tick.bid    = tick.price * 0.9999;
            tick.ask    = tick.price * 1.0001;
            return tick;
        }
    } catch (...) {}
    return Tick{};
}

std::vector<Bar> AlpacaClient::get_bars(const std::string& symbol,
                                         const std::string& timeframe,
                                         int limit) {
    std::string url = m_data_url + "/v2/stocks/" + symbol
                    + "/bars?timeframe=" + timeframe
                    + "&limit=" + std::to_string(limit);
    std::string response = send_request(url);
    if (response.empty()) return {};

    try {
        json j = json::parse(response);
        std::vector<Bar> bars;
        if (j.contains("bars")) {
            for (const auto& b : j["bars"]) {
                Bar bar;
                bar.symbol      = symbol;
                bar.timestamp   = std::stoull(b.value("t", "0"));
                bar.open        = b["o"];
                bar.high        = b["h"];
                bar.low         = b["l"];
                bar.close       = b["c"];
                bar.volume      = b["v"];
                bar.vwap        = b["vw"];
                bar.trade_count = b["n"];
                bars.push_back(bar);
            }
        }
        return bars;
    } catch (...) {}
    return {};
}

// ── WebSocket ─────────────────────────────────────────────────────────────────
bool AlpacaClient::connect_websocket() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif

    m_ws_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_ws_socket < 0) return false;

    // Parse host from ws URL
    std::string host = m_ws_url.substr(m_ws_url.find("://") + 3);
    host = host.substr(0, host.find("/"));

    struct hostent* server = gethostbyname(host.c_str());
    if (!server) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(443);
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (::connect(m_ws_socket,
                  reinterpret_cast<struct sockaddr*>(&addr),
                  sizeof(addr)) < 0) {
        return false;
    }

    std::string client_key     = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string expected_accept = websocket_accept_key(client_key);

    std::string path = m_ws_url.substr(m_ws_url.find("://") + 3);
    path = path.substr(path.find("/"));

    std::string handshake =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + client_key + "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    send(m_ws_socket, handshake.c_str(), handshake.length(), 0);

    char buf[4096] = {};
    recv(m_ws_socket, buf, sizeof(buf) - 1, 0);

    if (std::string(buf).find("101") == std::string::npos) {
        std::cerr << "[WS] Handshake rejected\n";
        return false;
    }

    m_streaming = true;
    m_ws_thread = std::thread(&AlpacaClient::websocket_thread, this);

    json auth;
    auth["action"] = "auth";
    auth["key"]    = m_api_key;
    auth["secret"] = m_secret_key;
    send_websocket_message(auth.dump());

    return true;
}

void AlpacaClient::websocket_thread() {
    char buf[65536];
    while (m_streaming && m_ws_socket != -1) {
        int bytes = recv(m_ws_socket, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) break;
        buf[bytes] = '\0';
        handle_websocket_message(std::string(buf, bytes));
    }
}

void AlpacaClient::send_websocket_message(const std::string& message) {
    if (m_ws_socket == -1) return;

    std::vector<uint8_t> frame;
    frame.push_back(0x81);  // FIN + text opcode

    size_t len = message.size();
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(0x80 | len));
    } else if (len <= 65535) {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(0x80 | 127);
        for (int s = 56; s >= 0; s -= 8)
            frame.push_back(static_cast<uint8_t>((len >> s) & 0xFF));
    }

    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    frame.push_back(mask[0]); frame.push_back(mask[1]);
    frame.push_back(mask[2]); frame.push_back(mask[3]);

    for (size_t i = 0; i < len; i++)
        frame.push_back(static_cast<uint8_t>(message[i]) ^ mask[i % 4]);

    send(m_ws_socket, reinterpret_cast<char*>(frame.data()), frame.size(), 0);
}

void AlpacaClient::handle_websocket_message(const std::string& message) {
    if (message.size() < 2) return;

    uint8_t first = static_cast<uint8_t>(message[0]);
    if ((first & 0x0F) != 0x01 && (first & 0x0F) != 0x00) return;

    size_t  pos         = 2;
    size_t  payload_len = static_cast<uint8_t>(message[1]) & 0x7F;
    bool    is_masked   = (static_cast<uint8_t>(message[1]) & 0x80) != 0;

    if (payload_len == 126) {
        if (message.size() < 4) return;
        payload_len = (static_cast<uint8_t>(message[2]) << 8) |
                       static_cast<uint8_t>(message[3]);
        pos = 4;
    } else if (payload_len == 127) {
        if (message.size() < 10) return;
        payload_len = 0;
        for (int i = 2; i < 10; i++)
            payload_len = (payload_len << 8) | static_cast<uint8_t>(message[i]);
        pos = 10;
    }

    if (message.size() < pos + (is_masked ? 4u : 0u) + payload_len) return;

    std::string payload;
    if (is_masked) {
        uint8_t mk[4] = {
            static_cast<uint8_t>(message[pos]),
            static_cast<uint8_t>(message[pos+1]),
            static_cast<uint8_t>(message[pos+2]),
            static_cast<uint8_t>(message[pos+3])
        };
        pos += 4;
        payload.reserve(payload_len);
        for (size_t i = 0; i < payload_len; i++)
            payload += static_cast<char>(static_cast<uint8_t>(message[pos+i]) ^ mk[i%4]);
    } else {
        payload = message.substr(pos, payload_len);
    }

    try {
        json j = json::parse(payload);
        if (!j.is_array()) return;

        for (const auto& msg : j) {
            std::string T = msg.value("T", "");

            if (T == "t" && m_trade_callback) {
                Tick tick;
                tick.symbol    = msg["S"];
                tick.price     = msg["p"];
                tick.volume    = msg["s"];
                tick.timestamp = msg["t"];
                tick.bid       = tick.price * 0.9999;
                tick.ask       = tick.price * 1.0001;
                std::lock_guard<std::mutex> lock(m_callback_mutex);
                m_trade_callback(tick);
            }
            else if (T == "q" && m_quote_callback) {
                Tick tick;
                tick.symbol    = msg["S"];
                tick.bid       = msg["bp"];
                tick.ask       = msg["ap"];
                tick.bid_size  = msg["bs"];
                tick.ask_size  = msg["as"];
                tick.price     = (tick.bid + tick.ask) / 2.0;
                tick.timestamp = msg["t"];
                std::lock_guard<std::mutex> lock(m_callback_mutex);
                m_quote_callback(tick);
            }
            else if (T == "subscription") {
                std::cout << "[WS] Subscribed\n";
            }
            else if (T == "error") {
                std::cerr << "[WS] Error: " << msg.dump() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[WS] Parse error: " << e.what() << "\n";
    }
}

void AlpacaClient::subscribe_trades(const std::vector<std::string>& symbols) {
    json msg; msg["action"] = "subscribe"; msg["trades"] = symbols;
    send_websocket_message(msg.dump());
}
void AlpacaClient::subscribe_quotes(const std::vector<std::string>& symbols) {
    json msg; msg["action"] = "subscribe"; msg["quotes"] = symbols;
    send_websocket_message(msg.dump());
}
void AlpacaClient::subscribe_bars(const std::vector<std::string>& symbols) {
    json msg; msg["action"] = "subscribe"; msg["bars"] = symbols;
    send_websocket_message(msg.dump());
}

void AlpacaClient::set_trade_callback(std::function<void(const Tick&)> cb) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_trade_callback = cb;
}
void AlpacaClient::set_quote_callback(std::function<void(const Tick&)> cb) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_quote_callback = cb;
}

// ── Order polling ─────────────────────────────────────────────────────────────
void AlpacaClient::poll_order_updates() {
    while (m_connected) {
        auto now     = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           now.time_since_epoch()).count();

        if (elapsed - static_cast<long long>(m_last_order_check.load()) >= 1) {
            auto orders = get_orders(true);
            std::lock_guard<std::mutex> lock(m_order_mutex);
            for (const auto& order : orders) {
                auto it = m_active_orders.find(order.alpaca_id);
                if (it == m_active_orders.end() || it->second.status != order.status) {
                    m_active_orders[order.alpaca_id] = order;
                    if (order.is_filled()) {
                        std::cout << "✓ FILLED: " << order.symbol
                                  << " " << (order.is_buy ? "BUY" : "SELL")
                                  << " x" << order.filled_quantity
                                  << " @ $" << order.price << "\n";
                    }
                }
            }
            m_last_order_check = static_cast<uint64_t>(elapsed);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void AlpacaClient::update_order_status(const json& /*order_data*/) {
    // Reserved for WebSocket order stream integration
}

void AlpacaClient::poll(std::function<void(const Tick&)> callback) {
    set_trade_callback(callback);
    while (m_connected)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

} // namespace alpaca