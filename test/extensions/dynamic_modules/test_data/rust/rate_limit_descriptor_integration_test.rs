//! Integration test module for the dynamic module rate limit descriptor producer.

use envoy_proxy_dynamic_modules_rust_sdk::rate_limit_descriptor::*;
use envoy_proxy_dynamic_modules_rust_sdk::*;

declare_all_init_functions!(init, rate_limit_descriptor: new_descriptor);

fn init() -> bool {
  true
}

fn new_descriptor(name: &str, config: &[u8]) -> Option<Box<dyn RateLimitDescriptorConfig>> {
  (name == "rust-key").then(|| {
    Box::new(TestDescriptor {
      fallback: String::from_utf8_lossy(config).into_owned(),
    }) as Box<dyn RateLimitDescriptorConfig>
  })
}

struct TestDescriptor {
  fallback: String,
}

struct CachedValue(String);

const CACHE_KEY: &[u8] = b"envoy.dynamic_modules.rate_limit_descriptor.test";

extern "C" fn destroy_cached_value(value: *mut std::ffi::c_void) {
  unsafe {
    drop(Box::from_raw(value as *mut CachedValue));
  }
}

impl RateLimitDescriptorConfig for TestDescriptor {
  fn populate(&self, ctx: &mut RateLimitDescriptorContext) -> bool {
    let value = match ctx.get_filter_state_object(CACHE_KEY) {
      Some(cached) => unsafe { (&*(cached as *const CachedValue)).0.clone() },
      None => {
        let value = ctx
          .get_request_header("x-rate-limit-value")
          .or_else(|| ctx.get_local_service_cluster())
          .map(|value| String::from_utf8_lossy(value.as_slice()).into_owned())
          .unwrap_or_else(|| self.fallback.clone());
        let cached = Box::into_raw(Box::new(CachedValue(value.clone())));
        if !unsafe {
          ctx.set_filter_state_object(
            CACHE_KEY,
            cached.cast::<std::ffi::c_void>(),
            destroy_cached_value,
          )
        } {
          return false;
        }
        value
      },
    };
    ctx.set_descriptor_entry("rust-key", &value);
    true
  }
}
