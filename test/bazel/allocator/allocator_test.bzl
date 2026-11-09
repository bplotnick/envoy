"""Analysis and build tests for allocator selection and transitions."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("//bazel:envoy_internal.bzl", "envoy_copts", "tcmalloc_external_dep", "tcmalloc_external_deps")

_ALLOCATOR = str(Label("//bazel:allocator"))
_MARKERS = [
    "-DABSL_MALLOC_HOOK_MMAP_DISABLE",
    "-DENVOY_MEMORY_DEBUG_ENABLED=1",
    "-DGPERFTOOLS_TCMALLOC",
    "-DJEMALLOC",
    "-DTCMALLOC",
]

_AllocatorInfo = provider(fields = ["allocator", "copts", "dep", "deps", "output", "variables"])

def _allocator_probe_impl(ctx):
    output = ctx.actions.declare_file(ctx.label.name + ".txt")
    ctx.actions.write(output, ctx.attr.allocator[BuildSettingInfo].value)
    return [
        DefaultInfo(files = depset([output])),
        _AllocatorInfo(
            allocator = ctx.attr.allocator[BuildSettingInfo].value,
            copts = [opt for opt in ctx.attr.copts if opt in _MARKERS],
            dep = ctx.attr.allocator_dep.label if ctx.attr.allocator_dep else None,
            deps = [dep.label for dep in ctx.attr.allocator_deps],
            output = output.path,
            variables = {key: value for key, value in ctx.var.items() if key not in ["BINDIR", "GENDIR"]},
        ),
    ]

_allocator_probe = rule(
    implementation = _allocator_probe_impl,
    attrs = {
        "allocator": attr.label(default = Label("//bazel:allocator")),
        "allocator_dep": attr.label(),
        "allocator_deps": attr.label_list(),
        "copts": attr.string_list(),
        "guard": attr.label(default = Label("//bazel:check_removed_allocator_flags")),
    },
)

def _system_allocator_transition_impl(settings, attr):
    return {_ALLOCATOR: "system"}

_system_allocator_transition = transition(
    implementation = _system_allocator_transition_impl,
    inputs = [],
    outputs = [_ALLOCATOR],
)

def _with_system_allocator_impl(ctx):
    return [ctx.attr.dep[0][DefaultInfo]]

with_system_allocator = rule(
    implementation = _with_system_allocator_impl,
    attrs = {
        "dep": attr.label(cfg = _system_allocator_transition),
        "_allowlist_function_transition": attr.label(default = "@bazel_tools//tools/allowlists/function_transition_allowlist"),
    },
)

def _assert_allocator(env, info, allocator, google_tcmalloc):
    expected = {
        "tcmalloc": (["-DTCMALLOC"] if google_tcmalloc else ["-DGPERFTOOLS_TCMALLOC"], Label("//bazel:tcmalloc_lib"), [Label("//bazel:tcmalloc_all_libs")]),
        "gperftools": (["-DGPERFTOOLS_TCMALLOC"], Label("//bazel/external:gperftools"), [Label("//bazel/external:gperftools")]),
        "debug": (["-DENVOY_MEMORY_DEBUG_ENABLED=1", "-DGPERFTOOLS_TCMALLOC"], Label("//bazel/external:gperftools"), [Label("//bazel/external:gperftools")]),
        "jemalloc": (["-DJEMALLOC"], Label("@jemalloc//:jemalloc_with_defaults"), [Label("@jemalloc//:jemalloc_with_defaults")]),
        "system": (["-DABSL_MALLOC_HOOK_MMAP_DISABLE"], None, []),
    }[allocator]
    asserts.equals(env, allocator, info.allocator)
    asserts.equals(env, expected[0], info.copts)
    asserts.equals(env, expected[1], info.dep)
    asserts.equals(env, expected[2], info.deps)

def _allocator_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    google_tcmalloc = (
        ctx.target_platform_has_constraint(ctx.attr._x86_64[platform_common.ConstraintValueInfo]) or
        ctx.target_platform_has_constraint(ctx.attr._aarch64[platform_common.ConstraintValueInfo])
    )
    _assert_allocator(env, target[_AllocatorInfo], ctx.attr.expected_allocator, google_tcmalloc)
    return analysistest.end(env)

def _make_allocator_test(implementation, allocator):
    return analysistest.make(
        implementation,
        config_settings = {_ALLOCATOR: allocator},
        attrs = {
            "expected_allocator": attr.string(default = allocator),
            "_x86_64": attr.label(default = "@platforms//cpu:x86_64"),
            "_aarch64": attr.label(default = "@platforms//cpu:aarch64"),
        },
    )

_tcmalloc_selection_test = _make_allocator_test(_allocator_test_impl, "tcmalloc")
_gperftools_selection_test = _make_allocator_test(_allocator_test_impl, "gperftools")
_debug_selection_test = _make_allocator_test(_allocator_test_impl, "debug")
_jemalloc_selection_test = _make_allocator_test(_allocator_test_impl, "jemalloc")
_system_selection_test = _make_allocator_test(_allocator_test_impl, "system")

def _transition_pair_impl(ctx):
    return [_TransitionInfo(plain = ctx.attr.plain[_AllocatorInfo], system = ctx.attr.system[0][_AllocatorInfo])]

_TransitionInfo = provider(fields = ["plain", "system"])

_transition_pair = rule(
    implementation = _transition_pair_impl,
    attrs = {
        "plain": attr.label(),
        "system": attr.label(cfg = _system_allocator_transition),
        "_allowlist_function_transition": attr.label(default = "@bazel_tools//tools/allowlists/function_transition_allowlist"),
    },
)

def _transition_test_impl(ctx):
    env = analysistest.begin(ctx)
    pair = analysistest.target_under_test(env)[_TransitionInfo]
    asserts.equals(env, ctx.attr.expected_allocator, pair.plain.allocator)
    _assert_allocator(env, pair.system, "system", False)
    asserts.equals(env, pair.plain.variables, pair.system.variables)
    if ctx.attr.expected_allocator == "system":
        asserts.equals(env, pair.plain.output, pair.system.output, "Selecting system again must reuse the same configuration")
    else:
        asserts.true(env, pair.plain.output != pair.system.output)
    return analysistest.end(env)

_tcmalloc_transition_test = _make_allocator_test(_transition_test_impl, "tcmalloc")
_gperftools_transition_test = _make_allocator_test(_transition_test_impl, "gperftools")
_debug_transition_test = _make_allocator_test(_transition_test_impl, "debug")
_jemalloc_transition_test = _make_allocator_test(_transition_test_impl, "jemalloc")
_system_transition_test = _make_allocator_test(_transition_test_impl, "system")

def _removed_jemalloc_test_impl(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(env, "Use --@envoy//bazel:allocator=jemalloc instead.")
    return analysistest.end(env)

_removed_jemalloc_test = analysistest.make(
    _removed_jemalloc_test_impl,
    config_settings = {str(Label("//bazel:jemalloc")): True},
    expect_failure = True,
)

def _invalid_allocator_test_impl(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(env, "invalid value 'invalid'")
    return analysistest.end(env)

_invalid_allocator_test = analysistest.make(
    _invalid_allocator_test_impl,
    config_settings = {_ALLOCATOR: "invalid"},
    expect_failure = True,
)

def allocator_test_suite(name):
    """Exercise all choices, inherited settings, and transitions without compiling their closures."""
    _allocator_probe(
        name = "allocator_probe",
        allocator_dep = tcmalloc_external_dep(),
        allocator_deps = tcmalloc_external_deps(),
        copts = envoy_copts(),
        tags = ["manual"],
    )
    _transition_pair(
        name = "transition_pair",
        plain = ":allocator_probe",
        system = ":allocator_probe",
        tags = ["manual"],
    )
    tests = []
    for allocator, test in {
        "tcmalloc": _tcmalloc_selection_test,
        "gperftools": _gperftools_selection_test,
        "debug": _debug_selection_test,
        "jemalloc": _jemalloc_selection_test,
        "system": _system_selection_test,
    }.items():
        test_name = allocator + "_selection_test"
        test(name = test_name, target_under_test = ":allocator_probe")
        tests.append(test_name)
    for allocator, test in {
        "tcmalloc": _tcmalloc_transition_test,
        "gperftools": _gperftools_transition_test,
        "debug": _debug_transition_test,
        "jemalloc": _jemalloc_transition_test,
        "system": _system_transition_test,
    }.items():
        test_name = allocator + "_transition_test"
        test(name = test_name, target_under_test = ":transition_pair")
        tests.append(test_name)
    _removed_jemalloc_test(name = "removed_jemalloc_test", target_under_test = ":allocator_probe")
    _invalid_allocator_test(name = "invalid_allocator_test", target_under_test = "//bazel:allocator")
    native.test_suite(name = name, tests = tests + ["removed_jemalloc_test", "invalid_allocator_test"])
