Added an ``envoy.rate_limit_descriptors.dynamic_modules`` extension that lets a dynamic module
produce entries for local and global HTTP rate limit descriptors from request headers, stream info
attributes, dynamic metadata, and the local service cluster. See
:ref:`DynamicModuleRateLimitDescriptor <envoy_v3_api_msg_extensions.rate_limit_descriptors.dynamic_modules.v3.DynamicModuleRateLimitDescriptor>`
for configuration details. The Rust SDK exposes the interface through its
``rate_limit_descriptor`` module.
