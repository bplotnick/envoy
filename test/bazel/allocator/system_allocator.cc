// Compile-only check of the allocator flags supplied to Envoy targets.
// NOLINT(namespace-envoy)

#ifndef ABSL_MALLOC_HOOK_MMAP_DISABLE
#error "System allocator selection must disable malloc mmap hooks"
#endif

#if defined(TCMALLOC) || defined(GPERFTOOLS_TCMALLOC) || defined(JEMALLOC) ||                      \
    defined(ENVOY_MEMORY_DEBUG_ENABLED)
#error "System allocator selection must not enable a bundled allocator"
#endif
