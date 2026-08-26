#include "envoy/config/route/v3/route_components.pb.h"

#include "source/common/protobuf/protobuf.h"
#include "source/extensions/filters/common/ratelimit_config/ratelimit_config.h"
#include "source/extensions/rate_limit_descriptors/dynamic_modules/config.h"
#include "source/extensions/rate_limit_descriptors/dynamic_modules/descriptor.h"

#include "test/extensions/dynamic_modules/util.h"
#include "test/mocks/server/server_factory_context.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {
namespace {

class DynamicModuleRateLimitDescriptorTest : public testing::Test {
public:
  DynamicModuleRateLimitDescriptorTest() {
    TestEnvironment::setEnvVar("ENVOY_DYNAMIC_MODULES_SEARCH_PATH",
                               TestEnvironment::substitute(
                                   "{{ test_rundir }}/test/extensions/dynamic_modules/test_data/c"),
                               1);
  }

  std::unique_ptr<DynamicModuleRateLimitDescriptor>
  createDescriptor(absl::string_view module_name, absl::string_view descriptor_name = "key",
                   absl::string_view descriptor_config = "fallback") {
    auto module = Extensions::DynamicModules::newDynamicModuleByName(module_name, true, false);
    EXPECT_TRUE(module.ok());
    return createDescriptor(std::move(module.value()), descriptor_name, descriptor_config);
  }

  std::unique_ptr<DynamicModuleRateLimitDescriptor>
  createDescriptor(Extensions::DynamicModules::DynamicModulePtr module,
                   absl::string_view descriptor_name, absl::string_view descriptor_config) {
    DynamicModuleRateLimitDescriptorProto config;
    config.set_descriptor_name(descriptor_name);
    Protobuf::StringValue value;
    value.set_value(descriptor_config);
    EXPECT_TRUE(config.mutable_descriptor_config()->PackFrom(value));
    auto descriptor = newDynamicModuleRateLimitDescriptor(config, std::move(module));
    EXPECT_TRUE(descriptor.ok());
    return std::move(descriptor.value());
  }

  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
};

TEST_F(DynamicModuleRateLimitDescriptorTest, ProducesEntryFromRequestHeader) {
  auto descriptor = createDescriptor("rate_limit_descriptor_callbacks");
  Http::TestRequestHeaderMapImpl headers{{"x-rate-limit-value", "header-value"}};
  RateLimit::DescriptorEntry entry;
  EXPECT_TRUE(descriptor->populateDescriptor(entry, "local-cluster", headers, stream_info_));
  EXPECT_EQ("key", entry.key_);
  EXPECT_EQ("header-value", entry.value_);
}

TEST_F(DynamicModuleRateLimitDescriptorTest, ProducesEntryFromLocalServiceCluster) {
  auto descriptor = createDescriptor("rate_limit_descriptor_callbacks");
  Http::TestRequestHeaderMapImpl headers;
  RateLimit::DescriptorEntry entry;
  EXPECT_TRUE(descriptor->populateDescriptor(entry, "local-cluster", headers, stream_info_));
  EXPECT_EQ("key", entry.key_);
  EXPECT_EQ("local-cluster", entry.value_);
}

TEST_F(DynamicModuleRateLimitDescriptorTest, ProducesEntryFromConfigFallback) {
  auto descriptor = createDescriptor("rate_limit_descriptor_callbacks");
  Http::TestRequestHeaderMapImpl headers;
  RateLimit::DescriptorEntry entry;
  EXPECT_TRUE(descriptor->populateDescriptor(entry, "", headers, stream_info_));
  EXPECT_EQ("key", entry.key_);
  EXPECT_EQ("fallback", entry.value_);
}

TEST_F(DynamicModuleRateLimitDescriptorTest, CanSkipEntry) {
  auto descriptor = createDescriptor("rate_limit_descriptor_callbacks");
  Http::TestRequestHeaderMapImpl headers{{"x-skip-descriptor", "true"}};
  RateLimit::DescriptorEntry entry;
  EXPECT_TRUE(descriptor->populateDescriptor(entry, "local-cluster", headers, stream_info_));
  EXPECT_TRUE(entry.key_.empty());
  EXPECT_TRUE(entry.value_.empty());
}

TEST_F(DynamicModuleRateLimitDescriptorTest, CanAbortDescriptorGeneration) {
  auto descriptor = createDescriptor("rate_limit_descriptor_callbacks");
  Http::TestRequestHeaderMapImpl headers{{"x-abort-descriptor", "true"}};
  RateLimit::DescriptorEntry entry;
  EXPECT_FALSE(descriptor->populateDescriptor(entry, "local-cluster", headers, stream_info_));
}

TEST_F(DynamicModuleRateLimitDescriptorTest, NoOpModuleSkipsEntry) {
  auto descriptor = createDescriptor("rate_limit_descriptor_no_op");
  Http::TestRequestHeaderMapImpl headers;
  RateLimit::DescriptorEntry entry;
  EXPECT_TRUE(descriptor->populateDescriptor(entry, "local-cluster", headers, stream_info_));
  EXPECT_TRUE(entry.key_.empty());
}

TEST_F(DynamicModuleRateLimitDescriptorTest, RustSdkModuleProducesEntry) {
  auto module = Extensions::DynamicModules::newDynamicModule(
      Extensions::DynamicModules::testSharedObjectPath("rate_limit_descriptor_integration_test",
                                                       "rust"),
      true);
  ASSERT_TRUE(module.ok()) << module.status();
  auto descriptor = createDescriptor(std::move(module.value()), "rust-key", "fallback");
  Http::TestRequestHeaderMapImpl headers{{"x-rate-limit-value", "rust-value"}};
  RateLimit::DescriptorEntry entry;
  EXPECT_TRUE(descriptor->populateDescriptorWithMutableStreamInfo(entry, "local-cluster", headers,
                                                                  stream_info_));
  EXPECT_EQ("rust-key", entry.key_);
  EXPECT_EQ("rust-value", entry.value_);

  headers.setCopy(Http::LowerCaseString("x-rate-limit-value"), "changed-after-cache");
  auto second_module = Extensions::DynamicModules::newDynamicModule(
      Extensions::DynamicModules::testSharedObjectPath("rate_limit_descriptor_integration_test",
                                                       "rust"),
      true);
  ASSERT_TRUE(second_module.ok()) << second_module.status();
  auto second_descriptor =
      createDescriptor(std::move(second_module.value()), "rust-key", "fallback");
  RateLimit::DescriptorEntry second_entry;
  EXPECT_TRUE(second_descriptor->populateDescriptorWithMutableStreamInfo(
      second_entry, "local-cluster", headers, stream_info_));
  EXPECT_EQ("rust-value", second_entry.value_);
}

TEST_F(DynamicModuleRateLimitDescriptorTest, LocalRateLimitPolicyUsesDynamicModuleDescriptor) {
  envoy::config::route::v3::RateLimit rate_limit_config;
  auto* extension = rate_limit_config.add_actions()->mutable_extension();
  extension->set_name("envoy.rate_limit_descriptors.dynamic_modules");

  DynamicModuleRateLimitDescriptorProto descriptor_config;
  descriptor_config.mutable_dynamic_module_config()->set_name("rate_limit_descriptor_callbacks");
  descriptor_config.mutable_dynamic_module_config()->set_do_not_close(true);
  descriptor_config.set_descriptor_name("local-key");
  ASSERT_TRUE(extension->mutable_typed_config()->PackFrom(descriptor_config));

  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  absl::Status creation_status;
  Filters::Common::RateLimit::RateLimitPolicy policy(rate_limit_config, context, creation_status);
  ASSERT_TRUE(creation_status.ok()) << creation_status;

  Http::TestRequestHeaderMapImpl headers{{"x-rate-limit-value", "local-value"}};
  std::vector<RateLimit::Descriptor> descriptors;
  policy.populateDescriptors(headers, stream_info_, "local-cluster", descriptors);
  ASSERT_EQ(1, descriptors.size());
  ASSERT_EQ(1, descriptors[0].entries_.size());
  EXPECT_EQ("local-key", descriptors[0].entries_[0].key_);
  EXPECT_EQ("local-value", descriptors[0].entries_[0].value_);

  headers.setCopy(Http::LowerCaseString("x-abort-descriptor"), "true");
  descriptors.clear();
  policy.populateDescriptors(headers, stream_info_, "local-cluster", descriptors);
  EXPECT_TRUE(descriptors.empty());
}

} // namespace
} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
