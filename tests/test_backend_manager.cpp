#include <gtest/gtest.h>
#include "backend_manager.hpp"

using namespace lb;

class BackendManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        BackendConfig bc1{"localhost", 8080, "/health"};
        BackendConfig bc2{"localhost", 8081, "/health"};
        BackendConfig bc3{"localhost", 8082, "/health"};

        std::vector<BackendConfig> configs = {bc1, bc2, bc3};
        manager = std::make_unique<BackendManager>(configs);
    }

    std::unique_ptr<BackendManager> manager;
};

TEST_F(BackendManagerTest, InitialState) {
    EXPECT_EQ(manager->backend_count(), 3);

    auto all_backends = manager->get_all_backends();
    EXPECT_EQ(all_backends.size(), 3);

    // All backends should be healthy initially
    auto healthy_backends = manager->get_healthy_backends();
    EXPECT_EQ(healthy_backends.size(), 3);
}

TEST_F(BackendManagerTest, UpdateHealth) {
    // Mark backend 1 as unhealthy
    manager->update_health(1, false);

    auto healthy_backends = manager->get_healthy_backends();
    EXPECT_EQ(healthy_backends.size(), 2);

    // Verify the remaining healthy backends
    EXPECT_EQ(healthy_backends[0]->port, 8080);
    EXPECT_EQ(healthy_backends[1]->port, 8082);
}

TEST_F(BackendManagerTest, AllUnhealthy) {
    manager->update_health(0, false);
    manager->update_health(1, false);
    manager->update_health(2, false);

    auto healthy_backends = manager->get_healthy_backends();
    EXPECT_EQ(healthy_backends.size(), 0);

    auto all_backends = manager->get_all_backends();
    EXPECT_EQ(all_backends.size(), 3);
}

TEST_F(BackendManagerTest, RecoverBackend) {
    // Mark backend as unhealthy
    manager->update_health(1, false);
    EXPECT_EQ(manager->get_healthy_backends().size(), 2);

    // Recover backend
    manager->update_health(1, true);
    EXPECT_EQ(manager->get_healthy_backends().size(), 3);
}

TEST_F(BackendManagerTest, GetAllBackends) {
    auto backends = manager->get_all_backends();
    EXPECT_EQ(backends.size(), 3);
    EXPECT_EQ(backends[0]->port, 8080);
    EXPECT_EQ(backends[1]->port, 8081);
    EXPECT_EQ(backends[2]->port, 8082);
}
