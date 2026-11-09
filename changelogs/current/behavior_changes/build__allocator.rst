build: allocator selection now uses the ``//bazel:allocator`` string build setting with values
``tcmalloc`` (the default), ``gperftools``, ``debug``, ``jemalloc`` and ``system``. This allows
per-target allocator selection through Starlark transitions. The existing platform-dependent default
is unchanged. Replace ``--define tcmalloc=disabled`` with ``--//bazel:allocator=system``,
``--define tcmalloc=gperftools`` or ``--define tcmalloc=debug`` with the corresponding allocator value,
and ``--//bazel:jemalloc`` with ``--//bazel:allocator=jemalloc``. Removed allocator options fail with
migration guidance.
