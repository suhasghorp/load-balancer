# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C++23 HTTP load balancer with configurable routing algorithms, health checking, and automatic backend recovery. The architecture uses template-based compile-time policy selection for routing algorithms.

## Build Commands

### Initial Build
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Rebuild After Changes
```bash
cd build
cmake --build .
```

### Run Tests
```bash
cd build
ctest --output-on-failure
```

### Run Single Test
```bash
cd build
./tests --gtest_filter=ConfigLoaderTest.LoadValidConfig
./tests --gtest_filter=RoutingPolicyTest.*
```

### Clean Build
```bash
rm -rf build
mkdir build && cd build
cmake .. && cmake --build .
```

## Running the Application

### Start Backend Servers
```bash
# From project root
./start_backends.sh           # Start all backends (8080-8089)
./start_backends.sh 8080      # Start specific backend
./status_backends.sh          # Check status
./stop_backends.sh 8085       # Stop specific backend
./stop_backends.sh all        # Stop all backends
```

### Start Load Balancer
```bash
cd build
./load_balancer               # Reads config.json from current dir
```

### Test the System
```bash
# Send test requests
curl http://localhost:8000/api/test

# View logs
tail -f lb.log
tail -f backend_8080.log
```

## Architecture

### Template-Based Routing System

The core design uses compile-time template selection for routing policies:

```cpp
// In main.cpp
RequestRouter<RoundRobinPolicy> router(backend_manager);
```

**Key Design Decision**: Routing policies are selected at compile-time via templates, not runtime polymorphism. This enables zero-overhead abstraction and allows the compiler to inline policy logic.

To add a new routing policy:
1. Define the policy class in [include/routing_policy.hpp](include/routing_policy.hpp)
2. Implement `select(const std::vector<BackendServer*>&)` method
3. Satisfy the `RoutingPolicy` C++20 concept
4. Change template parameter in [src/main.cpp](src/main.cpp)

### Thread Model

**Single-threaded components:**
- Main thread runs the HTTP server (cpp-httplib handles request threads automatically)
- Health checker runs in dedicated `std::jthread`

**Concurrency patterns:**
- Backend health status: `std::atomic<bool>` per backend (lock-free reads)
- Backend list: `std::shared_mutex` (many readers, rare writers)
- Round-robin counter: `std::atomic<size_t>` (lock-free increment)

**Critical sections:**
- Health checker acquires **write lock** when updating backend health status
- Request router acquires **read lock** when selecting backends
- This read-heavy design minimizes lock contention

### Health Checking Flow

1. Health checker thread wakes every 1 second (configurable in config.json)
2. For each backend: HTTP GET to `/health` endpoint with 1-second timeout
3. On state change (HEALTHY ↔ UNHEALTHY): log the transition
4. Update `BackendServer::is_healthy` atomically
5. Request router automatically skips unhealthy backends

### Response Body Injection

The `ResponseInjector` parses Content-Type headers and modifies responses:
- **HTML**: Inserts comment before `</body>` tag using regex
- **JSON**: Parses with nlohmann/json, adds `"_server"` field
- **Text**: Appends footer to body
- **Other**: Returns unchanged

## File Organization

### Header Files ([include/](include/))
- `config_loader.hpp` - JSON config parsing with `std::expected`
- `logger.hpp` - spdlog wrapper with component-based logging
- `backend_manager.hpp` - Backend tracking with thread-safe health updates
- `health_checker.hpp` - `std::jthread`-based health monitoring
- `routing_policy.hpp` - Policy implementations (RoundRobin, etc.)
- `request_router.hpp` - Template class for policy selection
- `response_injector.hpp` - Content-type aware body injection

### Source Files ([src/](src/))
- `main.cpp` - Load balancer application entry point
- `backend_server.cpp` - Simple backend server for testing
- Other `.cpp` files implement corresponding headers

### Tests ([tests/](tests/))
- Unit tests using GoogleTest framework
- Each component has dedicated test file
- Tests verify core logic without network I/O where possible

## Important Implementation Notes

### C++23 Features Used

1. **`std::expected`** - Error handling without exceptions
   - Used in: config loading, backend selection
   - Pattern: `if (!result.has_value()) { handle error }`

2. **`std::format`** - Type-safe string formatting
   - Used in: all logging statements
   - Pattern: `std::format("Backend {}: {}", port, status)`

3. **`std::jthread`** - Auto-joining thread with stop tokens
   - Used in: health checker
   - Pattern: `std::jthread([](std::stop_token token) { ... })`

4. **`std::shared_mutex`** - Reader-writer lock
   - Used in: backend manager
   - Pattern: `std::shared_lock` for reads, `std::unique_lock` for writes

5. **C++20 Concepts** - Compile-time interface checking
   - Used in: `RoutingPolicy` concept
   - Ensures policies implement required methods

### Backend Manager Thread Safety

The `BackendManager` uses a specific synchronization pattern:

```cpp
// Reading (happens frequently - request routing)
std::shared_lock lock(mutex_);  // Multiple readers allowed
auto backends = get_healthy_backends();

// Writing (happens rarely - health updates)
std::unique_lock lock(mutex_);  // Exclusive access
backends_[index].is_healthy.store(false);
```

**Why this works**: Health checks are infrequent (1/second), but request routing happens on every request. Shared locks allow many concurrent readers without blocking.

### Backend Server Move Semantics

`BackendServer` uses move-only semantics because it contains `std::atomic<bool>`:
- Copy constructor/assignment: deleted
- Move constructor/assignment: implemented
- Store atomics in `std::vector` - requires move support

### Error Handling Pattern

```cpp
auto result = do_something();
if (!result.has_value()) {
    Logger::error(Component::XYZ, result.error());
    return std::unexpected(result.error());
}
// Use result.value()
```

## Configuration

All runtime behavior is controlled by [config.json](config.json):
- Load balancer port (default: 8000)
- Backend list with health endpoints
- Health check interval (default: 1 second)
- Routing algorithm (currently only "round-robin")

## Common Development Tasks

### Adding Support for a New HTTP Method

Edit [src/main.cpp](src/main.cpp) and add a new handler:
```cpp
server.Put(".*", [&](const httplib::Request& req, httplib::Response& res) {
    // Follow the pattern from Post handler
});
```

### Changing Health Check Interval

Edit [config.json](config.json):
```json
"health_check": {
  "interval_seconds": 5,    // Check every 5 seconds instead of 1
  "timeout_seconds": 2      // 2 second timeout per check
}
```

### Debugging Health Checks

Change log level to DEBUG in [config.json](config.json):
```json
"load_balancer": {
  "log_level": "DEBUG"
}
```

This enables detailed health check logging (per-backend timing, all state checks).

### Adding a New Routing Policy

1. Define policy in [include/routing_policy.hpp](include/routing_policy.hpp):
```cpp
class MyPolicy {
public:
    std::expected<BackendServer*, std::string> select(
        const std::vector<BackendServer*>& backends) {
        // Implementation
    }
};
```

2. Update [src/main.cpp](src/main.cpp):
```cpp
RequestRouter<MyPolicy> router(backend_manager);
```

3. Recompile (policy selection is compile-time)

## Dependencies

All external dependencies are fetched automatically by CMake FetchContent:
- **cpp-httplib**: HTTP server/client library (header-only)
- **spdlog**: Fast logging library
- **nlohmann/json**: JSON parsing

GoogleTest is expected to be system-installed (not fetched).

## Port Configuration

- **Load balancer**: Port 8000 (configurable in config.json)
- **Backend servers**: Ports 8080-8089 (configurable)
- Originally spec'd for port 80, changed to 8000 to avoid requiring root privileges

## Backend Management Scripts

Three helper scripts for managing backends:
- `start_backends.sh [port|all]` - Start backends
- `stop_backends.sh <port|all>` - Stop backends
- `status_backends.sh` - Show running status

These scripts use PID files stored in `.pids/` directory.
