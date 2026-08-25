//! Rate limit descriptor producer support for dynamic modules.
//!
//! The entry point is the `rate_limit_descriptor:` arm of
//! [`crate::declare_all_init_functions!`], which registers a factory through
//! [`crate::NEW_RATE_LIMIT_DESCRIPTOR_CONFIG_FUNCTION`].

use crate::{abi, EnvoyBuffer};
use std::ffi::c_void;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;

/// Context for producing one rate limit descriptor entry for a request.
///
/// The context is valid only for the duration of a single
/// [`RateLimitDescriptorConfig::populate`] call and must not be stored.
pub struct RateLimitDescriptorContext {
  envoy_ptr: *mut c_void,
}

impl RateLimitDescriptorContext {
  /// Create a new context. Used internally by the SDK.
  ///
  /// # Safety
  ///
  /// `envoy_ptr` must be the context Envoy passed to
  /// [`envoy_dynamic_module_on_rate_limit_descriptor_populate`].
  #[doc(hidden)]
  pub unsafe fn new(envoy_ptr: *mut c_void) -> Self {
    Self { envoy_ptr }
  }

  /// Get the local service cluster configured in Envoy.
  pub fn get_local_service_cluster(&self) -> Option<EnvoyBuffer<'_>> {
    let mut result = abi::envoy_dynamic_module_type_envoy_buffer {
      ptr: ptr::null_mut(),
      length: 0,
    };
    if unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_local_service_cluster(
        self.envoy_ptr,
        &mut result,
      )
    } {
      Some(unsafe { EnvoyBuffer::new_from_raw(result.ptr as *const u8, result.length) })
    } else {
      None
    }
  }

  /// Get the number of request header entries.
  pub fn get_request_headers_count(&self) -> usize {
    unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_request_headers_size(
        self.envoy_ptr,
      )
    }
  }

  /// Get all request headers as key-value [`EnvoyBuffer`] pairs.
  pub fn get_all_request_headers(&self) -> Vec<(EnvoyBuffer<'_>, EnvoyBuffer<'_>)> {
    let count = self.get_request_headers_count();
    if count == 0 {
      return Vec::new();
    }
    let mut raw: Vec<abi::envoy_dynamic_module_type_envoy_http_header> = Vec::with_capacity(count);
    let success = unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_request_headers(
        self.envoy_ptr,
        raw.as_mut_ptr(),
      )
    };
    if !success {
      return Vec::new();
    }
    unsafe {
      raw.set_len(count);
    }
    raw
      .iter()
      .map(|header| {
        (
          unsafe { EnvoyBuffer::new_from_raw(header.key_ptr as *const _, header.key_length) },
          unsafe { EnvoyBuffer::new_from_raw(header.value_ptr as *const _, header.value_length) },
        )
      })
      .collect()
  }

  /// Get the first value of the request header with the given key.
  pub fn get_request_header(&self, key: &str) -> Option<EnvoyBuffer<'_>> {
    self
      .get_request_header_value(key, 0)
      .map(|(value, _)| value)
  }

  /// Get an indexed request header value and the total number of values for the key.
  pub fn get_request_header_value(
    &self,
    key: &str,
    index: usize,
  ) -> Option<(EnvoyBuffer<'_>, usize)> {
    let key_buf = crate::str_to_module_buffer(key);
    let mut result = abi::envoy_dynamic_module_type_envoy_buffer {
      ptr: ptr::null_mut(),
      length: 0,
    };
    let mut total_count = 0;
    if unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_request_header_value(
        self.envoy_ptr,
        key_buf,
        &mut result,
        index,
        &mut total_count,
      )
    } {
      Some((
        unsafe { EnvoyBuffer::new_from_raw(result.ptr as *const u8, result.length) },
        total_count,
      ))
    } else {
      None
    }
  }

  /// Get a string stream info attribute.
  pub fn get_attribute_string(
    &self,
    attribute_id: abi::envoy_dynamic_module_type_attribute_id,
  ) -> Option<EnvoyBuffer<'_>> {
    let mut result = abi::envoy_dynamic_module_type_envoy_buffer {
      ptr: ptr::null_mut(),
      length: 0,
    };
    if unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_string(
        self.envoy_ptr,
        attribute_id,
        &mut result,
      )
    } {
      Some(unsafe { EnvoyBuffer::new_from_raw(result.ptr as *const u8, result.length) })
    } else {
      None
    }
  }

  /// Get an integer stream info attribute.
  pub fn get_attribute_int(
    &self,
    attribute_id: abi::envoy_dynamic_module_type_attribute_id,
  ) -> Option<u64> {
    let mut result = 0;
    if unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_int(
        self.envoy_ptr,
        attribute_id,
        &mut result,
      )
    } {
      Some(result)
    } else {
      None
    }
  }

  /// Get a boolean stream info attribute.
  pub fn get_attribute_bool(
    &self,
    attribute_id: abi::envoy_dynamic_module_type_attribute_id,
  ) -> Option<bool> {
    let mut result = false;
    if unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_attribute_bool(
        self.envoy_ptr,
        attribute_id,
        &mut result,
      )
    } {
      Some(result)
    } else {
      None
    }
  }

  /// Get a string value from stream dynamic metadata.
  pub fn get_dynamic_metadata(&self, filter_name: &str, path: &str) -> Option<EnvoyBuffer<'_>> {
    let mut result = abi::envoy_dynamic_module_type_envoy_buffer {
      ptr: ptr::null_mut(),
      length: 0,
    };
    if unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_dynamic_metadata(
        self.envoy_ptr,
        crate::str_to_module_buffer(filter_name),
        crate::str_to_module_buffer(path),
        &mut result,
      )
    } {
      Some(unsafe { EnvoyBuffer::new_from_raw(result.ptr as *const u8, result.length) })
    } else {
      None
    }
  }

  /// Store an opaque, module-owned object in request-lifetime filter state under `key`.
  ///
  /// This can cache a parsed or computed value for later descriptor producers in the same request.
  /// Ownership transfers to Envoy on every path: `destructor` runs exactly once, either when the
  /// request state is destroyed or before this returns `false`.
  ///
  /// # Safety
  ///
  /// `object` must be valid for `destructor` to free. The destructor must not unwind because it
  /// runs on the request teardown path outside an FFI unwind guard.
  pub unsafe fn set_filter_state_object(
    &mut self,
    key: &[u8],
    object: *mut c_void,
    destructor: extern "C" fn(*mut c_void),
  ) -> bool {
    let destructor: unsafe extern "C" fn(*mut c_void) = destructor;
    unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_set_filter_state_object(
        self.envoy_ptr,
        crate::bytes_to_module_buffer(key),
        object,
        Some(destructor),
      )
    }
  }

  /// Borrow an opaque object stored through the descriptor ABI in request filter state under `key`.
  ///
  /// Ownership remains with Envoy. The pointer is valid until the entry is destroyed or
  /// overwritten. Modules must namespace keys and use the same concrete object type for every use
  /// of a shared key.
  pub fn get_filter_state_object(&self, key: &[u8]) -> Option<*mut c_void> {
    let object = unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_get_filter_state_object(
        self.envoy_ptr,
        crate::bytes_to_module_buffer(key),
      )
    };
    if object.is_null() {
      None
    } else {
      Some(object)
    }
  }

  /// Set the descriptor entry produced for this request.
  ///
  /// Envoy copies both strings before this function returns. A later call replaces the entry.
  pub fn set_descriptor_entry(&mut self, key: &str, value: &str) {
    unsafe {
      abi::envoy_dynamic_module_callback_rate_limit_descriptor_set_descriptor_entry(
        self.envoy_ptr,
        crate::str_to_module_buffer(key),
        crate::str_to_module_buffer(value),
      )
    }
  }
}

/// Trait implemented by a dynamic module rate limit descriptor producer.
///
/// The configuration is shared across worker threads and must be thread-safe.
pub trait RateLimitDescriptorConfig: Send + Sync {
  /// Populate one descriptor entry for the current request.
  ///
  /// Return `true` to continue building the descriptor. Returning `true` without setting an entry
  /// skips this producer. Returning `false` aborts descriptor generation for this policy entry.
  fn populate(&self, ctx: &mut RateLimitDescriptorContext) -> bool;
}

/// # Safety
///
/// This is an FFI function called by Envoy. All pointer arguments must satisfy the dynamic module
/// ABI contract.
#[no_mangle]
pub unsafe extern "C" fn envoy_dynamic_module_on_rate_limit_descriptor_config_new(
  _config_envoy_ptr: abi::envoy_dynamic_module_type_rate_limit_descriptor_config_envoy_ptr,
  name: abi::envoy_dynamic_module_type_envoy_buffer,
  config: abi::envoy_dynamic_module_type_envoy_buffer,
) -> *const c_void {
  catch_unwind(AssertUnwindSafe(|| {
    let name =
      unsafe { crate::ffi_helpers::str_lossy_from_raw(name.ptr as *const u8, name.length) };
    let config = unsafe {
      crate::ffi_helpers::slice_from_raw_or_empty(config.ptr as *const u8, config.length)
    };
    envoy_dynamic_module_on_rate_limit_descriptor_config_new_impl(
      name.as_ref(),
      config,
      crate::NEW_RATE_LIMIT_DESCRIPTOR_CONFIG_FUNCTION
        .get()
        .expect("NEW_RATE_LIMIT_DESCRIPTOR_CONFIG_FUNCTION must be set"),
    )
  }))
  .unwrap_or_else(|panic| {
    crate::log_ffi_panic(
      "envoy_dynamic_module_on_rate_limit_descriptor_config_new",
      panic,
    );
    ptr::null()
  })
}

/// Testable implementation of the descriptor config creation hook.
pub fn envoy_dynamic_module_on_rate_limit_descriptor_config_new_impl(
  name: &str,
  config: &[u8],
  new_fn: &crate::NewRateLimitDescriptorConfigFunction,
) -> *const c_void {
  match new_fn(name, config) {
    Some(config) => crate::wrap_into_c_void_ptr!(config),
    None => ptr::null(),
  }
}

/// # Safety
///
/// This is an FFI function called by Envoy. `config_ptr` must be a pointer returned by the config
/// creation hook.
#[no_mangle]
pub unsafe extern "C" fn envoy_dynamic_module_on_rate_limit_descriptor_config_destroy(
  config_ptr: *const c_void,
) {
  let _ = catch_unwind(AssertUnwindSafe(|| {
    crate::drop_wrapped_c_void_ptr!(config_ptr, RateLimitDescriptorConfig);
  }))
  .map_err(|panic| {
    crate::log_ffi_panic(
      "envoy_dynamic_module_on_rate_limit_descriptor_config_destroy",
      panic,
    );
  });
}

/// # Safety
///
/// This is an FFI function called by Envoy. Both pointers must satisfy the dynamic module ABI
/// contract.
#[no_mangle]
pub unsafe extern "C" fn envoy_dynamic_module_on_rate_limit_descriptor_populate(
  config_ptr: abi::envoy_dynamic_module_type_rate_limit_descriptor_config_module_ptr,
  context_envoy_ptr: abi::envoy_dynamic_module_type_rate_limit_descriptor_context_envoy_ptr,
) -> bool {
  catch_unwind(AssertUnwindSafe(|| {
    let config = &*(config_ptr as *const Box<dyn RateLimitDescriptorConfig>);
    let mut context = unsafe { RateLimitDescriptorContext::new(context_envoy_ptr) };
    config.populate(&mut context)
  }))
  .unwrap_or_else(|panic| {
    crate::log_ffi_panic(
      "envoy_dynamic_module_on_rate_limit_descriptor_populate",
      panic,
    );
    false
  })
}
