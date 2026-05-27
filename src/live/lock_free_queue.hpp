#pragma once
#include <atomic>
#include <cstddef>

template<typename T, size_t Capacity>
class LockFreeQueue {
private:
    alignas(64) std::atomic<size_t> head{0};
    alignas(64) std::atomic<size_t> tail{0};
    T buffer[Capacity];
    
public:
    bool push(const T& item) {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % Capacity;
        
        if (next_tail == head.load(std::memory_order_acquire)) {
            return false;
        }
        
        buffer[current_tail] = item;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) {
        size_t current_head = head.load(std::memory_order_relaxed);
        
        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;
        }
        
        item = buffer[current_head];
        head.store((current_head + 1) % Capacity, std::memory_order_release);
        return true;
    }
    
    size_t size() const {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        if (t >= h) return t - h;
        return Capacity - (h - t);
    }
    
    bool empty() const { return head.load() == tail.load(); }
    bool full() const { return ((tail.load() + 1) % Capacity) == head.load(); }
};