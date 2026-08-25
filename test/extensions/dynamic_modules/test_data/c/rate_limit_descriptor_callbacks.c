#include <stdlib.h>
#include <string.h>

#include "source/extensions/dynamic_modules/abi/abi.h"

typedef struct {
  char* name;
  size_t name_length;
  char* config;
  size_t config_length;
} rate_limit_descriptor_config;

envoy_dynamic_module_type_abi_version_module_ptr envoy_dynamic_module_on_program_init(void) {
  return envoy_dynamic_modules_abi_version;
}

envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr
envoy_dynamic_module_on_rate_limit_descriptor_config_new(
    envoy_dynamic_module_type_rate_limit_descriptor_config_envoy_ptr config_envoy_ptr,
    envoy_dynamic_module_type_envoy_buffer descriptor_name,
    envoy_dynamic_module_type_envoy_buffer descriptor_config) {
  rate_limit_descriptor_config* config = malloc(sizeof(rate_limit_descriptor_config));
  config->name = malloc(descriptor_name.length);
  memcpy(config->name, descriptor_name.ptr, descriptor_name.length);
  config->name_length = descriptor_name.length;
  config->config = malloc(descriptor_config.length);
  memcpy(config->config, descriptor_config.ptr, descriptor_config.length);
  config->config_length = descriptor_config.length;
  return config;
}

void envoy_dynamic_module_on_rate_limit_descriptor_config_destroy(
    envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr config_module_ptr) {
  rate_limit_descriptor_config* config = (rate_limit_descriptor_config*)config_module_ptr;
  free(config->name);
  free(config->config);
  free(config);
}

static bool
get_header(envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr,
           const char* key, size_t key_length, envoy_dynamic_module_type_envoy_buffer* result) {
  envoy_dynamic_module_type_module_buffer key_buffer = {key, key_length};
  return envoy_dynamic_module_callback_rate_limit_descriptor_get_request_header_value(
      context_envoy_ptr, key_buffer, result, 0, NULL);
}

bool envoy_dynamic_module_on_rate_limit_descriptor_populate(
    envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr config_module_ptr,
    envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr context_envoy_ptr) {
  envoy_dynamic_module_type_envoy_buffer unused;
  if (get_header(context_envoy_ptr, "x-abort-descriptor", 18, &unused)) {
    return false;
  }
  if (get_header(context_envoy_ptr, "x-skip-descriptor", 17, &unused)) {
    return true;
  }

  envoy_dynamic_module_type_envoy_buffer value;
  if (!get_header(context_envoy_ptr, "x-rate-limit-value", 18, &value) &&
      !envoy_dynamic_module_callback_rate_limit_descriptor_get_local_service_cluster(
          context_envoy_ptr, &value)) {
    rate_limit_descriptor_config* config = (rate_limit_descriptor_config*)config_module_ptr;
    value.ptr = config->config;
    value.length = config->config_length;
  }

  rate_limit_descriptor_config* config = (rate_limit_descriptor_config*)config_module_ptr;
  envoy_dynamic_module_type_module_buffer key = {config->name, config->name_length};
  envoy_dynamic_module_type_module_buffer module_value = {value.ptr, value.length};
  envoy_dynamic_module_callback_rate_limit_descriptor_set_descriptor_entry(context_envoy_ptr, key,
                                                                           module_value);
  return true;
}
