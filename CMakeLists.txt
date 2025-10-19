cmake_minimum_required(VERSION 3.20)
project(LoadBalancer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clangd and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# External dependencies using FetchContent
include(FetchContent)

# cpp-httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(httplib spdlog json)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Load balancer library (shared code)
add_library(lb_core
    src/config_loader.cpp
    src/logger.cpp
    src/backend_manager.cpp
    src/health_checker.cpp
    src/response_injector.cpp
)

target_link_libraries(lb_core
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        httplib::httplib
)

target_include_directories(lb_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Load balancer executable
add_executable(load_balancer
    src/main.cpp
)

target_link_libraries(load_balancer
    PRIVATE
        lb_core
        pthread
)

# Backend server executable
add_executable(backend_server
    src/backend_server.cpp
)

target_link_libraries(backend_server
    PRIVATE
        lb_core
        pthread
)

# Tests (GoogleTest is system-installed)
enable_testing()
find_package(GTest REQUIRED)

add_executable(tests
    tests/test_config_loader.cpp
    tests/test_routing_policy.cpp
    tests/test_backend_manager.cpp
    tests/test_response_injector.cpp
)

target_link_libraries(tests
    PRIVATE
        lb_core
        GTest::gtest
        GTest::gtest_main
        pthread
)

include(GoogleTest)
gtest_discover_tests(tests)

# Installation
install(TARGETS load_balancer backend_server
    RUNTIME DESTINATION bin
)
