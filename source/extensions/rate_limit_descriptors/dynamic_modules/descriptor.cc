#include "source/extensions/rate_limit_descriptors/dynamic_modules/descriptor.h"

#include "source/common/common/assert.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {

DynamicModuleRateLimitDescriptor::DynamicModuleRateLimitDescriptor(
    absl::string_view descriptor_name, absl::string_view descriptor_config,
    Extensions::DynamicModules::DynamicModulePtr dynamic_module)
    : descriptor_name_(descriptor_name), descriptor_config_(descriptor_config),
      dynamic_module_(std::move(dynamic_module)) {}

DynamicModuleRateLimitDescriptor::~DynamicModuleRateLimitDescriptor() {
  if (in_module_config_ != nullptr) {
    ASSERT(on_config_destroy_ != nullptr);
    on_config_destroy_(in_module_config_);
  }
}

absl::StatusOr<std::unique_ptr<DynamicModuleRateLimitDescriptor>>
newDynamicModuleRateLimitDescriptor(const DynamicModuleRateLimitDescriptorProto& proto_config,
                                    Extensions::DynamicModules::DynamicModulePtr dynamic_module) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  auto on_config_new = dynamic_module->getFunctionPointer<OnRateLimitDescriptorConfigNewType>(
      "envoy_dynamic_module_on_rate_limit_descriptor_config_new");
  RETURN_IF_NOT_OK_REF(on_config_new.status());

  auto on_config_destroy =
      dynamic_module->getFunctionPointer<OnRateLimitDescriptorConfigDestroyType>(
          "envoy_dynamic_module_on_rate_limit_descriptor_config_destroy");
  RETURN_IF_NOT_OK_REF(on_config_destroy.status());

  auto on_populate = dynamic_module->getFunctionPointer<OnRateLimitDescriptorPopulateType>(
      "envoy_dynamic_module_on_rate_limit_descriptor_populate");
  RETURN_IF_NOT_OK_REF(on_populate.status());

  std::string descriptor_config;
  if (proto_config.has_descriptor_config()) {
    auto config_or_error = MessageUtil::knownAnyToBytes(proto_config.descriptor_config());
    RETURN_IF_NOT_OK_REF(config_or_error.status());
    descriptor_config = std::move(config_or_error.value());
  }

  auto descriptor = std::make_unique<DynamicModuleRateLimitDescriptor>(
      proto_config.descriptor_name(), descriptor_config, std::move(dynamic_module));
  descriptor->on_config_destroy_ = on_config_destroy.value();
  descriptor->on_populate_ = on_populate.value();

  envoy_dynamic_module_type_envoy_buffer name_buf = {.ptr = descriptor->descriptor_name_.data(),
                                                     .length = descriptor->descriptor_name_.size()};
  envoy_dynamic_module_type_envoy_buffer config_buf = {.ptr = descriptor->descriptor_config_.data(),
                                                       .length =
                                                           descriptor->descriptor_config_.size()};
  descriptor->in_module_config_ =
      (*on_config_new.value())(static_cast<void*>(descriptor.get()), name_buf, config_buf);
  if (descriptor->in_module_config_ == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to initialize dynamic module rate limit descriptor config");
  }
  return descriptor;
}

bool DynamicModuleRateLimitDescriptor::populateDescriptor(
    RateLimit::DescriptorEntry& descriptor_entry, const std::string& local_service_cluster,
    const Http::RequestHeaderMap& headers, const StreamInfo::StreamInfo& info) const {
  RateLimitDescriptorContext context{local_service_cluster, headers, info, {}};
  if (!on_populate_(in_module_config_, static_cast<void*>(&context))) {
    return false;
  }
  descriptor_entry = std::move(context.descriptor_entry);
  return true;
}

} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
