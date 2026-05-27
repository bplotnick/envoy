// Tests for the per_upstream_min_connections preconnect feature.

#include "test/common/upstream/cluster_manager_impl_test_common.h"
#include "test/mocks/upstream/load_balancer_context.h"

namespace Envoy {
namespace Upstream {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnNew;

class PerUpstreamMinConnectionsTest : public ClusterManagerImplTest {
public:
  void createWithMinConnections(const std::string& yaml) {
    // Priming may allocate conn pools during initial host setup; default-mock allocator.
    ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _))
        .WillByDefault(ReturnNew<NiceMock<Http::ConnectionPool::MockInstance>>());
    create(parseBootstrapFromV3Yaml(yaml));
  }
};

TEST_F(PerUpstreamMinConnectionsTest, ConfigParsing) {
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
      preconnect_policy:
        per_upstream_min_connections:
          value: 3
  )EOF";

  createWithMinConnections(yaml);

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_EQ(3, cluster->info()->perUpstreamMinConnections());

  factory_.tls_.shutdownThread();
}

TEST_F(ClusterManagerImplTest, MinConnectionsDefaultZero) {
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
  EXPECT_EQ(0, cluster->info()->perUpstreamMinConnections());

  factory_.tls_.shutdownThread();
}

TEST_F(ClusterManagerImplTest, MinConnectionsUnsetWithOtherPreconnectFields) {
  // Setting other PreconnectPolicy fields should not enable min_connections.
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
      preconnect_policy:
        per_upstream_preconnect_ratio:
          value: 1.5
  )EOF";

  create(parseBootstrapFromV3Yaml(yaml));

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_EQ(0, cluster->info()->perUpstreamMinConnections());

  factory_.tls_.shutdownThread();
}

TEST_F(PerUpstreamMinConnectionsTest, LargeMinConnections) {
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
      preconnect_policy:
        per_upstream_min_connections:
          value: 25
  )EOF";

  createWithMinConnections(yaml);

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);
  EXPECT_EQ(25, cluster->info()->perUpstreamMinConnections());

  factory_.tls_.shutdownThread();
}

// -----------------------------------------------------------------------------
// Behavioral tests
// -----------------------------------------------------------------------------

// Test A: the floor is enforced per (host, pool-key) tuple, not per upstream host.
// When a cluster sets connection_pool_per_downstream_connection=true, each downstream
// connection produces a distinct pool key for the same upstream host. Each such pool
// independently maintains the configured floor. This is intentional: connections in
// pool A cannot serve requests routed to pool B, so the floor is only useful when
// applied per pool.
TEST_F(PerUpstreamMinConnectionsTest, FloorIsPerPoolKeyNotPerHost) {
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
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
      preconnect_policy:
        per_upstream_min_connections:
          value: 1
  )EOF";

  // Count pool allocations to demonstrate that distinct downstream connections
  // produce distinct pools for the same host.
  uint32_t pools_allocated = 0;
  ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _))
      .WillByDefault([&pools_allocated](auto&&...) {
        ++pools_allocated;
        return new NiceMock<Http::ConnectionPool::MockInstance>();
      });

  create(parseBootstrapFromV3Yaml(yaml));

  // Priming on cluster init creates the "default" pool key (no downstream connection,
  // no socket options, default priority).
  const uint32_t pools_after_priming = pools_allocated;
  EXPECT_EQ(1, pools_after_priming) << "priming should create exactly one pool";

  // Now drive a real request through with a downstream connection — this builds a
  // different hash key and forces a distinct pool to be allocated for the same host.
  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);

  NiceMock<Network::MockClientConnection> downstream_conn;
  NiceMock<MockLoadBalancerContext> lb_context;
  ON_CALL(lb_context, downstreamConnection()).WillByDefault(Return(&downstream_conn));
  // The pool hash key inclusion path calls socketOptions() and hashKey() on the
  // downstream connection. Stub both so a request with a downstream connection
  // produces a hash key distinct from the priming default.
  Network::Socket::OptionsSharedPtr empty_options = std::make_shared<Network::Socket::Options>();
  ON_CALL(downstream_conn, socketOptions()).WillByDefault(testing::ReturnRef(empty_options));
  ON_CALL(downstream_conn, hashKey(_)).WillByDefault([](std::vector<uint8_t>& hash) {
    hash.push_back(42);
  });

  auto host = cluster->chooseHost(&lb_context).host;
  ASSERT_NE(nullptr, host);
  auto opt_pool =
      cluster->httpConnPool(host, ResourcePriority::Default, Http::Protocol::Http11, &lb_context);
  EXPECT_TRUE(opt_pool.has_value());

  // We expect a second, distinct pool — same host, different pool key (downstream
  // connection ID is part of the hash). If the floor were aggregated across pools
  // per host, this would not be the case.
  EXPECT_EQ(2, pools_allocated)
      << "downstream-keyed pool should be distinct from the primed default pool";

  factory_.tls_.shutdownThread();
}

// Test B: after a primed pool drains fully and the cluster manager erases it from
// host_http_conn_pool_map_, the cluster manager re-primes the host so the floor is
// restored. The pool-internal per-close hook in ConnPoolImplBase keeps the pool
// non-empty during normal churn; this re-prime path covers the case where the pool
// was erased (e.g. transient priming failure left the pool empty, or it was drained).
TEST_F(PerUpstreamMinConnectionsTest, RefillsAfterPoolErased) {
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
      preconnect_policy:
        per_upstream_min_connections:
          value: 1
  )EOF";

  // Capture the idle callback registered by the cluster manager on the primed pool,
  // and count pool allocations.
  Http::ConnectionPool::Instance::IdleCb captured_idle_cb;
  uint32_t pools_allocated = 0;
  ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _))
      .WillByDefault([&captured_idle_cb, &pools_allocated](auto&&...) {
        ++pools_allocated;
        auto* pool = new NiceMock<Http::ConnectionPool::MockInstance>();
        ON_CALL(*pool, addIdleCallback(_))
            .WillByDefault([&captured_idle_cb](Http::ConnectionPool::Instance::IdleCb cb) {
              captured_idle_cb = std::move(cb);
            });
        return pool;
      });

  create(parseBootstrapFromV3Yaml(yaml));

  // Priming should have created one pool and registered an idle callback.
  ASSERT_EQ(1, pools_allocated);
  ASSERT_TRUE(static_cast<bool>(captured_idle_cb))
      << "cluster manager should have registered an idle callback on the primed pool";

  // Simulate the primed pool becoming fully idle (e.g., its preconnect attempt
  // failed and the pool drained). This is what the cluster manager treats as
  // "erase the pool" — after this, no pool exists for the host.
  captured_idle_cb();

  // The cluster manager should have re-primed the host, creating a new pool.
  EXPECT_EQ(2, pools_allocated) << "cluster manager should re-prime after pool erase";

  factory_.tls_.shutdownThread();
}

// Test C: when the cluster's connection circuit breaker is full at the time a host
// is added, the host is queued (no in-flight callback fires to drain it later).
// To avoid a permanently stuck queue, the cluster manager arms a retry timer that
// re-attempts the drain on a short interval. This test verifies the host ends up
// queued and the pending gauge reflects it; the timer arming is exercised by the
// underlying code path but is not directly inspected here.
TEST_F(PerUpstreamMinConnectionsTest, CircuitBreakerQueuesHostForRetry) {
  const std::string yaml = R"EOF(
  static_resources:
    clusters:
    - name: cluster_1
      connect_timeout: 0.25s
      type: STATIC
      lb_policy: ROUND_ROBIN
      circuit_breakers:
        thresholds:
        - priority: DEFAULT
          max_connections: 0
      load_assignment:
        cluster_name: cluster_1
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 127.0.0.1
                  port_value: 11001
      preconnect_policy:
        per_upstream_min_connections:
          value: 1
  )EOF";

  uint32_t pools_allocated = 0;
  ON_CALL(factory_, allocateConnPool_(_, _, _, _, _, _, _))
      .WillByDefault([&pools_allocated](auto&&...) {
        ++pools_allocated;
        return new NiceMock<Http::ConnectionPool::MockInstance>();
      });

  create(parseBootstrapFromV3Yaml(yaml));

  // Breaker max_connections=0 should have caused the host to be queued, not primed.
  EXPECT_EQ(0, pools_allocated) << "circuit breaker should block priming";

  auto* cluster = cluster_manager_->getThreadLocalCluster("cluster_1");
  ASSERT_NE(nullptr, cluster);

  // The pending gauge should have been incremented to reflect the queued host.
  EXPECT_GT(cluster->info()->trafficStats()->upstream_cx_eager_pending_.value(), 0u)
      << "queued host should show up in the pending gauge";

  factory_.tls_.shutdownThread();
}

} // namespace
} // namespace Upstream
} // namespace Envoy
