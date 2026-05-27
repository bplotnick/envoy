// Tests for the eager (async) connection establishment feature.

#include "test/common/upstream/cluster_manager_impl_test_common.h"

namespace Envoy {
namespace Upstream {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnNew;

class EagerConnectionEstablishmentTest : public ClusterManagerImplTest {
public:
  void createWithEagerConfig(const std::string& yaml) {
    // When eager connection establishment is enabled, the priming code may call
    // allocateConnPool_ during initial host setup. Set up a default mock.
    ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _))
        .WillByDefault(ReturnNew<NiceMock<Http::ConnectionPool::MockInstance>>());
    create(parseBootstrapFromV3Yaml(yaml));
  }
};

// ---------------------------------------------------------------------------
// Configuration parsing tests
// ---------------------------------------------------------------------------

TEST_F(EagerConnectionEstablishmentTest, ConfigParsing) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
      eager_connection_establishment:
        enabled: true
        max_concurrent_priming:
          value: 5
        prefer_ready_hosts:
          value: true
  )EOF";

  createWithEagerConfig(yaml);

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_TRUE(cluster->info()->eagerConnectionEstablishmentEnabled());
  EXPECT_EQ(5, cluster->info()->eagerConnectionMaxConcurrentPriming());
  EXPECT_TRUE(cluster->info()->eagerConnectionPreferReadyHosts());

  factory_.tls_.shutdownThread();
}

TEST_F(ClusterManagerImplTest, EagerConnectionDisabledByDefault) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
  )EOF";

  create(parseBootstrapFromV3Yaml(yaml));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_FALSE(cluster->info()->eagerConnectionEstablishmentEnabled());

  factory_.tls_.shutdownThread();
}

TEST_F(EagerConnectionEstablishmentTest, Defaults) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
      eager_connection_establishment:
        enabled: true
  )EOF";

  createWithEagerConfig(yaml);

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_TRUE(cluster->info()->eagerConnectionEstablishmentEnabled());
  EXPECT_EQ(10, cluster->info()->eagerConnectionMaxConcurrentPriming());
  EXPECT_TRUE(cluster->info()->eagerConnectionPreferReadyHosts());

  factory_.tls_.shutdownThread();
}

TEST_F(EagerConnectionEstablishmentTest, ExplicitlyDisabled) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
      eager_connection_establishment:
        enabled: false
  )EOF";

  create(parseBootstrapFromV3Yaml(yaml));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_FALSE(cluster->info()->eagerConnectionEstablishmentEnabled());

  factory_.tls_.shutdownThread();
}

TEST_F(EagerConnectionEstablishmentTest, PreferReadyHostsDisabled) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
      eager_connection_establishment:
        enabled: true
        prefer_ready_hosts:
          value: false
  )EOF";

  createWithEagerConfig(yaml);

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_TRUE(cluster->info()->eagerConnectionEstablishmentEnabled());
  EXPECT_FALSE(cluster->info()->eagerConnectionPreferReadyHosts());

  factory_.tls_.shutdownThread();
}

TEST_F(EagerConnectionEstablishmentTest, MaxConcurrentPriming) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
      eager_connection_establishment:
        enabled: true
        max_concurrent_priming:
          value: 25
  )EOF";

  createWithEagerConfig(yaml);

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_EQ(25, cluster->info()->eagerConnectionMaxConcurrentPriming());

  factory_.tls_.shutdownThread();
}

} // namespace
} // namespace Upstream
} // namespace Envoy
