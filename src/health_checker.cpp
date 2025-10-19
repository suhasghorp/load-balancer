#include "health_checker.hpp"
#include "logger.hpp"
#include <httplib.h>
#include <spdlog/fmt/fmt.h>

namespace lb {

HealthChecker::HealthChecker(std::shared_ptr<BackendManager> backend_manager,
                            const HealthCheckConfig& config)
    : backend_manager_(backend_manager), config_(config) {}

HealthChecker::~HealthChecker() {
    stop();
}

void HealthChecker::start() {
    health_check_thread_ = std::jthread([this](std::stop_token stop_token) {
        health_check_loop(stop_token);
    });
}

void HealthChecker::stop() {
    if (health_check_thread_.joinable()) {
        health_check_thread_.request_stop();
    }
}

void HealthChecker::health_check_loop(std::stop_token stop_token) {
    Logger::info(Logger::Component::HealthCheck, "Health check thread started");

    while (!stop_token.stop_requested()) {
        Logger::debug(Logger::Component::HealthCheck, "Starting health check cycle");

        auto backends = backend_manager_->get_all_backends();

        for (size_t i = 0; i < backends.size(); ++i) {
            if (stop_token.stop_requested()) {
                break;
            }

            auto* backend = backends[i];
            bool was_healthy = backend->is_healthy.load();

            auto start = std::chrono::steady_clock::now();
            bool is_healthy = check_backend_health(backend);
            auto end = std::chrono::steady_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            // Update health status
            backend_manager_->update_health(i, is_healthy);

            // Log result
            if (is_healthy) {
                Logger::debug(Logger::Component::HealthCheck,
                    fmt::format("Backend {}: HEALTHY ({}ms)", backend->port, duration_ms));
            } else {
                Logger::error(Logger::Component::HealthCheck,
                    fmt::format("Backend {}: UNHEALTHY (timeout {}ms)", backend->port, duration_ms));
            }

            // Log state changes
            if (was_healthy && !is_healthy) {
                Logger::warn(Logger::Component::HealthCheck,
                    fmt::format("Backend {}: state changed HEALTHY → UNHEALTHY", backend->port));
            } else if (!was_healthy && is_healthy) {
                Logger::info(Logger::Component::HealthCheck,
                    fmt::format("Backend {}: state changed UNHEALTHY → HEALTHY", backend->port));
            }
        }

        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::seconds(config_.interval_seconds));
    }

    Logger::info(Logger::Component::HealthCheck, "Health check thread stopped");
}

bool HealthChecker::check_backend_health(const BackendServer* backend) {
    try {
        httplib::Client client(backend->host, backend->port);
        client.set_connection_timeout(0, config_.timeout_seconds * 1000000); // microseconds
        client.set_read_timeout(config_.timeout_seconds, 0); // seconds, microseconds

        auto res = client.Get(backend->health_endpoint);

        if (res && res->status == 200) {
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        Logger::debug(Logger::Component::HealthCheck,
            fmt::format("Backend {} health check exception: {}", backend->port, e.what()));
        return false;
    }
}

} // namespace lb
