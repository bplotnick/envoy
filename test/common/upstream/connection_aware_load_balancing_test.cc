// Tests for the connection-aware load balancing feature.

#include "test/common/upstream/cluster_manager_impl_test_common.h"
#include "test/mocks/upstream/load_balancer_context.h"

namespace Envoy {
namespace Upstream {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

TEST_F(ClusterManagerImplTest, ConnectionAwareLBConfigParsing) {
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
      connection_aware_load_balancing:
        enabled: true
  )EOF";

  create(parseBootstrapFromV3Yaml(yaml));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_TRUE(cluster->info()->connectionAwareLoadBalancingEnabled());

  factory_.tls_.shutdownThread();
}

TEST_F(ClusterManagerImplTest, ConnectionAwareLBDisabledByDefault) {
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
  EXPECT_FALSE(cluster->info()->connectionAwareLoadBalancingEnabled());

  factory_.tls_.shutdownThread();
}

TEST_F(ClusterManagerImplTest, ConnectionAwareLBExplicitlyDisabled) {
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
      connection_aware_load_balancing:
        enabled: false
  )EOF";

  create(parseBootstrapFromV3Yaml(yaml));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_FALSE(cluster->info()->connectionAwareLoadBalancingEnabled());

  factory_.tls_.shutdownThread();
}

// -----------------------------------------------------------------------------
// Behavioral tests
// -----------------------------------------------------------------------------

namespace {
constexpr char kFourHostRoundRobin[] = R"EOF(
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
              address: { socket_address: { address: 127.0.0.1, port_value: 11001 } }
          - endpoint:
              address: { socket_address: { address: 127.0.0.1, port_value: 11002 } }
          - endpoint:
              address: { socket_address: { address: 127.0.0.1, port_value: 11003 } }
          - endpoint:
              address: { socket_address: { address: 127.0.0.1, port_value: 11004 } }
      connection_aware_load_balancing:
        enabled: true
  )EOF";
} // namespace

// Test E: when hosts are warm, connection-aware LB must not perturb the round-robin
// cursor. Because host selection now uses the LB's own host-rejection retry loop
// (shouldSelectAnotherHost) rather than re-invoking chooseHost from the cluster
// manager, a warm host is accepted on the first attempt and the cursor advances
// exactly once per request. Four consecutive picks on a 4-host cluster therefore
// cycle through all four hosts. (The earlier re-pick implementation advanced the
// cursor 4× per request and collapsed all traffic onto one host.)
TEST_F(ClusterManagerImplTest, ConnectionAwareLBWarmHostsCycleNormally) {
  // Every allocated pool reports a ready connection, so once a host has a pool it is
  // considered warm.
  ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _)).WillByDefault([](auto&&...) {
    auto* pool = new NiceMock<Http::ConnectionPool::MockInstance>();
    ON_CALL(*pool, hasReadyConnection()).WillByDefault(Return(true));
    return pool;
  });

  create(parseBootstrapFromV3Yaml(kFourHostRoundRobin));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);

  // Pre-warm all four hosts by creating a (ready) pool for each.
  for (const auto& host_set : cluster->prioritySet().hostSetsPerPriority()) {
    for (const auto& host : host_set->hosts()) {
      auto opt_pool =
          cluster->httpConnPool(host, ResourcePriority::Default, Http::Protocol::Http11, nullptr);
      ASSERT_TRUE(opt_pool.has_value());
    }
  }

  // With all hosts warm, four consecutive picks should cycle through all four hosts.
  absl::flat_hash_set<std::string> distinct_hosts;
  for (int i = 0; i < 4; ++i) {
    auto host = cluster->chooseHost(nullptr).host;
    ASSERT_NE(nullptr, host);
    distinct_hosts.insert(host->address()->asString());
  }

  EXPECT_EQ(4U, distinct_hosts.size())
      << "round-robin over warm hosts should cycle through all four; cursor must advance once/pick";

  factory_.tls_.shutdownThread();
}

// Test E': when every host is cold (cold start), connection-aware LB must still
// return a host rather than starving. The LB's retry loop bounds out and returns
// its last pick.
TEST_F(ClusterManagerImplTest, ConnectionAwareLBAllColdStillReturnsHost) {
  // Pools never report ready, so no host is ever warm.
  ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _)).WillByDefault([](auto&&...) {
    return new NiceMock<Http::ConnectionPool::MockInstance>();
  });

  create(parseBootstrapFromV3Yaml(kFourHostRoundRobin));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);

  // Even though no host has a ready connection, chooseHost must return a host
  // (cold-start fallback) rather than nullptr.
  auto host = cluster->chooseHost(nullptr).host;
  EXPECT_NE(nullptr, host) << "cold start must fall back to a host, not starve";

  factory_.tls_.shutdownThread();
}

// Test F: the stimulation path passes the request's LoadBalancerContext to
// httpConnPoolImpl, so the pool it primes uses the same socket/transport options
// and downstream-connection hash as the real request. For a cluster with
// connection_pool_per_downstream_connection=true, stimulation therefore primes the
// same pool the request will use, rather than a default-keyed pool the request
// would never touch.
TEST_F(ClusterManagerImplTest, ConnectionAwareLBStimulatesRequestPool) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      connection_pool_per_downstream_connection: true
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address: { socket_address: { address: 127.0.0.1, port_value: 11001 } }
      connection_aware_load_balancing:
        enabled: true
  )EOF";

  // Track pools created so we can compare the stimulation pool to the request pool.
  std::vector<Http::ConnectionPool::MockInstance*> pools;
  ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _)).WillByDefault([&pools](auto&&...) {
    auto* pool = new NiceMock<Http::ConnectionPool::MockInstance>();
    pools.push_back(pool);
    return pool;
  });

  create(parseBootstrapFromV3Yaml(yaml));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);

  // Set up a downstream connection and LB context so the *request* path produces
  // a per-downstream pool key.
  NiceMock<Network::MockClientConnection> downstream_conn;
  NiceMock<MockLoadBalancerContext> lb_context;
  ON_CALL(lb_context, downstreamConnection()).WillByDefault(Return(&downstream_conn));
  Network::Socket::OptionsSharedPtr empty_options = std::make_shared<Network::Socket::Options>();
  ON_CALL(downstream_conn, socketOptions()).WillByDefault(testing::ReturnRef(empty_options));
  ON_CALL(downstream_conn, hashKey(_)).WillByDefault([](std::vector<uint8_t>& hash) {
    hash.push_back(77);
  });

  // chooseHost triggers stimulation (no ready connection on the host yet). The
  // stimulation passes the request's context to httpConnPoolImpl, so it builds the
  // same pool key the request will use. Track which pool got stimulated.
  auto host = cluster->chooseHost(&lb_context).host;
  ASSERT_NE(nullptr, host);
  ASSERT_FALSE(pools.empty()) << "stimulation should have allocated a pool";
  auto* stimulated_pool = pools.back();

  // Now drive the actual request through with the same downstream-connection-bearing
  // lb_context. This must resolve to the pool the stimulation already created.
  auto opt_pool =
      cluster->httpConnPool(host, ResourcePriority::Default, Http::Protocol::Http11, &lb_context);
  ASSERT_TRUE(opt_pool.has_value());
  auto* request_pool = HttpPoolDataPeer::getPool(opt_pool);
  ASSERT_NE(nullptr, request_pool);

  // The stimulation primed the same pool the request uses, so the warmth applies.
  EXPECT_EQ(stimulated_pool, request_pool)
      << "stimulation should prime the same pool the request will use";

  factory_.tls_.shutdownThread();
}

} // namespace
} // namespace Upstream
} // namespace Envoy
