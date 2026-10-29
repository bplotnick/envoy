The Rust SDK's ``EnvoyBuffer`` now implements ``Deref<Target = [u8]>``, allowing slice methods,
indexing, and coercion to ``&[u8]`` without calling ``as_slice()``.
``EnvoyMutBuffer`` also implements ``Deref<Target = [u8]>`` and ``DerefMut``, allowing mutable
slice methods, indexing, and coercion to ``&mut [u8]`` without calling ``as_mut_slice()``.
