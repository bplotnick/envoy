#include "envoy/api/v2/rds.pb.h"
#include "envoy/service/overload/v3/odds.pb.h"
#include "envoy/service/route/v3/rds.pb.h"

#include "source/common/config/type_to_endpoint.h"

#include "test/config/v2_link_hacks.h"

#include "gtest/gtest.h"

// API_NO_BOOST_FILE

namespace Envoy {
namespace Config {
namespace {

// Verify type-to-endpoint methods with RDS as an exemplar.
TEST(TypeToEndpoint, All) {
  // The dummy messages are included for link purposes only.
  envoy::service::route::v3::RdsDummy _v3_rds_dummy [[maybe_unused]];

  // Delta gRPC endpoints.
  EXPECT_EQ(
      "envoy.service.route.v3.RouteDiscoveryService.DeltaRoutes",
      deltaGrpcMethod("type.googleapis.com/envoy.config.route.v3.RouteConfiguration").full_name());

  // SotW gRPC endpoints.
  EXPECT_EQ(
      "envoy.service.route.v3.RouteDiscoveryService.StreamRoutes",
      sotwGrpcMethod("type.googleapis.com/envoy.config.route.v3.RouteConfiguration").full_name());

  // REST endpoints.
  EXPECT_EQ("envoy.service.route.v3.RouteDiscoveryService.FetchRoutes",
            restMethod("type.googleapis.com/envoy.config.route.v3.RouteConfiguration").full_name());
}

TEST(TypeToEndpoint, OverloadDiscovery) {
  envoy::service::overload::v3::OddsDummy dummy [[maybe_unused]];
  constexpr absl::string_view type_url =
      "type.googleapis.com/envoy.config.overload.v3.OverloadManager";
  EXPECT_EQ("envoy.service.overload.v3.OverloadDiscoveryService.DeltaOverload",
            deltaGrpcMethod(type_url).full_name());
  EXPECT_EQ("envoy.service.overload.v3.OverloadDiscoveryService.StreamOverload",
            sotwGrpcMethod(type_url).full_name());
  EXPECT_EQ("envoy.service.overload.v3.OverloadDiscoveryService.FetchOverload",
            restMethod(type_url).full_name());
}

} // namespace
} // namespace Config
} // namespace Envoy
