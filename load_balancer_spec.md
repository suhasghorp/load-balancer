# Load Balancer Implementation Specification

## Project Overview
Build an HTTP load balancer in C++23 using CMake that routes requests to backend servers using a user defined algorithm with health checking capabilities.

## Requirements

### Core Functionality
1. **Load Balancer**: Listen on port 80, forward HTTP requests to backend servers (ports 8080-8089)
2. **User defined Routing**: Distribute requests evenly across healthy backends using an algorithm specified in configuration
3. **Health Checking**: Check backend health every 10 seconds via HTTP GET to /health endpoint (1 second timeout)
4. **Auto-Recovery**: Automatically use backends when they become healthy again
5. **Logging**: Comprehensive logging for both load balancer and backend servers
6. **Response Injection**: Add backend server port information to response body

### Technical Requirements
- **Language**: C++23
- **Build System**: CMake
- **HTTP Library**: cpp-httplib (header-only, lightweight)
- **Logging Library**: spdlog
- **Configuration**: JSON config file
- **Concurrency**: Hybrid model (thread pool + dedicated health check thread)

## Architecture

### Components

#### 1. Load Balancer Application
**Main Components:**
- HTTP Server (port 80)
- Backend Manager (tracks backend servers and health status)
- Health Checker (dedicated thread, 1-second interval)
- Request Router (user defined with health filtering)
- Logger

**Concurrency Model:**
- Main thread: HTTP server
- Health check thread: std::jthread with 1-second periodic checks
- Request handling: Thread pool or per-request threads (light load)
- Shared state protection: std::shared_mutex (read-heavy workload)

#### 2. Backend Server Applications
**Purpose:** Simple HTTP servers for testing the load balancer
**Features:**
- Listen on ports 8080-8089
- Respond to /health endpoint (200 OK)
- Handle general requests
- Log all incoming requests
- Can be started/stopped independently for testing

#### 3. Configuration System
**Format:** JSON configuration file
**Location:** config.json in project root

### Data Structures

#### Backend Server
```cpp
struct BackendServer {
    std::string host;
    uint16_t port;
    std::string health_endpoint;
    std::atomic<bool> is_healthy;
    std::chrono::steady_clock::time_point last_check;
};
```

#### Request Context
```cpp
struct RequestContext {
    std::string client_ip;
    std::string method;
    std::string path;
    std::chrono::steady_clock::time_point start_time;
    uint16_t backend_port;
};
```

## Detailed Specifications

### 1. Configuration File (config.json)

```json
{
  "load_balancer": {
    "port": 80,
    "log_file": "lb.log",
    "log_level": "INFO"
  },
  "backends": [
    {"host": "localhost", "port": 8080, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8081, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8082, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8083, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8084, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8085, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8086, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8087, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8088, "health_endpoint": "/health"},
    {"host": "localhost", "port": 8089, "health_endpoint": "/health"}
  ],
  "health_check": {
    "interval_seconds": 10,
    "timeout_seconds": 1
  },
  "algorithm" : "round-robin"
}
```

### 2. Load Balancer Behavior

#### Routing Algorithm 
- Should be able to switch routing algorithms with minimal code changes
- Use template based design templated on the algorithm

#### When Round-Robin Algorithm is specified
- Maintain atomic counter for round-robin index
- Skip unhealthy backends
- Wrap around to beginning after reaching end
- If all backends unhealthy: return 503 Service Unavailable

#### Health Checking
- Dedicated std::jthread runs every 1 second
- HTTP GET request to backend's health_endpoint
- 1 second timeout per check
- Update backend health status (acquire write lock)
- Log health state changes

#### Request Forwarding
1. Receive request from client
2. Select next healthy backend (read lock on backend list)
3. Forward complete HTTP request to backend
4. Receive response from backend
5. Inject backend port info into response body
6. Return response to client
7. Log the complete transaction

#### Response Body Injection
- Detect Content-Type from response headers
- **HTML**: Add comment before closing `</body>` tag: `<!-- Served by backend server on port XXXX -->`
- **JSON**: Add field `"_server": "backend-XXXX"` to root object
- **Plain text**: Append `\n[Served by backend server on port XXXX]` at end
- **Other types**: Skip injection

### 3. Load Balancer Logging

#### Log Format
```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [COMPONENT] Message
```

#### Events to Log

**Startup/Shutdown:**
```
[INFO] [LB] Started on port 80
[INFO] [Config] Loaded 10 backends: ports 8080-8089
[INFO] [LB] Shutting down gracefully
```

**Health Checks:**
```
[INFO] [HealthCheck] Starting health check cycle
[INFO] [HealthCheck] Backend 8082: HEALTHY (45ms)
[ERROR] [HealthCheck] Backend 8085: UNHEALTHY (timeout 1000ms)
[WARN] [HealthCheck] Backend 8085: state changed HEALTHY → UNHEALTHY
[INFO] [HealthCheck] Backend 8085: state changed UNHEALTHY → HEALTHY
```

**Request Processing:**
```
[INFO] [Request] Client 192.168.1.100 → GET /api/users
[INFO] [Router] Selected backend: 8083
[INFO] [Response] 200 OK → Client (44ms) via backend 8083
[WARN] [Router] Backend 8085 unhealthy, skipping
[ERROR] [Router] All backends unhealthy, returning 503
```

**Log Configuration:**
- Console output: INFO and above
- File output: rotating logs, 10MB per file, keep 5 files
- Development mode: DEBUG level (includes health check details)

### 4. Backend Server Specifications

#### Purpose
Simple HTTP servers for testing the load balancer. Each backend server should be a standalone executable that can be started or stopped independently.

#### Endpoints

**/health**
- Method: GET
- Response: 200 OK with body `{"status": "healthy"}`
- Purpose: Health check endpoint

**/* (all other paths)**
- Method: Any
- Response: 200 OK with simple response indicating the backend
- Example response body: `{"message": "Hello from backend", "port": 8083}`

#### Behavior
- Accept command-line argument for port number
- Log all incoming requests
- Respond quickly (simulate real application)
- Can be killed/restarted to test health checking

#### Backend Server Logging

**Log Format:** Same as load balancer

**Events to Log:**
```
[INFO] [Backend] Started on port 8083
[DEBUG] [Backend] Health check from 127.0.0.1
[INFO] [Request] GET /api/users from 127.0.0.1
[INFO] [Response] 200 OK (40ms)
```

**Log Configuration:**
- Console output: INFO and above
- File output: `backend_XXXX.log`, rotating, 5MB per file, keep 3 files

### 5. C++23/20 Features to Use

**Required:**
- `std::jthread` with `std::stop_token` for graceful shutdown
- `std::shared_mutex` for backend list synchronization
- `std::atomic<bool>` for health status
- `std::atomic<size_t>` for round-robin counter
- `std::expected` for error handling (C++23)
- `std::format` for string formatting (C++20)
- `std::chrono` for timing and intervals

**Optional but Recommended:**
- Range algorithms for filtering healthy backends
- `std::span` where appropriate

### 6. Error Handling

#### All Backends Unhealthy
- Return 503 Service Unavailable to client
- Response body: `{"error": "No healthy backends available"}`
- Log error

#### Backend Connection Failure
- Mark backend as unhealthy immediately
- Try next backend in round-robin
- Log error with backend port

#### Backend Timeout
- Use 5-second timeout for request forwarding (configurable)
- Mark backend as unhealthy
- Try next backend
- Log timeout

#### Configuration Errors
- Log error and exit if config file missing or invalid
- Validate all required fields present

### 7. Project Structure

```
load-balancer/
├── CMakeLists.txt
├── config.json
├── README.md
├── include/
│   ├── backend_manager.hpp
│   ├── health_checker.hpp
│   ├── request_router.hpp
│   ├── config_loader.hpp
│   └── logger.hpp
├── src/
│   ├── main.cpp (load balancer)
│   ├── backend_server.cpp (backend application)
│   ├── backend_manager.cpp
│   ├── health_checker.cpp
│   ├── request_router.cpp
│   ├── config_loader.cpp
│   └── logger.cpp
├── external/
│   ├── httplib.h (cpp-httplib)
│   └── spdlog/ (as submodule or copied)
└── tests/
    └── (optional unit tests)
```

### 8. CMake Configuration

**Requirements:**
- C++23 standard
- Link cpp-httplib (header-only)
- Link spdlog
- Build two executables: `load_balancer` and `backend_server`
- Support for Linux/macOS (Windows optional)

**Targets:**
- `load_balancer` - main load balancer application
- `backend_server` - simple backend server for testing

### 9. Build and Run Instructions

#### Build
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

#### Run Load Balancer
```bash
./load_balancer
```
(Reads config.json from current directory)

#### Run Backend Servers
```bash
./backend_server 8080 &
./backend_server 8081 &
./backend_server 8082 &
# ... etc for ports 8080-8089
```

#### Test
```bash
# Send request to load balancer
curl http://localhost:80/api/test

# Check logs
tail -f lb.log
tail -f backend_8080.log
```

### 10. Testing Scenarios

#### Scenario 1: Normal Operation
1. Start load balancer
2. Start all 10 backend servers
3. Send multiple requests to load balancer
4. Verify round-robin distribution in logs
5. Verify response bodies contain backend port info

#### Scenario 2: Backend Failure
1. Start load balancer with all backends running
2. Kill one backend server (e.g., port 8083)
3. Wait for health check to detect failure (~10 seconds)
4. Verify load balancer skips unhealthy backend
5. Send requests and verify they go to other backends

#### Scenario 3: Backend Recovery
1. Continue from Scenario 2
2. Restart the killed backend server
3. Wait for health check to detect recovery (~10 seconds)
4. Verify load balancer starts using recovered backend
5. Verify round-robin includes recovered backend

#### Scenario 4: All Backends Down
1. Start load balancer
2. Stop all backend servers
3. Send request to load balancer
4. Verify 503 response
5. Check logs for appropriate error messages

## Implementation Notes

### Synchronization Strategy
```
Backend List (vector<BackendServer>):
├─ Health Check Thread: write lock when updating health status
└─ Request Router Threads: read lock when selecting backend

Round-robin counter: std::atomic<size_t> (lock-free)
```

### Thread Safety
- Backend health status: atomic bool per backend
- Backend list: protected by shared_mutex
- Round-robin counter: atomic increment
- Logging: spdlog handles thread-safety internally

### Performance Considerations
- No connection pooling (create new connection per request)
- Lightweight for low-medium load
- Read locks are fast (shared_mutex)
- Health checks don't block request processing

## Dependencies

### Required Libraries
1. **cpp-httplib**: https://github.com/yhirose/cpp-httplib
   - Single header: httplib.h
   - Place in external/ directory

2. **spdlog**: https://github.com/gabime/spdlog
   - Can use as submodule or copy headers
   - Header-only mode available

3. **nlohmann/json** (for config parsing): https://github.com/nlohmann/json
   - Single header: json.hpp
   - Place in external/ directory

### Compiler Requirements
- GCC 13+ or Clang 16+ (for C++23 support)
- CMake 3.20+

## Deliverables

1. Complete source code with all components
2. CMakeLists.txt with proper configuration
3. config.json example file
4. README.md with build and usage instructions
5. Working load balancer executable
6. Working backend server executable
7. All components properly logged

## Success Criteria

- [ ] Load balancer starts and listens on port 80
- [ ] Backend servers start on ports 8080-8089
- [ ] Round-robin distribution works correctly
- [ ] Health checks run every 10 seconds
- [ ] Unhealthy backends are skipped
- [ ] Recovered backends are reused
- [ ] All events are logged appropriately
- [ ] Response bodies contain backend port information
- [ ] 503 returned when all backends are down
- [ ] Graceful shutdown on SIGINT/SIGTERM
- [ ] Code compiles with C++23 standard
- [ ] No race conditions or deadlocks

## Optional Enhancements (Not Required)

- Configurable round-robin vs other algorithms (random, least connections)
- Weighted backends
- Request retry logic
- Prometheus metrics endpoint
- Docker containerization
- Graceful backend draining

---

**This specification is complete and ready for implementation. Start with the project structure, then implement components in this order: config loader, logger, backend manager, health checker, request router, main load balancer, and finally backend server.**