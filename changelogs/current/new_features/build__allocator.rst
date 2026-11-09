build: added the ``//bazel:allocator`` string build setting with values ``from_flags`` (the default),
``tcmalloc``, ``gperftools``, ``debug``, ``jemalloc`` and ``system`` for per-target allocator selection
through Starlark transitions. The default preserves support for ``--define tcmalloc=...`` and
``--//bazel:jemalloc``. Explicit allocator values override those legacy options. Envoy's build configs
now use the string setting; use ``--//bazel:allocator=from_flags`` to restore legacy flag selection
when overriding one of those configs.
