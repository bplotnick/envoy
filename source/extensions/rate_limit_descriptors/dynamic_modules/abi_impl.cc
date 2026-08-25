#include "source/extensions/dynamic_modules/abi/abi.h"
#include "source/extensions/dynamic_modules/abi_context_accessors.h"
#include "source/extensions/rate_limit_descriptors/dynamic_modules/descriptor.h"

namespace Envoy {
namespace Extensions {
namespace RateLimitDescriptors {
namespace DynamicModules {

using Envoy::Extensions::DynamicModules::ContextAccessor;
using Envoy::Extensions::DynamicModules::HeadersMapOptConstRef;

namespace {

RateLimitDescriptorContext*
rateLimitDescriptorContext(envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr ptr) {
  return static_cast<RateLimitDescriptorContext*>(ptr);
}

} // namespace

extern "C" {

bool envoy_dynamic_module_callback_rate_limit_descriptor_get_local_service_cluster(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_envoy_buffer* result) {
  const std::string& cluster = rateLimitDescriptorContext(context_envoy_ptr)->local_service_cluster;
  if (cluster.empty()) {
    return false;
  }
  *result = {cluster.data(), cluster.size()};
  return true;
}

size_t envoy_dynamic_module_callback_rate_limit_descriptor_get_request_headers_size(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr) {
  return rateLimitDescriptorContext(context_envoy_ptr)->headers.size();
}

bool envoy_dynamic_module_callback_rate_limit_descriptor_get_request_headers(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_envoy_http_header* result_headers) {
  auto* context = rateLimitDescriptorContext(context_envoy_ptr);
  return ContextAccessor::getHeaders(HeadersMapOptConstRef(context->headers), result_headers);
}

bool envoy_dynamic_module_callback_rate_limit_descriptor_get_request_header_value(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_module_buffer key, envoy_dynamic_module_type_envoy_buffer* result,
    size_t index, size_t* total_count_out) {
  auto* context = rateLimitDescriptorContext(context_envoy_ptr);
  return ContextAccessor::getHeaderValue(HeadersMapOptConstRef(context->headers), key, result,
                                         index, total_count_out);
}

bool envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_string(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_attribute_id attribute_id,
    envoy_dynamic_module_type_envoy_buffer* result) {
  auto* context = rateLimitDescriptorContext(context_envoy_ptr);
  return ContextAccessor::getAttributeString(context->stream_info, attribute_id, result);
}

bool envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_int(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_attribute_id attribute_id, uint64_t* result) {
  auto* context = rateLimitDescriptorContext(context_envoy_ptr);
  return ContextAccessor::getAttributeInt(context->stream_info, attribute_id, result);
}

bool envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_bool(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_attribute_id attribute_id, bool* result) {
  auto* context = rateLimitDescriptorContext(context_envoy_ptr);
  return ContextAccessor::getAttributeBool(context->stream_info, attribute_id, result);
}

bool envoy_dynamic_module_callback_rate_limit_descriptor_get_dynamic_metadata(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_module_buffer filter_name,
    envoy_dynamic_module_type_module_buffer path, envoy_dynamic_module_type_envoy_buffer* result) {
  auto* context = rateLimitDescriptorContext(context_envoy_ptr);
  return ContextAccessor::getDynamicMetadata(context->stream_info, filter_name, path, result);
}

void envoy_dynamic_module_callback_rate_limit_descriptor_set_descriptor_entry(
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
    envoy_dynamic_module_type_module_buffer key, envoy_dynamic_module_type_module_buffer value) {
  auto* context = rateLimitDescriptorContext(context_envoy_ptr);
  context->descriptor_entry.key_.assign(key.ptr, key.length);
  context->descriptor_entry.value_.assign(value.ptr, value.length);
}

} // extern "C"

} // namespace DynamicModules
} // namespace RateLimitDescriptors
} // namespace Extensions
} // namespace Envoy
