#include "logger.hpp"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/fmt.h>
#include <iostream>
#include <filesystem>

namespace lb {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::init(const std::string& log_file, const std::string& log_level,
                 bool is_backend, int backend_port) {
    try {
        // Create sinks
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        // Rotating file sink with appropriate size limits
        size_t max_size = is_backend ? 5 * 1024 * 1024 : 10 * 1024 * 1024;  // 5MB or 10MB
        size_t max_files = is_backend ? 3 : 5;

        std::string actual_log_file = log_file;
        if (is_backend && backend_port > 0) {
            actual_log_file = fmt::format("logs/backend_{}.log", backend_port);
        }

        // Try to find the correct path for logs (search parent dirs)
        std::vector<std::string> search_paths;
        if (actual_log_file.find("logs/") == 0) {
            search_paths = {
                actual_log_file,
                "../" + actual_log_file,
                "../../" + actual_log_file
            };
        } else {
            search_paths = {actual_log_file};
        }

        // Find the first path where the logs directory exists or can be created
        std::string final_log_path = actual_log_file;
        for (const auto& path : search_paths) {
            std::filesystem::path log_path(path);
            std::filesystem::path log_dir = log_path.parent_path();

            // If parent directory exists or we can create it, use this path
            if (log_dir.empty() || std::filesystem::exists(log_dir)) {
                final_log_path = path;
                break;
            }

            // Try to create the directory
            std::error_code ec;
            if (std::filesystem::create_directories(log_dir, ec)) {
                final_log_path = path;
                break;
            }
        }

        actual_log_file = final_log_path;

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            actual_log_file, max_size, max_files);
        file_sink->set_level(string_to_level(log_level));

        // Combine sinks
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        logger_ = std::make_shared<spdlog::logger>("logger", sinks.begin(), sinks.end());

        logger_->set_level(string_to_level(log_level));
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        logger_->flush_on(spdlog::level::info);

        spdlog::register_logger(logger_);

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void Logger::shutdown() {
    if (logger_) {
        logger_->flush();
        spdlog::drop_all();
    }
}

void Logger::info(Component component, const std::string& message) {
    if (logger_) {
        logger_->info("[{}] {}", component_to_string(component), message);
    }
}

void Logger::warn(Component component, const std::string& message) {
    if (logger_) {
        logger_->warn("[{}] {}", component_to_string(component), message);
    }
}

void Logger::error(Component component, const std::string& message) {
    if (logger_) {
        logger_->error("[{}] {}", component_to_string(component), message);
    }
}

void Logger::debug(Component component, const std::string& message) {
    if (logger_) {
        logger_->debug("[{}] {}", component_to_string(component), message);
    }
}

std::string Logger::component_to_string(Component component) {
    switch (component) {
        case Component::LB: return "LB";
        case Component::Config: return "Config";
        case Component::HealthCheck: return "HealthCheck";
        case Component::Request: return "Request";
        case Component::Router: return "Router";
        case Component::Response: return "Response";
        case Component::Backend: return "Backend";
        default: return "Unknown";
    }
}

spdlog::level::level_enum Logger::string_to_level(const std::string& level) {
    if (level == "DEBUG") return spdlog::level::debug;
    if (level == "INFO") return spdlog::level::info;
    if (level == "WARN") return spdlog::level::warn;
    if (level == "ERROR") return spdlog::level::err;
    return spdlog::level::info;
}

} // namespace lb
