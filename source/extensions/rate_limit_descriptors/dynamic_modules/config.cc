#include "source/extensions/rate_limit_descriptors/dynamic_modules/config.h"

#include "envoy/extensions/rate_limit_descriptors/dynamic_modules/v3/dynamic_modules.pb.h"
#include "envoy/extensions/rate_limit_descriptors/dynamic_modules/v3/dynamic_modules.pb.validate.h"
#include "envoy/registry/registry.h"

#include "source/common/protobuf/utility.h"
#include "source/extensions/dynamic_modules/dynamic_module_stats.h"
#include "source/extensions/dynamic_modules/dynamic_modules.h"
#include "source/extensions/rate_limit_descriptors/dynamic_modules/descriptor.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {

using DynamicModuleRateLimitDescriptorProto = envoy::extensions::rate_limit_descriptors::
    dynamic_modules::v3::DynamicModuleRateLimitDescriptor;

std::string DynamicModuleRateLimitDescriptorFactory::name() const {
  return "envoy.rate_limit_descriptors.dynamic_modules";
}

ProtobufTypes::MessagePtr DynamicModuleRateLimitDescriptorFactory::createEmptyConfigProto() {
  return std::make_unique<DynamicModuleRateLimitDescriptorProto>();
}

absl::StatusOr<RateLimit::DescriptorProducerPtr>
DynamicModuleRateLimitDescriptorFactory::createDescriptorProducerFromProto(
    const Protobuf::Message& config, Server::Configuration::CommonFactoryContext& context) {
  const auto& proto_config =
      MessageUtil::downcastAndValidate<const DynamicModuleRateLimitDescriptorProto&>(
          config, context.messageValidationVisitor());

  auto load_result = Extensions::DynamicModules::newDynamicModuleByConfig(
      proto_config.dynamic_module_config(), proto_config.descriptor_name(), context);
  RETURN_IF_NOT_OK_REF(load_result.status());

  auto descriptor =
      newDynamicModuleRateLimitDescriptor(proto_config, std::move(load_result->loaded));
  if (!descriptor.ok()) {
    const absl::string_view failure_stat =
        descriptor.status().message().starts_with("Failed to resolve symbol")
            ? Extensions::DynamicModules::ModuleLoadErrorStat
            : Extensions::DynamicModules::ConfigInitErrorStat;
    Extensions::DynamicModules::incrementLoadFailure(context, proto_config.descriptor_name(),
                                                     failure_stat);
    return descriptor.status();
  }
  return std::move(descriptor.value());
}

REGISTER_FACTORY(DynamicModuleRateLimitDescriptorFactory, RateLimit::DescriptorProducerFactory);

} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
