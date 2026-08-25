load("//bazel:envoy_build_system.bzl", "envoy_cc_test", "envoy_package")

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    data = [
        "//test/extensions/dynamic_modules/test_data/c:rate_limit_descriptor_config_new_fail",
        "//test/extensions/dynamic_modules/test_data/c:rate_limit_descriptor_missing_config_destroy",
        "//test/extensions/dynamic_modules/test_data/c:rate_limit_descriptor_missing_config_new",
        "//test/extensions/dynamic_modules/test_data/c:rate_limit_descriptor_missing_populate",
        "//test/extensions/dynamic_modules/test_data/c:rate_limit_descriptor_no_op",
    ],
    deps = [
        "//envoy/registry",
        "//source/extensions/rate_limit_descriptors/dynamic_modules:config",
        "//test/extensions/dynamic_modules:util",
        "//test/mocks/server:server_factory_context_mocks",
        "//test/test_common:status_utility_lib",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/extensions/rate_limit_descriptors/dynamic_modules/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "descriptor_test",
    srcs = ["descriptor_test.cc"],
    data = [
        "//test/extensions/dynamic_modules/test_data/c:rate_limit_descriptor_callbacks",
        "//test/extensions/dynamic_modules/test_data/c:rate_limit_descriptor_no_op",
        "//test/extensions/dynamic_modules/test_data/rust:rate_limit_descriptor_integration_test",
    ],
    deps = [
        "//source/common/protobuf:cc_wkt_protos",
        "//source/extensions/filters/common/ratelimit_config:ratelimit_config_lib",
        "//source/extensions/rate_limit_descriptors/dynamic_modules:config",
        "//source/extensions/rate_limit_descriptors/dynamic_modules:descriptor_lib",
        "//test/extensions/dynamic_modules:util",
        "//test/mocks/server:server_factory_context_mocks",
        "//test/mocks/stream_info:stream_info_mocks",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/config/route/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "abi_impl_test",
    srcs = ["abi_impl_test.cc"],
    deps = [
        "//source/extensions/rate_limit_descriptors/dynamic_modules:descriptor_lib",
        "//test/mocks/stream_info:stream_info_mocks",
        "//test/test_common:utility_lib",
    ],
)
