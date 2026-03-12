#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace ariane::graphql::internal {

template <typename T>
class Channel {
public:
    void Push(T value) {
        std::unique_lock lock(_mutex);
        if (_closed)
            return;
        _queue.push_back(std::move(value));
        _condition.notify_one();
    }

    void Close() {
        std::unique_lock lock(_mutex);
        _closed = true;
        _condition.notify_all();
    }

    std::optional<T> Next() {
        std::unique_lock lock(_mutex);
        _condition.wait(lock, [this] { return !_queue.empty() || _closed; });
        if (_queue.empty())
            return std::nullopt;
        T val = std::move(_queue.front());
        _queue.pop_front();
        return val;
    }
    
private:
    std::mutex _mutex;
    std::condition_variable _condition;
    std::deque<T> _queue;
    bool _closed = false;
};

}
