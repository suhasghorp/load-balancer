#pragma once

#include <string>
#include <vector>
#include <expected>
#include <cstdint>

namespace lb {

struct BackendConfig {
    std::string host;
    uint16_t port;
    std::string health_endpoint;
};

struct LoadBalancerConfig {
    uint16_t port;
    std::string log_file;
    std::string log_level;
};

struct HealthCheckConfig {
    int interval_seconds;
    int timeout_seconds;
};

struct Config {
    LoadBalancerConfig load_balancer;
    std::vector<BackendConfig> backends;
    HealthCheckConfig health_check;
    std::string algorithm;
};

class ConfigLoader {
public:
    static std::expected<Config, std::string> load(const std::string& config_path);

private:
    static std::expected<Config, std::string> parse_config(const std::string& content);
    static bool validate_config(const Config& config);
};

} // namespace lb
