#pragma once

#include "backend_manager.hpp"
#include "config_loader.hpp"
#include <thread>
#include <stop_token>
#include <memory>
#include <chrono>

namespace lb {

class HealthChecker {
public:
    HealthChecker(std::shared_ptr<BackendManager> backend_manager,
                 const HealthCheckConfig& config);

    ~HealthChecker();

    // Start health checking in background thread
    void start();

    // Stop health checking gracefully
    void stop();

private:
    void health_check_loop(std::stop_token stop_token);
    bool check_backend_health(const BackendServer* backend);

    std::shared_ptr<BackendManager> backend_manager_;
    HealthCheckConfig config_;
    std::jthread health_check_thread_;
};

} // namespace lb
