# HTTP Load Balancer

A high-performance HTTP load balancer implementation in C++23 with health checking and configurable routing algorithms.

## Features

- **Template-based Routing**: Compile-time algorithm selection (Round-robin, extensible to Random, Least Connections)
- **Health Checking**: Automatic backend health monitoring with 1-second interval
- **Auto-Recovery**: Automatically detects and uses recovered backends
- **Response Injection**: Adds backend server information to responses (HTML/JSON/text)
- **Comprehensive Logging**: Detailed logs for all components using spdlog
- **Modern C++23**: Uses `std::jthread`, `std::expected`, `std::format`, `std::shared_mutex`
- **Graceful Shutdown**: Clean shutdown on SIGINT/SIGTERM

## Architecture

```
Load Balancer (port 8000)
├── Config Loader      → JSON configuration parsing
├── Logger            → Rotating file logs + console
├── Backend Manager   → Backend tracking with health status
├── Health Checker    → std::jthread, 1-second interval checks
├── Request Router<T> → Template-based routing policy
│   └── RoundRobinPolicy → Atomic counter-based distribution
└── Response Injector → Content-type aware injection
```

## Requirements

- **Compiler**: GCC 13+ or Clang 16+ (C++23 support)
- **CMake**: 3.20+
- **GoogleTest**: System-installed (for tests)

### Dependencies (auto-fetched by CMake)

- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - HTTP server/client
- [spdlog](https://github.com/gabime/spdlog) - Logging
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing

## Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
cmake --build .

# Run tests
ctest --output-on-failure
```

## Configuration

Edit `config.json` to configure the load balancer:

```json
{
  "load_balancer": {
    "port": 8000,
    "log_file": "lb.log",
    "log_level": "INFO"
  },
  "backends": [
    {"host": "localhost", "port": 8080, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8081, "health_endpoint": "/health"}
  ],
  "health_check": {
    "interval_seconds": 1,
    "timeout_seconds": 1
  },
  "algorithm": "round-robin"
}
```

## Usage

### Start Backend Servers

```bash
# Start all backend servers (ports 8080-8089)
./start_backends.sh

# Start a specific backend
./start_backends.sh 8080

# Check status
./status_backends.sh
```

### Start Load Balancer

```bash
cd build
./load_balancer
```

The load balancer will start on port 8000 (or configured port).

### Send Requests

```bash
# Test basic request
curl http://localhost:8000/api/test

# Send multiple requests to see round-robin
for i in {1..10}; do curl http://localhost:8000/api/test; echo; done

# POST request
curl -X POST http://localhost:8000/api/data -d '{"key": "value"}'
```

### Stop Backend Servers

```bash
# Stop a specific backend
./stop_backends.sh 8085

# Stop all backends
./stop_backends.sh all
```

## Testing Scenarios

### Scenario 1: Normal Operation

```bash
# Start everything
./start_backends.sh
cd build && ./load_balancer

# Send requests
curl http://localhost:8000/api/test

# Check logs
tail -f lb.log
tail -f backend_8080.log
```

### Scenario 2: Backend Failure

```bash
# Kill a backend
./stop_backends.sh 8083

# Wait ~1 second for health check to detect
# Send requests - they'll skip the failed backend
curl http://localhost:8000/api/test
```

### Scenario 3: Backend Recovery

```bash
# Restart the backend
./start_backends.sh 8083

# Wait ~1 second for health check
# Send requests - recovered backend is used
curl http://localhost:8000/api/test
```

### Scenario 4: All Backends Down

```bash
# Stop all backends
./stop_backends.sh all

# Request returns 503
curl http://localhost:8000/api/test
# Response: {"error": "No healthy backends available"}
```

## Response Body Injection

The load balancer injects backend server information based on content type:

### HTML
```html
<body>
  <h1>Content</h1>
  <!-- Served by backend server on port 8080 -->
</body>
```

### JSON
```json
{
  "message": "Hello",
  "_server": "backend-8080"
}
```

### Text
```
Hello World
[Served by backend server on port 8080]
```

## Project Structure

```
load-balancer/
├── CMakeLists.txt              # Build configuration
├── config.json                 # Runtime configuration
├── README.md                   # This file
├── CLAUDE.md                   # Claude Code guidance
├── load_balancer_spec.md       # Full specification
├── include/                    # Header files
│   ├── backend_manager.hpp
│   ├── config_loader.hpp
│   ├── health_checker.hpp
│   ├── logger.hpp
│   ├── request_router.hpp
│   ├── response_injector.hpp
│   └── routing_policy.hpp
├── src/                        # Source files
│   ├── main.cpp               # Load balancer main
│   ├── backend_server.cpp     # Backend server main
│   ├── backend_manager.cpp
│   ├── config_loader.cpp
│   ├── health_checker.cpp
│   ├── logger.cpp
│   └── response_injector.cpp
├── tests/                      # Unit tests
│   ├── test_config_loader.cpp
│   ├── test_routing_policy.cpp
│   ├── test_backend_manager.cpp
│   └── test_response_injector.cpp
├── start_backends.sh          # Start backend servers
├── stop_backends.sh           # Stop backend servers
└── status_backends.sh         # Check backend status
```

## Key Design Features

### Template-based Routing
```cpp
// Compile-time policy selection
RequestRouter<RoundRobinPolicy> router(backend_manager);

// Easy to add new policies
RequestRouter<RandomPolicy> router(backend_manager);
RequestRouter<LeastConnectionsPolicy> router(backend_manager);
```

### Thread Safety
- **Backend health status**: `std::atomic<bool>` per backend
- **Backend list**: `std::shared_mutex` (read-heavy workload)
- **Round-robin counter**: `std::atomic<size_t>` (lock-free)
- **Logging**: spdlog handles thread-safety internally

### Error Handling
- Uses C++23 `std::expected` for error propagation
- No exceptions in hot path
- Graceful degradation when backends fail

## Logs

### Load Balancer Log (`lb.log`)
- Startup/shutdown events
- Health check cycles
- Request routing decisions
- Backend state changes

### Backend Logs (`backend_XXXX.log`)
- Server startup
- Incoming requests
- Response times

## Adding New Routing Policies

1. Implement the policy in [routing_policy.hpp](include/routing_policy.hpp):
```cpp
class MyPolicy {
public:
    std::expected<BackendServer*, std::string> select(
        const std::vector<BackendServer*>& backends) {
        // Your selection logic
    }
};
```

2. Instantiate the router with your policy in [main.cpp](src/main.cpp):
```cpp
RequestRouter<MyPolicy> router(backend_manager);
```

3. Update config.json:
```json
{
  "algorithm": "my-policy"
}
```

## License

This project is for educational purposes.
