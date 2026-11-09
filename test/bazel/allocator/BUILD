load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("//bazel:envoy_build_system.bzl", "envoy_cc_library", "envoy_package")
load(":allocator_test.bzl", "allocator_test_suite", "with_system_allocator")

licenses(["notice"])  # Apache 2

envoy_package()

allocator_test_suite(name = "allocator_tests")

envoy_cc_library(
    name = "system_allocator_lib",
    srcs = ["system_allocator.cc"],
    tags = ["manual"],
)

with_system_allocator(
    name = "transitioned_system_allocator_lib",
    dep = ":system_allocator_lib",
    tags = ["manual"],
)

build_test(
    name = "system_allocator_build_test",
    targets = [":transitioned_system_allocator_lib"],
)
