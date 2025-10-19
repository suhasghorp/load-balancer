#include "config_loader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>


using json = nlohmann::json;

namespace lb {

std::expected<Config, std::string> ConfigLoader::load(const std::string& config_path) {
    // Try multiple locations for the config file
    std::vector<std::string> search_paths = {
        config_path,                           // Current directory
        "../" + config_path,                   // Parent directory (for build dirs)
        "../../" + config_path                 // Two levels up (for nested builds)
    };

    std::ifstream file;
    std::string found_path;

    for (const auto& path : search_paths) {
        file.open(path);
        if (file.is_open()) {
            found_path = path;
            break;
        }
        file.clear(); // Clear error flags before next attempt
    }

    if (!file.is_open()) {
        return std::unexpected("Failed to open config file: " + config_path +
                             " (searched in: ., .., ../..)");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return parse_config(content);
}

std::expected<Config, std::string> ConfigLoader::parse_config(const std::string& content) {
    try {
        json j = json::parse(content);

        Config config;

        // Parse load_balancer section
        if (!j.contains("load_balancer")) {
            return std::unexpected("Missing 'load_balancer' section");
        }
        auto& lb = j["load_balancer"];
        config.load_balancer.port = lb.value("port", 8000);
        config.load_balancer.log_file = lb.value("log_file", "lb.log");
        config.load_balancer.log_level = lb.value("log_level", "INFO");

        // Parse backends section
        if (!j.contains("backends")) {
            return std::unexpected("Missing 'backends' section");
        }
        for (const auto& backend : j["backends"]) {
            BackendConfig bc;
            bc.host = backend.value("host", "localhost");
            bc.port = backend.value("port", 8080);
            bc.health_endpoint = backend.value("health_endpoint", "/health");
            config.backends.push_back(bc);
        }

        // Parse health_check section
        if (!j.contains("health_check")) {
            return std::unexpected("Missing 'health_check' section");
        }
        auto& hc = j["health_check"];
        config.health_check.interval_seconds = hc.value("interval_seconds", 1);
        config.health_check.timeout_seconds = hc.value("timeout_seconds", 1);

        // Parse algorithm
        config.algorithm = j.value("algorithm", "round-robin");

        // Validate
        if (!validate_config(config)) {
            return std::unexpected("Configuration validation failed");
        }

        return config;

    } catch (const json::exception& e) {
        return std::unexpected(std::string("JSON parsing error: ") + e.what());
    }
}

bool ConfigLoader::validate_config(const Config& config) {
    if (config.backends.empty()) {
        return false;
    }

    if (config.health_check.interval_seconds <= 0) {
        return false;
    }

    if (config.health_check.timeout_seconds <= 0) {
        return false;
    }

    return true;
}

} // namespace lb
