#pragma once

#include "config_loader.hpp"
#include <vector>
#include <atomic>
#include <shared_mutex>
#include <chrono>
#include <string>

namespace lb {

struct BackendServer {
    std::string host;
    uint16_t port;
    std::string health_endpoint;
    std::atomic<bool> is_healthy;
    std::chrono::steady_clock::time_point last_check;

    BackendServer(const std::string& h, uint16_t p, const std::string& ep)
        : host(h), port(p), health_endpoint(ep), is_healthy(true),
          last_check(std::chrono::steady_clock::now()) {}

    // Delete copy constructor and assignment to prevent issues with atomic
    BackendServer(const BackendServer&) = delete;
    BackendServer& operator=(const BackendServer&) = delete;

    // Allow move operations
    BackendServer(BackendServer&& other) noexcept
        : host(std::move(other.host)),
          port(other.port),
          health_endpoint(std::move(other.health_endpoint)),
          is_healthy(other.is_healthy.load()),
          last_check(other.last_check) {}

    BackendServer& operator=(BackendServer&& other) noexcept {
        if (this != &other) {
            host = std::move(other.host);
            port = other.port;
            health_endpoint = std::move(other.health_endpoint);
            is_healthy.store(other.is_healthy.load());
            last_check = other.last_check;
        }
        return *this;
    }
};

class BackendManager {
public:
    explicit BackendManager(const std::vector<BackendConfig>& backend_configs);

    // Get all backends (read lock)
    std::vector<BackendServer*> get_all_backends();

    // Get only healthy backends (read lock)
    std::vector<BackendServer*> get_healthy_backends();

    // Update health status of a backend (write lock)
    void update_health(size_t index, bool is_healthy);

    // Get backend count
    size_t backend_count() const;

private:
    std::vector<BackendServer> backends_;
    mutable std::shared_mutex mutex_;
};

} // namespace lb
