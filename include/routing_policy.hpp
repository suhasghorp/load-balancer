#pragma once

#include "backend_manager.hpp"
#include <vector>
#include <atomic>
#include <expected>
#include <string>

namespace lb {

// Base concept for routing policies (C++20 concept)
template<typename T>
concept RoutingPolicy = requires(T policy, const std::vector<BackendServer*>& backends) {
    { policy.select(backends) } -> std::same_as<std::expected<BackendServer*, std::string>>;
};

// Round-robin policy implementation
class RoundRobinPolicy {
public:
    RoundRobinPolicy() : counter_(0) {}

    std::expected<BackendServer*, std::string> select(const std::vector<BackendServer*>& backends) {
        if (backends.empty()) {
            return std::unexpected("No healthy backends available");
        }

        // Get current index and increment atomically
        size_t index = counter_.fetch_add(1, std::memory_order_relaxed) % backends.size();
        return backends[index];
    }

    void reset() {
        counter_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<size_t> counter_;
};

// Random policy (future implementation)
class RandomPolicy {
public:
    std::expected<BackendServer*, std::string> select(const std::vector<BackendServer*>& backends);
};

// Least connections policy (future implementation)
class LeastConnectionsPolicy {
public:
    std::expected<BackendServer*, std::string> select(const std::vector<BackendServer*>& backends);
};

} // namespace lb
