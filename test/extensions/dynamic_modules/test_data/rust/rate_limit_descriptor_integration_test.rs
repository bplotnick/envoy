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

impl RateLimitDescriptorConfig for TestDescriptor {
  fn populate(&self, ctx: &mut RateLimitDescriptorContext) -> bool {
    let value = ctx
      .get_request_header("x-rate-limit-value")
      .or_else(|| ctx.get_local_service_cluster())
      .map(|value| String::from_utf8_lossy(value.as_slice()).into_owned())
      .unwrap_or_else(|| self.fallback.clone());
    ctx.set_descriptor_entry("rust-key", &value);
    true
  }
}
