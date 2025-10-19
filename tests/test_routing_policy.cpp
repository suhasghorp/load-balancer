#include <gtest/gtest.h>
#include "routing_policy.hpp"
#include "backend_manager.hpp"
#include <vector>

using namespace lb;

class RoutingPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test backends
        BackendConfig bc1{"localhost", 8080, "/health"};
        BackendConfig bc2{"localhost", 8081, "/health"};
        BackendConfig bc3{"localhost", 8082, "/health"};

        std::vector<BackendConfig> configs = {bc1, bc2, bc3};
        backend_manager = std::make_shared<BackendManager>(configs);
    }

    std::shared_ptr<BackendManager> backend_manager;
};

TEST_F(RoutingPolicyTest, RoundRobinDistribution) {
    RoundRobinPolicy policy;

    auto backends = backend_manager->get_healthy_backends();
    ASSERT_EQ(backends.size(), 3);

    // Test round-robin distribution
    std::vector<uint16_t> ports;
    for (int i = 0; i < 9; ++i) {
        auto result = policy.select(backends);
        ASSERT_TRUE(result.has_value());
        ports.push_back(result.value()->port);
    }

    // Verify round-robin pattern: 8080, 8081, 8082, 8080, 8081, 8082, ...
    EXPECT_EQ(ports[0], 8080);
    EXPECT_EQ(ports[1], 8081);
    EXPECT_EQ(ports[2], 8082);
    EXPECT_EQ(ports[3], 8080);
    EXPECT_EQ(ports[4], 8081);
    EXPECT_EQ(ports[5], 8082);
    EXPECT_EQ(ports[6], 8080);
    EXPECT_EQ(ports[7], 8081);
    EXPECT_EQ(ports[8], 8082);
}

TEST_F(RoutingPolicyTest, NoBackends) {
    RoundRobinPolicy policy;
    std::vector<BackendServer*> empty_backends;

    auto result = policy.select(empty_backends);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "No healthy backends available");
}

TEST_F(RoutingPolicyTest, SingleBackend) {
    RoundRobinPolicy policy;

    auto backends = backend_manager->get_healthy_backends();
    std::vector<BackendServer*> single_backend = {backends[0]};

    for (int i = 0; i < 5; ++i) {
        auto result = policy.select(single_backend);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value()->port, 8080);
    }
}

TEST_F(RoutingPolicyTest, ResetCounter) {
    RoundRobinPolicy policy;

    auto backends = backend_manager->get_healthy_backends();

    // Select a few backends
    policy.select(backends);
    policy.select(backends);
    policy.select(backends);

    // Reset and verify it starts from beginning
    policy.reset();

    auto result = policy.select(backends);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->port, 8080);
}
