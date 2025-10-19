#pragma once

#include "backend_manager.hpp"
#include "routing_policy.hpp"
#include <expected>
#include <string>
#include <memory>

namespace lb {

template<RoutingPolicy Policy>
class RequestRouter {
public:
    explicit RequestRouter(std::shared_ptr<BackendManager> backend_manager)
        : backend_manager_(backend_manager), policy_() {}

    std::expected<BackendServer*, std::string> select_backend() {
        // Get healthy backends
        auto healthy_backends = backend_manager_->get_healthy_backends();

        if (healthy_backends.empty()) {
            return std::unexpected("No healthy backends available");
        }

        // Use policy to select backend
        return policy_.select(healthy_backends);
    }

    Policy& get_policy() {
        return policy_;
    }

private:
    std::shared_ptr<BackendManager> backend_manager_;
    Policy policy_;
};

} // namespace lb
