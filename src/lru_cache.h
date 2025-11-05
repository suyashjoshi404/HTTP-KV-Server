#pragma once
#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>

template <typename K, typename V>
class LRUCache
{
public:
    LRUCache(size_t capacity) : cap(capacity) {}

    bool get(const K &key, V &value)
    {
        std::lock_guard<std::mutex> lock(mu);
        auto it = map.find(key);
        if (it == map.end())
            return false;
        // move to front
        lst.splice(lst.begin(), lst, it->second);
        value = it->second->second;
        return true;
    }

    void put(const K &key, const V &value)
    {
        std::lock_guard<std::mutex> lock(mu);
        auto it = map.find(key);
        if (it != map.end())
        {
            // update and move to front
            it->second->second = value;
            lst.splice(lst.begin(), lst, it->second);
            return;
        }
        if (lst.size() >= cap)
        {
            auto last = lst.back();
            map.erase(last.first);
            lst.pop_back();
        }
        lst.emplace_front(key, value);
        map[key] = lst.begin();
    }

    void remove(const K &key)
    {
        std::lock_guard<std::mutex> lock(mu);
        auto it = map.find(key);
        if (it == map.end())
            return;
        lst.erase(it->second);
        map.erase(it);
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(mu);
        return lst.size();
    }

private:
    size_t cap;
    std::list<std::pair<K, V>> lst;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map;
    std::mutex mu;
};
