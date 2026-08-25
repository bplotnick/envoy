#include "source/extensions/dynamic_modules/abi/abi.h"

envoy_dynamic_module_type_abi_version_module_ptr envoy_dynamic_module_on_program_init(void) {
  return envoy_dynamic_modules_abi_version;
}

envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr
envoy_dynamic_module_on_rate_limit_descriptor_config_new(
    envoy_dynamic_module_type_rate_limit_descriptor_config_envoy_ptr config_envoy_ptr,
    envoy_dynamic_module_type_envoy_buffer descriptor_name,
    envoy_dynamic_module_type_envoy_buffer descriptor_config) {
  return NULL;
}

void envoy_dynamic_module_on_rate_limit_descriptor_config_destroy(
    envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr config_module_ptr) {}

bool envoy_dynamic_module_on_rate_limit_descriptor_populate(
    envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr config_module_ptr,
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr) {
  return false;
}
