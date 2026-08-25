#include "envoy/extensions/rate_limit_descriptors/dynamic_modules/v3/dynamic_modules.pb.h"
#include "envoy/registry/registry.h"

#include "source/extensions/rate_limit_descriptors/dynamic_modules/config.h"

#include "test/extensions/dynamic_modules/util.h"
#include "test/mocks/server/server_factory_context.h"
#include "test/test_common/status_utility.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {
namespace {

using ::Envoy::Extensions::DynamicModules::failureCounter;
using StatusHelpers::HasStatusMessage;
using ::testing::HasSubstr;
using ::testing::NiceMock;

using DynamicModuleRateLimitDescriptorProto = envoy::extensions::rate_limit_descriptors::
    dynamic_modules::v3::DynamicModuleRateLimitDescriptor;

DynamicModuleRateLimitDescriptorProto protoConfig(absl::string_view module_name) {
  DynamicModuleRateLimitDescriptorProto config;
  config.mutable_dynamic_module_config()->set_name(module_name);
  config.mutable_dynamic_module_config()->set_do_not_close(true);
  config.set_descriptor_name("test_descriptor");
  return config;
}

class DynamicModuleRateLimitDescriptorFactoryTest : public testing::Test {
public:
  DynamicModuleRateLimitDescriptorFactoryTest() {
    TestEnvironment::setEnvVar("ENVOY_DYNAMIC_MODULES_SEARCH_PATH",
                               TestEnvironment::substitute(
                                   "{{ test_rundir }}/test/extensions/dynamic_modules/test_data/c"),
                               1);
  }

  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  DynamicModuleRateLimitDescriptorFactory factory_;
};

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, FactoryNameAndRegistration) {
  EXPECT_EQ("envoy.rate_limit_descriptors.dynamic_modules", factory_.name());
  EXPECT_NE(nullptr, Registry::FactoryRegistry<RateLimit::DescriptorProducerFactory>::getFactory(
                         "envoy.rate_limit_descriptors.dynamic_modules"));
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, CreateEmptyConfigProto) {
  auto config = factory_.createEmptyConfigProto();
  EXPECT_NE(nullptr, dynamic_cast<DynamicModuleRateLimitDescriptorProto*>(config.get()));
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, ValidConfig) {
  auto config = protoConfig("rate_limit_descriptor_no_op");
  auto descriptor = factory_.createDescriptorProducerFromProto(config, context_);
  ASSERT_TRUE(descriptor.ok());
  EXPECT_NE(nullptr, descriptor.value());
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, ValidLocalFileConfig) {
  DynamicModuleRateLimitDescriptorProto config;
  config.mutable_dynamic_module_config()->mutable_module()->mutable_local()->set_filename(
      Extensions::DynamicModules::testSharedObjectPath("rate_limit_descriptor_no_op", "c"));
  config.mutable_dynamic_module_config()->set_do_not_close(true);
  config.set_descriptor_name("test_descriptor");
  auto descriptor = factory_.createDescriptorProducerFromProto(config, context_);
  ASSERT_TRUE(descriptor.ok());
  EXPECT_NE(nullptr, descriptor.value());
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, MissingDynamicModuleConfigRejected) {
  DynamicModuleRateLimitDescriptorProto config;
  EXPECT_THROW(static_cast<void>(factory_.createDescriptorProducerFromProto(config, context_)),
               ProtoValidationException);
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, InvalidModule) {
  auto config = protoConfig("does_not_exist");
  EXPECT_THAT(factory_.createDescriptorProducerFromProto(config, context_).status(),
              HasStatusMessage(HasSubstr("Failed to load")));
  EXPECT_EQ(1U, failureCounter(context_.scope(), "module_load_error", "test_descriptor"));
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, MissingConfigNew) {
  auto config = protoConfig("rate_limit_descriptor_missing_config_new");
  EXPECT_THAT(factory_.createDescriptorProducerFromProto(config, context_).status(),
              HasStatusMessage(HasSubstr("config_new")));
  EXPECT_EQ(1U, failureCounter(context_.scope(), "module_load_error", "test_descriptor"));
  EXPECT_EQ(0U, failureCounter(context_.scope(), "config_init_error", "test_descriptor"));
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, MissingConfigDestroy) {
  auto config = protoConfig("rate_limit_descriptor_missing_config_destroy");
  EXPECT_THAT(factory_.createDescriptorProducerFromProto(config, context_).status(),
              HasStatusMessage(HasSubstr("config_destroy")));
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, MissingPopulate) {
  auto config = protoConfig("rate_limit_descriptor_missing_populate");
  EXPECT_THAT(factory_.createDescriptorProducerFromProto(config, context_).status(),
              HasStatusMessage(HasSubstr("populate")));
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, ConfigNewReturnsNull) {
  auto config = protoConfig("rate_limit_descriptor_config_new_fail");
  EXPECT_THAT(factory_.createDescriptorProducerFromProto(config, context_).status(),
              HasStatusMessage(HasSubstr("Failed to initialize")));
  EXPECT_EQ(1U, failureCounter(context_.scope(), "config_init_error", "test_descriptor"));
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, MalformedDescriptorConfigRejected) {
  auto config = protoConfig("rate_limit_descriptor_no_op");
  config.mutable_descriptor_config()->set_type_url(
      "type.googleapis.com/google.protobuf.StringValue");
  config.mutable_descriptor_config()->set_value(std::string("\x0a\x05\x41", 3));
  EXPECT_FALSE(factory_.createDescriptorProducerFromProto(config, context_).ok());
}

TEST_F(DynamicModuleRateLimitDescriptorFactoryTest, RemoteSourceRejectedOnCacheMiss) {
  DynamicModuleRateLimitDescriptorProto config;
  auto* remote = config.mutable_dynamic_module_config()->mutable_module()->mutable_remote();
  remote->mutable_http_uri()->set_uri("https://example.com/module.so");
  remote->mutable_http_uri()->set_cluster("cluster_1");
  remote->mutable_http_uri()->mutable_timeout()->set_seconds(1);
  remote->set_sha256("abc123");
  config.set_descriptor_name("test_descriptor");
  EXPECT_FALSE(factory_.createDescriptorProducerFromProto(config, context_).ok());
}

} // namespace
} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
