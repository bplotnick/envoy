#include "source/extensions/rate_limit_descriptors/dynamic_modules/descriptor.h"

#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {
namespace {

using ::testing::NiceMock;

int filter_state_object_destructor_calls = 0;

void filterStateObjectDestructor(envoy_dynamic_module_type_filter_state_object_module_ptr object) {
  filter_state_object_destructor_calls++;
  delete static_cast<int*>(object);
}

class RateLimitDescriptorAbiTest : public testing::Test {
public:
  envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr contextPtr() {
    return static_cast<void*>(&context_);
  }

  std::string local_service_cluster_ = "local-cluster";
  Http::TestRequestHeaderMapImpl headers_{{":path", "/"}, {"x-test", "first"}};
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
  DynamicModuleSharedPtr dynamic_module_{
      std::make_shared<Extensions::DynamicModules::DynamicModule>(static_cast<void*>(nullptr))};
  RateLimitDescriptorContext context_{dynamic_module_,
                                      local_service_cluster_,
                                      headers_,
                                      stream_info_,
                                      stream_info_.filter_state_.get(),
                                      stream_info_.filter_state_.get(),
                                      {}};
};

TEST_F(RateLimitDescriptorAbiTest, LocalServiceCluster) {
  envoy_dynamic_module_type_envoy_buffer result;
  EXPECT_TRUE(envoy_dynamic_module_callback_rate_limit_descriptor_get_local_service_cluster(
      contextPtr(), &result));
  EXPECT_EQ("local-cluster", absl::string_view(result.ptr, result.length));

  local_service_cluster_.clear();
  EXPECT_FALSE(envoy_dynamic_module_callback_rate_limit_descriptor_get_local_service_cluster(
      contextPtr(), &result));
}

TEST_F(RateLimitDescriptorAbiTest, RequestHeaders) {
  headers_.addCopy(Http::LowerCaseString("x-test"), "second");
  EXPECT_EQ(
      headers_.size(),
      envoy_dynamic_module_callback_rate_limit_descriptor_get_request_headers_size(contextPtr()));

  std::vector<envoy_dynamic_module_type_envoy_http_header> result(headers_.size());
  EXPECT_TRUE(envoy_dynamic_module_callback_rate_limit_descriptor_get_request_headers(
      contextPtr(), result.data()));
  EXPECT_EQ(":path", absl::string_view(result[0].key_ptr, result[0].key_length));

  envoy_dynamic_module_type_module_buffer key{"x-test", 6};
  envoy_dynamic_module_type_envoy_buffer value;
  size_t total_count = 0;
  EXPECT_TRUE(envoy_dynamic_module_callback_rate_limit_descriptor_get_request_header_value(
      contextPtr(), key, &value, 1, &total_count));
  EXPECT_EQ("second", absl::string_view(value.ptr, value.length));
  EXPECT_EQ(2, total_count);
  EXPECT_FALSE(envoy_dynamic_module_callback_rate_limit_descriptor_get_request_header_value(
      contextPtr(), key, &value, 2, nullptr));
}

TEST_F(RateLimitDescriptorAbiTest, SetDescriptorEntryCopiesBuffers) {
  std::string key_string = "key";
  std::string value_string = "value";
  envoy_dynamic_module_type_module_buffer key{key_string.data(), key_string.size()};
  envoy_dynamic_module_type_module_buffer value{value_string.data(), value_string.size()};
  envoy_dynamic_module_callback_rate_limit_descriptor_set_descriptor_entry(contextPtr(), key,
                                                                           value);
  key_string.clear();
  value_string.clear();
  EXPECT_EQ("key", context_.descriptor_entry.key_);
  EXPECT_EQ("value", context_.descriptor_entry.value_);
}

TEST_F(RateLimitDescriptorAbiTest, MissingAttributesAndMetadata) {
  envoy_dynamic_module_type_envoy_buffer string_result;
  uint64_t int_result = 0;
  bool bool_result = false;
  EXPECT_FALSE(envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_string(
      contextPtr(), static_cast<envoy_dynamic_module_type_attribute_id>(-1), &string_result));
  EXPECT_FALSE(envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_int(
      contextPtr(), static_cast<envoy_dynamic_module_type_attribute_id>(-1), &int_result));
  EXPECT_FALSE(envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_bool(
      contextPtr(), static_cast<envoy_dynamic_module_type_attribute_id>(-1), &bool_result));

  envoy_dynamic_module_type_module_buffer filter_name{"missing", 7};
  envoy_dynamic_module_type_module_buffer path{"value", 5};
  EXPECT_FALSE(envoy_dynamic_module_callback_rate_limit_descriptor_get_dynamic_metadata(
      contextPtr(), filter_name, path, &string_result));
}

TEST_F(RateLimitDescriptorAbiTest, FilterStateObjectSharedAndDestroyed) {
  filter_state_object_destructor_calls = 0;
  envoy_dynamic_module_type_module_buffer key{"envoy.test.memo", 15};
  auto* object = new int(42);
  EXPECT_TRUE(envoy_dynamic_module_callback_rate_limit_descriptor_set_filter_state_object(
      contextPtr(), key, object, filterStateObjectDestructor));
  EXPECT_EQ(object, envoy_dynamic_module_callback_rate_limit_descriptor_get_filter_state_object(
                        contextPtr(), key));

  stream_info_.filter_state_.reset();
  EXPECT_EQ(1, filter_state_object_destructor_calls);
}

TEST_F(RateLimitDescriptorAbiTest, FilterStateObjectStoreFailureDestroysObject) {
  filter_state_object_destructor_calls = 0;
  stream_info_.filter_state_.reset();
  context_.filter_state = nullptr;
  context_.mutable_filter_state = nullptr;
  envoy_dynamic_module_type_module_buffer key{"envoy.test.memo", 15};
  EXPECT_FALSE(envoy_dynamic_module_callback_rate_limit_descriptor_set_filter_state_object(
      contextPtr(), key, new int(42), filterStateObjectDestructor));
  EXPECT_EQ(1, filter_state_object_destructor_calls);
  EXPECT_EQ(nullptr, envoy_dynamic_module_callback_rate_limit_descriptor_get_filter_state_object(
                         contextPtr(), key));
}

} // namespace
} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
