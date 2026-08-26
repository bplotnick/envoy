#pragma once

#include <memory>
#include <string>

#include "envoy/extensions/rate_limit_descriptors/dynamic_modules/v3/dynamic_modules.pb.h"
#include "envoy/ratelimit/ratelimit.h"
#include "envoy/stream_info/filter_state.h"

#include "source/common/common/statusor.h"
#include "source/extensions/dynamic_modules/abi/abi.h"
#include "source/extensions/dynamic_modules/dynamic_modules.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {

using DynamicModuleRateLimitDescriptorProto = envoy::extensions::rate_limit_descriptors::
    dynamic_modules::v3::DynamicModuleRateLimitDescriptor;

using OnRateLimitDescriptorConfigNewType =
    decltype(&envoy_dynamic_module_on_rate_limit_descriptor_config_new);
using OnRateLimitDescriptorConfigDestroyType =
    decltype(&envoy_dynamic_module_on_rate_limit_descriptor_config_destroy);
using OnRateLimitDescriptorPopulateType =
    decltype(&envoy_dynamic_module_on_rate_limit_descriptor_populate);
using DynamicModuleSharedPtr = std::shared_ptr<Extensions::DynamicModules::DynamicModule>;

class DynamicModuleRateLimitDescriptor : public RateLimit::DescriptorProducer,
                                         public RateLimit::DescriptorProducerWithMutableStreamInfo {
public:
  DynamicModuleRateLimitDescriptor(absl::string_view descriptor_name,
                                   absl::string_view descriptor_config,
                                   Extensions::DynamicModules::DynamicModulePtr dynamic_module);
  ~DynamicModuleRateLimitDescriptor() override;

  // RateLimit::DescriptorProducer
  bool populateDescriptor(RateLimit::DescriptorEntry& descriptor_entry,
                          const std::string& local_service_cluster,
                          const Http::RequestHeaderMap& headers,
                          const StreamInfo::StreamInfo& info) const override;

  // RateLimit::DescriptorProducerWithMutableStreamInfo
  bool populateDescriptorWithMutableStreamInfo(RateLimit::DescriptorEntry& descriptor_entry,
                                               const std::string& local_service_cluster,
                                               const Http::RequestHeaderMap& headers,
                                               StreamInfo::StreamInfo& info) const override;

  envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr in_module_config_{nullptr};
  OnRateLimitDescriptorConfigDestroyType on_config_destroy_{nullptr};
  OnRateLimitDescriptorPopulateType on_populate_{nullptr};

  const DynamicModuleSharedPtr& dynamicModuleSharedPtr() const { return dynamic_module_; }

private:
  friend absl::StatusOr<std::unique_ptr<DynamicModuleRateLimitDescriptor>>
  newDynamicModuleRateLimitDescriptor(const DynamicModuleRateLimitDescriptorProto& proto_config,
                                      Extensions::DynamicModules::DynamicModulePtr dynamic_module);

  bool populateDescriptorImpl(RateLimit::DescriptorEntry& descriptor_entry,
                              const std::string& local_service_cluster,
                              const Http::RequestHeaderMap& headers,
                              const StreamInfo::StreamInfo& info,
                              StreamInfo::FilterState* mutable_filter_state) const;

  const std::string descriptor_name_;
  const std::string descriptor_config_;
  const DynamicModuleSharedPtr dynamic_module_;
};

absl::StatusOr<std::unique_ptr<DynamicModuleRateLimitDescriptor>>
newDynamicModuleRateLimitDescriptor(const DynamicModuleRateLimitDescriptorProto& proto_config,
                                    Extensions::DynamicModules::DynamicModulePtr dynamic_module);

struct RateLimitDescriptorContext {
  const DynamicModuleSharedPtr& dynamic_module;
  const std::string& local_service_cluster;
  const Http::RequestHeaderMap& headers;
  const StreamInfo::StreamInfo& stream_info;
  const StreamInfo::FilterState* filter_state;
  StreamInfo::FilterState* mutable_filter_state;
  RateLimit::DescriptorEntry descriptor_entry;
};

} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
