#include "backend_manager.hpp"
#include <algorithm>
#include <mutex>

namespace lb {

BackendManager::BackendManager(const std::vector<BackendConfig>& backend_configs) {
    backends_.reserve(backend_configs.size());
    for (const auto& config : backend_configs) {
        backends_.emplace_back(config.host, config.port, config.health_endpoint);
    }
}

std::vector<BackendServer*> BackendManager::get_all_backends() {
    std::shared_lock lock(mutex_);
    std::vector<BackendServer*> result;
    result.reserve(backends_.size());
    for (auto& backend : backends_) {
        result.push_back(&backend);
    }
    return result;
}

std::vector<BackendServer*> BackendManager::get_healthy_backends() {
    std::shared_lock lock(mutex_);
    std::vector<BackendServer*> result;
    for (auto& backend : backends_) {
        if (backend.is_healthy.load()) {
            result.push_back(&backend);
        }
    }
    return result;
}

void BackendManager::update_health(size_t index, bool is_healthy) {
    std::unique_lock lock(mutex_);
    if (index < backends_.size()) {
        backends_[index].is_healthy.store(is_healthy);
        backends_[index].last_check = std::chrono::steady_clock::now();
    }
}

size_t BackendManager::backend_count() const {
    std::shared_lock lock(mutex_);
    return backends_.size();
}

} // namespace lb
