#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "envoy/config/core/v3/base.pb.h"
#include "envoy/http/filter.h"
#include "envoy/ratelimit/ratelimit.h"

namespace Envoy {
namespace Router {

/**
 * Base interface for generic rate limit override action.
 */
class RateLimitOverrideAction {
public:
  virtual ~RateLimitOverrideAction() = default;

  /**
   * Potentially populate the descriptors 'limit' property with a RateLimitOverride instance
   * @param descriptor supplies the descriptor to optionally fill.
   * @param metadata supplies the dynamic metadata for the request.
   * @return true if RateLimitOverride was set in the descriptor.
   */
  virtual bool populateOverride(RateLimit::Descriptor& descriptor,
                                const envoy::config::core::v3::Metadata* metadata) const PURE;
};

using RateLimitOverrideActionPtr = std::unique_ptr<RateLimitOverrideAction>;

/**
 * Rate limit configuration.
 */
class RateLimitPolicyEntry {
public:
  virtual ~RateLimitPolicyEntry() = default;

  /**
   * @return the stage value that the configuration is applicable to.
   */
  virtual uint64_t stage() const PURE;

  /**
   * @return runtime key to be set to disable the configuration.
   */
  virtual const std::string& disableKey() const PURE;

  /**
   * @return true if this rate limit policy should be applied on stream done.
   */
  virtual bool applyOnStreamDone() const PURE;

  /**
   * Potentially populate the descriptor array with new descriptors to query.
   * @param descriptors supplies the descriptor array to optionally fill.
   * @param local_service_cluster supplies the name of the local service cluster.
   * @param headers supplies the header for the request.
   * @param info stream info associated with the request
   */
  virtual void populateDescriptors(std::vector<RateLimit::Descriptor>& descriptors,
                                   const std::string& local_service_cluster,
                                   const Http::RequestHeaderMap& headers,
                                   const StreamInfo::StreamInfo& info) const PURE;

  /**
   * Potentially populate the local descriptor array with new descriptors to query.
   * @param descriptors supplies the descriptor array to optionally fill.
   * @param local_service_cluster supplies the name of the local service cluster.
   * @param headers supplies the header for the request.
   * @param info stream info associated with the request
   */
  virtual void populateLocalDescriptors(std::vector<RateLimit::LocalDescriptor>& descriptors,
                                        const std::string& local_service_cluster,
                                        const Http::RequestHeaderMap& headers,
                                        const StreamInfo::StreamInfo& info) const PURE;
};

/**
 * Optional interface for rate limit policy entries whose descriptor producers need mutable stream
 * info. Existing RateLimitPolicyEntry implementations remain source compatible.
 */
class RateLimitPolicyEntryWithMutableStreamInfo {
public:
  virtual ~RateLimitPolicyEntryWithMutableStreamInfo() = default;

  virtual void populateDescriptorsWithMutableStreamInfo(
      std::vector<RateLimit::Descriptor>& descriptors, const std::string& local_service_cluster,
      const Http::RequestHeaderMap& headers, StreamInfo::StreamInfo& info) const PURE;

  virtual void populateLocalDescriptorsWithMutableStreamInfo(
      std::vector<RateLimit::LocalDescriptor>& descriptors,
      const std::string& local_service_cluster, const Http::RequestHeaderMap& headers,
      StreamInfo::StreamInfo& info) const PURE;
};

/**
 * Populates descriptors through the mutable interface when supported, falling back to the
 * source-compatible const interface otherwise.
 */
inline void populateDescriptorsWithMutableStreamInfo(
    const RateLimitPolicyEntry& entry, std::vector<RateLimit::Descriptor>& descriptors,
    const std::string& local_service_cluster, const Http::RequestHeaderMap& headers,
    StreamInfo::StreamInfo& info) {
  const auto* mutable_entry =
      dynamic_cast<const RateLimitPolicyEntryWithMutableStreamInfo*>(&entry);
  if (mutable_entry != nullptr) {
    mutable_entry->populateDescriptorsWithMutableStreamInfo(descriptors, local_service_cluster,
                                                            headers, info);
  } else {
    entry.populateDescriptors(descriptors, local_service_cluster, headers, info);
  }
}

inline void populateLocalDescriptorsWithMutableStreamInfo(
    const RateLimitPolicyEntry& entry, std::vector<RateLimit::LocalDescriptor>& descriptors,
    const std::string& local_service_cluster, const Http::RequestHeaderMap& headers,
    StreamInfo::StreamInfo& info) {
  const auto* mutable_entry =
      dynamic_cast<const RateLimitPolicyEntryWithMutableStreamInfo*>(&entry);
  if (mutable_entry != nullptr) {
    mutable_entry->populateLocalDescriptorsWithMutableStreamInfo(descriptors, local_service_cluster,
                                                                 headers, info);
  } else {
    entry.populateLocalDescriptors(descriptors, local_service_cluster, headers, info);
  }
}

/**
 * Rate limiting policy.
 */
class RateLimitPolicy {
public:
  virtual ~RateLimitPolicy() = default;

  /**
   * @return true if there is no rate limit policy for all stage settings.
   */
  virtual bool empty() const PURE;

  /**
   * @param stage the value for finding applicable rate limit configurations.
   * @return set of RateLimitPolicyEntry that are applicable for a stage.
   */
  virtual const std::vector<std::reference_wrapper<const RateLimitPolicyEntry>>&
  getApplicableRateLimit(uint64_t stage) const PURE;
};

} // namespace Router
} // namespace Envoy
