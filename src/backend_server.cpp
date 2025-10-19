#include "logger.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/fmt.h>
#include <iostream>
#include <csignal>
#include <atomic>

using json = nlohmann::json;
using namespace lb;

std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        shutdown_requested.store(true);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize logger for backend
    Logger::init("backend.log", "INFO", true, port);

    Logger::info(Logger::Component::Backend,
        fmt::format("Started on port {}", port));

    httplib::Server server;

    // Health check endpoint
    server.Get("/health", [port](const httplib::Request& req, httplib::Response& res) {
        Logger::debug(Logger::Component::Backend,
            fmt::format("Health check from {}", req.remote_addr));

        json response;
        response["status"] = "healthy";

        res.set_content(response.dump(), "application/json");
    });

    // Default handler for all other requests
    server.Get(".*", [port](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();

        Logger::info(Logger::Component::Request,
            fmt::format("{} {} from {}", req.method, req.path, req.remote_addr));

        json response;
        response["message"] = "Hello from backend";
        response["port"] = port;
        response["path"] = req.path;
        response["method"] = req.method;

        res.set_content(response.dump(), "application/json");

        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        Logger::info(Logger::Component::Response,
            fmt::format("200 OK ({}ms)", duration_ms));
    });

    // POST handler
    server.Post(".*", [port](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();

        Logger::info(Logger::Component::Request,
            fmt::format("{} {} from {}", req.method, req.path, req.remote_addr));

        json response;
        response["message"] = "POST received by backend";
        response["port"] = port;
        response["path"] = req.path;
        response["method"] = req.method;
        response["body_size"] = req.body.size();

        res.set_content(response.dump(), "application/json");

        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        Logger::info(Logger::Component::Response,
            fmt::format("200 OK ({}ms)", duration_ms));
    });

    std::cout << fmt::format("Backend server started on port {}\n", port);
    std::cout << "Press Ctrl+C to stop\n";

    // Run server in a separate thread to allow graceful shutdown
    std::thread server_thread([&]() {
        server.listen("0.0.0.0", port);
    });

    // Wait for shutdown signal
    while (!shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Graceful shutdown
    std::cout << "\nShutting down backend server...\n";
    Logger::info(Logger::Component::Backend,
        fmt::format("Backend on port {} shutting down", port));

    server.stop();

    if (server_thread.joinable()) {
        server_thread.join();
    }

    Logger::shutdown();

    return 0;
}
