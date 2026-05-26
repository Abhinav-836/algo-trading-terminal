#pragma once
#include <unordered_map>
#include <string>
#include <mutex>
#include <list>
#include <vector>
#include "../core/types.hpp"

template<typename Key, typename Value>
class LRUCache {
private:
    size_t capacity;
    std::list<std::pair<Key, Value>> cache_list;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map;
    mutable std::mutex mutex;
    
public:
    LRUCache(size_t cap = 1000) : capacity(cap) {}
    
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            cache_list.erase(it->second);
            cache_map.erase(it);
        }
        
        cache_list.push_front({key, value});
        cache_map[key] = cache_list.begin();
        
        if (cache_map.size() > capacity) {
            auto last = cache_list.end();
            --last;
            cache_map.erase(last->first);
            cache_list.pop_back();
        }
    }
    
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return false;
        }
        
        cache_list.splice(cache_list.begin(), cache_list, it->second);
        value = it->second->second;
        return true;
    }
    
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex);
        return cache_map.find(key) != cache_map.end();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        cache_list.clear();
        cache_map.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return cache_map.size();
    }
    
    std::vector<Key> get_keys() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<Key> keys;
        for (const auto& item : cache_list) {
            keys.push_back(item.first);
        }
        return keys;
    }
};

class MarketDataCache {
private:
    LRUCache<std::string, Tick> tick_cache{10000};
    
public:
    void cache_tick(const Tick& tick) {
        tick_cache.put(tick.symbol, tick);
    }
    
    bool get_last_tick(const std::string& symbol, Tick& tick) {
        return tick_cache.get(symbol, tick);
    }
    
    void clear() { tick_cache.clear(); }
    size_t size() const { return tick_cache.size(); }
};