load(
    "//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_extension_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    extension_names = ["envoy.filters.http.credential_injector"],
    rbe_pool = "6gig",
    deps = [
        "//source/extensions/http/injected_credentials/oauth2:config",
        "//test/mocks/server:factory_context_mocks",
    ],
)

envoy_extension_cc_test(
    name = "token_provider_test",
    srcs = ["token_provider_test.cc"],
    extension_names = ["envoy.filters.http.credential_injector"],
    rbe_pool = "6gig",
    deps = [
        "//source/extensions/http/injected_credentials/common:secret_reader_lib",
        "//source/extensions/http/injected_credentials/oauth2:token_provider_lib",
        "//test/mocks/event:event_mocks",
        "//test/mocks/server:factory_context_mocks",
        "//test/mocks/server:server_factory_context_mocks",
        "//test/mocks/server:server_mocks",
        "//test/mocks/thread_local:thread_local_mocks",
    ],
)

envoy_extension_cc_test(
    name = "credential_injector_oauth_integration_test",
    srcs = ["credential_injector_oauth_integration_test.cc"],
    extension_names = ["envoy.filters.http.credential_injector"],
    rbe_pool = "6gig",
    deps = [
        "//source/extensions/filters/http/credential_injector:config",
        "//source/extensions/http/injected_credentials/oauth2:config",
        "//source/extensions/http/injected_credentials/oauth2:oauth_response_cc_proto",
        "//source/common/common:fmt_lib",
        "//source/common/router:upstream_codec_filter_lib",
        "//test/integration:http_protocol_integration_lib",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/extensions/filters/http/upstream_codec/v3:pkg_cc_proto",
        "@envoy_api//envoy/extensions/filters/network/http_connection_manager/v3:pkg_cc_proto",
    ],
)
