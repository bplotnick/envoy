The Rust SDK's ``EnvoyBuffer`` now implements ``Deref<Target = [u8]>``, allowing slice methods,
indexing, and coercion to ``&[u8]`` without calling ``as_slice()``.
