"""Build a target with the system allocator regardless of inherited allocator flags."""

_ALLOCATOR = str(Label("//bazel:allocator"))

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
