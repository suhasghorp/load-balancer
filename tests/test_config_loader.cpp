#include <gtest/gtest.h>
#include "config_loader.hpp"
#include <fstream>

using namespace lb;

class ConfigLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test config file
        test_config_path = "test_config.json";
    }

    void TearDown() override {
        // Clean up test files
        std::remove(test_config_path.c_str());
    }

    void write_config(const std::string& content) {
        std::ofstream file(test_config_path);
        file << content;
        file.close();
    }

    std::string test_config_path;
};

TEST_F(ConfigLoaderTest, LoadValidConfig) {
    std::string valid_config = R"({
        "load_balancer": {
            "port": 8000,
            "log_file": "test.log",
            "log_level": "DEBUG"
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
    })";

    write_config(valid_config);

    auto result = ConfigLoader::load(test_config_path);
    ASSERT_TRUE(result.has_value());

    Config config = result.value();
    EXPECT_EQ(config.load_balancer.port, 8000);
    EXPECT_EQ(config.load_balancer.log_file, "test.log");
    EXPECT_EQ(config.load_balancer.log_level, "DEBUG");
    EXPECT_EQ(config.backends.size(), 2);
    EXPECT_EQ(config.backends[0].port, 8080);
    EXPECT_EQ(config.health_check.interval_seconds, 1);
    EXPECT_EQ(config.algorithm, "round-robin");
}

TEST_F(ConfigLoaderTest, MissingFile) {
    auto result = ConfigLoader::load("nonexistent.json");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ConfigLoaderTest, InvalidJson) {
    write_config("{ invalid json }");
    auto result = ConfigLoader::load(test_config_path);
    ASSERT_FALSE(result.has_value());
}

TEST_F(ConfigLoaderTest, MissingBackends) {
    std::string invalid_config = R"({
        "load_balancer": {
            "port": 8000,
            "log_file": "test.log",
            "log_level": "INFO"
        },
        "health_check": {
            "interval_seconds": 1,
            "timeout_seconds": 1
        },
        "algorithm": "round-robin"
    })";

    write_config(invalid_config);
    auto result = ConfigLoader::load(test_config_path);
    ASSERT_FALSE(result.has_value());
}

TEST_F(ConfigLoaderTest, EmptyBackends) {
    std::string invalid_config = R"({
        "load_balancer": {
            "port": 8000,
            "log_file": "test.log",
            "log_level": "INFO"
        },
        "backends": [],
        "health_check": {
            "interval_seconds": 1,
            "timeout_seconds": 1
        },
        "algorithm": "round-robin"
    })";

    write_config(invalid_config);
    auto result = ConfigLoader::load(test_config_path);
    ASSERT_FALSE(result.has_value());
}
