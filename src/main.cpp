#include "config_loader.hpp"
#include "logger.hpp"
#include "backend_manager.hpp"
#include "health_checker.hpp"
#include "request_router.hpp"
#include "routing_policy.hpp"
#include "response_injector.hpp"
#include <httplib.h>
#include <csignal>
#include <atomic>
#include <spdlog/fmt/fmt.h>
#include <iostream>

using namespace lb;

std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        shutdown_requested.store(true);
    }
}

int main() {
    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Load configuration
    auto config_result = ConfigLoader::load("config.json");
    if (!config_result.has_value()) {
        std::cerr << "Failed to load configuration: " << config_result.error() << std::endl;
        return 1;
    }

    Config config = config_result.value();

    // Initialize logger
    Logger::init(config.load_balancer.log_file, config.load_balancer.log_level);
    Logger::info(Logger::Component::Config,
        fmt::format("Loaded {} backends: ports {}-{}",
            config.backends.size(),
            config.backends.front().port,
            config.backends.back().port));

    // Create backend manager
    auto backend_manager = std::make_shared<BackendManager>(config.backends);

    // Create health checker
    HealthChecker health_checker(backend_manager, config.health_check);
    health_checker.start();

    // Create request router with round-robin policy
    RequestRouter<RoundRobinPolicy> router(backend_manager);

    // Create HTTP server
    httplib::Server server;

    // Configure server timeout
    server.set_read_timeout(5, 0);  // 5 seconds
    server.set_write_timeout(5, 0);

    // Handle all requests
    server.set_mount_point("/", ".");

    server.Get(".*", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        std::string client_ip = req.remote_addr;

        Logger::info(Logger::Component::Request,
            fmt::format("Client {} → {} {}", client_ip, req.method, req.path));

        // Select backend
        auto backend_result = router.select_backend();
        if (!backend_result.has_value()) {
            Logger::error(Logger::Component::Router, backend_result.error());
            res.status = 503;
            res.set_content(R"({"error": "No healthy backends available"})", "application/json");
            return;
        }

        BackendServer* backend = backend_result.value();
        Logger::info(Logger::Component::Router,
            fmt::format("Selected backend: {}", backend->port));

        // Forward request to backend
        try {
            httplib::Client client(backend->host, backend->port);
            client.set_connection_timeout(0, 5000000); // 5 seconds in microseconds
            client.set_read_timeout(5, 0);

            auto backend_res = client.Get(req.path);

            if (backend_res) {
                // Get content type
                std::string content_type = "text/plain";
                auto ct_it = backend_res->headers.find("Content-Type");
                if (ct_it != backend_res->headers.end()) {
                    content_type = ct_it->second;
                }

                // Inject backend port info
                std::string injected_body = ResponseInjector::inject(
                    backend_res->body, content_type, backend->port);

                // Set response
                res.status = backend_res->status;
                res.body = injected_body;
                res.headers = backend_res->headers;

                // Update Content-Length if body was modified
                if (injected_body != backend_res->body) {
                    res.set_header("Content-Length", std::to_string(injected_body.size()));
                }

                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count();

                Logger::info(Logger::Component::Response,
                    fmt::format("{} → Client ({}ms) via backend {}",
                        backend_res->status, duration_ms, backend->port));

            } else {
                Logger::error(Logger::Component::Router,
                    fmt::format("Backend {} connection failure", backend->port));
                res.status = 503;
                res.set_content(R"({"error": "Backend connection failed"})", "application/json");
            }

        } catch (const std::exception& e) {
            Logger::error(Logger::Component::Router,
                fmt::format("Exception forwarding to backend {}: {}", backend->port, e.what()));
            res.status = 503;
            res.set_content(R"({"error": "Backend request failed"})", "application/json");
        }
    });

    // Handle POST, PUT, DELETE, etc.
    server.Post(".*", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        std::string client_ip = req.remote_addr;

        Logger::info(Logger::Component::Request,
            fmt::format("Client {} → {} {}", client_ip, req.method, req.path));

        auto backend_result = router.select_backend();
        if (!backend_result.has_value()) {
            Logger::error(Logger::Component::Router, backend_result.error());
            res.status = 503;
            res.set_content(R"({"error": "No healthy backends available"})", "application/json");
            return;
        }

        BackendServer* backend = backend_result.value();
        Logger::info(Logger::Component::Router,
            fmt::format("Selected backend: {}", backend->port));

        try {
            httplib::Client client(backend->host, backend->port);
            client.set_connection_timeout(0, 5000000);
            client.set_read_timeout(5, 0);

            auto backend_res = client.Post(req.path, req.body, req.get_header_value("Content-Type"));

            if (backend_res) {
                std::string content_type = "text/plain";
                auto ct_it = backend_res->headers.find("Content-Type");
                if (ct_it != backend_res->headers.end()) {
                    content_type = ct_it->second;
                }

                std::string injected_body = ResponseInjector::inject(
                    backend_res->body, content_type, backend->port);

                res.status = backend_res->status;
                res.body = injected_body;
                res.headers = backend_res->headers;

                if (injected_body != backend_res->body) {
                    res.set_header("Content-Length", std::to_string(injected_body.size()));
                }

                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count();

                Logger::info(Logger::Component::Response,
                    fmt::format("{} → Client ({}ms) via backend {}",
                        backend_res->status, duration_ms, backend->port));
            } else {
                Logger::error(Logger::Component::Router,
                    fmt::format("Backend {} connection failure", backend->port));
                res.status = 503;
                res.set_content(R"({"error": "Backend connection failed"})", "application/json");
            }
        } catch (const std::exception& e) {
            Logger::error(Logger::Component::Router,
                fmt::format("Exception forwarding to backend {}: {}", backend->port, e.what()));
            res.status = 503;
            res.set_content(R"({"error": "Backend request failed"})", "application/json");
        }
    });

    // Start server
    Logger::info(Logger::Component::LB,
        fmt::format("Started on port {}", config.load_balancer.port));

    std::cout << fmt::format("Load balancer started on port {}\n", config.load_balancer.port);
    std::cout << "Press Ctrl+C to stop\n";

    // Run server in a separate thread to allow graceful shutdown
    std::thread server_thread([&]() {
        server.listen("0.0.0.0", config.load_balancer.port);
    });

    // Wait for shutdown signal
    while (!shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Graceful shutdown
    std::cout << "\nShutting down gracefully...\n";
    Logger::info(Logger::Component::LB, "Shutting down gracefully");

    server.stop();
    health_checker.stop();

    if (server_thread.joinable()) {
        server_thread.join();
    }

    Logger::shutdown();

    std::cout << "Shutdown complete\n";
    return 0;
}
