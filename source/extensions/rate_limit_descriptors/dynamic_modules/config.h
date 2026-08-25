#pragma once

#include "envoy/ratelimit/ratelimit.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {

class DynamicModuleRateLimitDescriptorFactory : public RateLimit::DescriptorProducerFactory {
public:
  std::string name() const override;
  ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  absl::StatusOr<RateLimit::DescriptorProducerPtr>
  createDescriptorProducerFromProto(const Protobuf::Message& config,
                                    Server::Configuration::CommonFactoryContext& context) override;
};

} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
