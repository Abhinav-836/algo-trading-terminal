#pragma once
#include "../core/types.hpp"
#include "../exchange/adapter.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <iomanip>

class OrderManager {
private:
    ExchangeAdapter& m_exchange;
    
    struct PendingOrder {
        Order order;
        uint64_t timestamp;
        std::string status;
    };
    
    std::queue<PendingOrder> m_order_queue;
    std::unordered_map<std::string, Order> m_active_orders;
    std::mutex m_queue_mutex;
    std::mutex m_orders_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running{false};
    std::thread m_processor_thread;
    std::thread m_status_thread;
    
    std::atomic<uint64_t> m_total_orders{0};
    std::atomic<uint64_t> m_filled_orders{0};
    std::atomic<uint64_t> m_rejected_orders{0};
    std::atomic<uint64_t> m_total_latency_us{0};
    
public:
    OrderManager(ExchangeAdapter& exchange) : m_exchange(exchange) {}
    
    ~OrderManager() { stop(); }
    
    void start() {
        m_running = true;
        m_processor_thread = std::thread(&OrderManager::process_orders, this);
        m_status_thread = std::thread(&OrderManager::check_order_status, this);
        std::cout << "[OrderManager] Started" << std::endl;
    }
    
    void stop() {
        m_running = false;
        m_cv.notify_all();
        if (m_processor_thread.joinable()) m_processor_thread.join();
        if (m_status_thread.joinable()) m_status_thread.join();
        std::cout << "[OrderManager] Stopped" << std::endl;
    }
    
    void process_signal(const Signal& signal) {
        if (!signal.valid) return;
        
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        
        for (const auto& order : signal.orders) {
            PendingOrder pending;
            pending.order = order;
            pending.order.client_order_id = generate_client_order_id();
            pending.timestamp = timestamp;
            pending.status = "pending";
            
            {
                std::lock_guard<std::mutex> lock(m_queue_mutex);
                m_order_queue.push(pending);
                m_total_orders++;
            }
            
            std::cout << "[OrderManager] Queued " << (order.is_buy ? "BUY" : "SELL")
                      << " " << order.quantity << " " << order.symbol
                      << " | Confidence: " << (signal.confidence * 100) << "%"
                      << " | Reason: " << signal.reason << std::endl;
        }
        
        m_cv.notify_one();
    }
    
    void print_metrics() {
        uint64_t filled = m_filled_orders.load();
        uint64_t total = m_total_orders.load();
        double fill_rate = total > 0 ? (double)filled / total * 100 : 0;
        double avg_latency = filled > 0 ? (double)m_total_latency_us.load() / filled : 0;
        
        std::cout << "\n[OrderManager Metrics]\n";
        std::cout << "  Total Orders: " << total << "\n";
        std::cout << "  Filled: " << filled << "\n";
        std::cout << "  Rejected: " << m_rejected_orders.load() << "\n";
        std::cout << "  Fill Rate: " << std::fixed << std::setprecision(1) << fill_rate << "%\n";
        std::cout << "  Avg Latency: " << avg_latency << " μs\n";
    }
    
private:
    void process_orders() {
        while (m_running) {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_cv.wait(lock, [this] { return !m_order_queue.empty() || !m_running; });
            
            while (!m_order_queue.empty() && m_running) {
                auto pending = m_order_queue.front();
                m_order_queue.pop();
                lock.unlock();
                
                auto start = std::chrono::high_resolution_clock::now();
                std::string order_id = m_exchange.place_order(pending.order);
                auto end = std::chrono::high_resolution_clock::now();
                uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                m_total_latency_us += latency;
                
                if (!order_id.empty()) {
                    Order submitted = pending.order;
                    submitted.alpaca_id = order_id;
                    submitted.status = "submitted";
                    
                    {
                        std::lock_guard<std::mutex> order_lock(m_orders_mutex);
                        m_active_orders[order_id] = submitted;
                    }
                    
                    std::cout << "  ✓ Order sent: " << order_id << " | Latency: " << latency << " μs" << std::endl;
                } else {
                    m_rejected_orders++;
                    std::cerr << "  ✗ Order failed" << std::endl;
                }
                
                lock.lock();
            }
        }
    }
    
    void check_order_status() {
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // Status checking simplified for now
        }
    }
    
    std::string generate_client_order_id() {
        static std::atomic<uint64_t> counter{0};
        return "client_" + std::to_string(++counter);
    }
};