#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace lb {

class Logger {
public:
    enum class Component {
        LB,
        Config,
        HealthCheck,
        Request,
        Router,
        Response,
        Backend
    };

    static void init(const std::string& log_file, const std::string& log_level,
                    bool is_backend = false, int backend_port = 0);

    static void shutdown();

    // Logging methods with component
    static void info(Component component, const std::string& message);
    static void warn(Component component, const std::string& message);
    static void error(Component component, const std::string& message);
    static void debug(Component component, const std::string& message);

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::string component_to_string(Component component);
    static spdlog::level::level_enum string_to_level(const std::string& level);
};

} // namespace lb
