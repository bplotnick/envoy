load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_library(
    name = "descriptor_lib",
    srcs = [
        "abi_impl.cc",
        "descriptor.cc",
    ],
    hdrs = ["descriptor.h"],
    deps = [
        "//envoy/ratelimit:ratelimit_interface",
        "//source/common/common:assert_lib",
        "//source/common/protobuf:utility_lib",
        "//source/extensions/dynamic_modules:abi_context_accessors_lib",
        "//source/extensions/dynamic_modules:abi_impl",
        "//source/extensions/dynamic_modules:dynamic_modules_lib",
        "@envoy_api//envoy/extensions/rate_limit_descriptors/dynamic_modules/v3:pkg_cc_proto",
    ],
)

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":descriptor_lib",
        "//envoy/ratelimit:ratelimit_interface",
        "//envoy/registry",
        "//source/common/protobuf:utility_lib",
        "//source/extensions/dynamic_modules:dynamic_modules_lib",
        "@envoy_api//envoy/extensions/rate_limit_descriptors/dynamic_modules/v3:pkg_cc_proto",
    ],
)
