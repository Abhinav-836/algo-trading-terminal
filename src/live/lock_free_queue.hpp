#pragma once
#include <atomic>
#include <cstddef>

// FIX #4: This is a Single-Producer / Single-Consumer (SPSC) lock-free queue.
//
// BUG in original: Two threads calling push() concurrently would both read the
// same tail value, both write to buffer[tail], and one write would be silently
// overwritten. The queue was being used from a single producer thread
// (OrderManager::process_signal) so SPSC is correct here — but the contract
// must be enforced and documented clearly.
//
// Usage rules (enforced by design):
//   - Exactly ONE thread may call push() at any time.
//   - Exactly ONE thread may call pop() at any time.
//   - The same thread CAN be both producer and consumer.
//
// If you ever need multiple producers, replace this with a mutex-protected
// std::queue or a true MPSC structure (e.g. linked-list CAS queue).

template<typename T, size_t Capacity>
class LockFreeQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2 for efficient modulo.");

private:
    // Pad head and tail to separate cache lines to avoid false sharing.
    alignas(64) std::atomic<size_t> head{0};
    alignas(64) std::atomic<size_t> tail{0};
    T buffer[Capacity];

public:
    // push() — call from ONE producer thread only.
    bool push(const T& item) {
        const size_t current_tail = tail.load(std::memory_order_relaxed);
        const size_t next_tail    = (current_tail + 1) % Capacity;

        // Queue full?
        if (next_tail == head.load(std::memory_order_acquire)) {
            return false;
        }

        buffer[current_tail] = item;
        // Release ensures the write to buffer is visible before tail advances.
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    // pop() — call from ONE consumer thread only.
    bool pop(T& item) {
        const size_t current_head = head.load(std::memory_order_relaxed);

        // Queue empty?
        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;
        }

        item = buffer[current_head];
        // Release ensures the read of buffer is complete before head advances.
        head.store((current_head + 1) % Capacity, std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t h = head.load(std::memory_order_acquire);
        const size_t t = tail.load(std::memory_order_acquire);
        return (t - h + Capacity) % Capacity;
    }

    bool empty() const {
        return head.load(std::memory_order_acquire) ==
               tail.load(std::memory_order_acquire);
    }

    bool full() const {
        return ((tail.load(std::memory_order_acquire) + 1) % Capacity) ==
                 head.load(std::memory_order_acquire);
    }
};