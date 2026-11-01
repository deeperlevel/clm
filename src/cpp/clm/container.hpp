/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#pragma once

#include "base.hpp"
#include "bpa.hpp"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef STATUS_ALREADY_REGISTERED
#undef STATUS_ALREADY_REGISTERED
#endif

#define CLM_CONTAINER_LIFETIME_SINGLETON 0
#define CLM_CONTAINER_LIFETIME_TRANSIENT 1
#define CLM_CONTAINER_LIFETIME_SCOPED 2

#define CLM_CONTAINER_REGISTRATION_SUCCESS 0
#define CLM_CONTAINER_REGISTRATION_FAILURE -1

#define CLM_CONTAINER_UNREGISTER_SUCCESS 0
#define CLM_CONTAINER_UNREGISTER_FAILURE -1

#define CLM_CONTAINER_STATUS_SUCCESS CLM_CONTAINER_REGISTRATION_SUCCESS
#define CLM_CONTAINER_STATUS_FAILURE CLM_CONTAINER_REGISTRATION_FAILURE
#define CLM_CONTAINER_STATUS_ALREADY_REGISTERED -2
#define CLM_CONTAINER_STATUS_NOT_REGISTERED -3
#define CLM_CONTAINER_STATUS_MISSING_DEPENDENCY -4
#define CLM_CONTAINER_STATUS_INVALID_ARGUMENT -5
#define CLM_CONTAINER_STATUS_UNSUPPORTED_LIFETIME -6
#define CLM_CONTAINER_STATUS_BUSY -7
#define CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE -8

typedef clm_int (*clm_container_constructor_fn)(
	clm_id service_id,
	clm_id context_id,
	clm_id* out_instance_id);

typedef clm_int (*clm_container_invoke_fn)(clm_id context_id);

namespace clm::container {

inline constexpr clm_int LIFETIME_SINGLETON = CLM_CONTAINER_LIFETIME_SINGLETON;
inline constexpr clm_int LIFETIME_TRANSIENT = CLM_CONTAINER_LIFETIME_TRANSIENT;
inline constexpr clm_int LIFETIME_SCOPED = CLM_CONTAINER_LIFETIME_SCOPED;

inline constexpr clm_int STATUS_SUCCESS = CLM_CONTAINER_STATUS_SUCCESS;
inline constexpr clm_int STATUS_FAILURE = CLM_CONTAINER_STATUS_FAILURE;
inline constexpr clm_int STATUS_ALREADY_REGISTERED = CLM_CONTAINER_STATUS_ALREADY_REGISTERED;
inline constexpr clm_int STATUS_NOT_REGISTERED = CLM_CONTAINER_STATUS_NOT_REGISTERED;
inline constexpr clm_int STATUS_MISSING_DEPENDENCY = CLM_CONTAINER_STATUS_MISSING_DEPENDENCY;
inline constexpr clm_int STATUS_INVALID_ARGUMENT = CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
inline constexpr clm_int STATUS_UNSUPPORTED_LIFETIME = CLM_CONTAINER_STATUS_UNSUPPORTED_LIFETIME;
inline constexpr clm_int STATUS_BUSY = CLM_CONTAINER_STATUS_BUSY;
inline constexpr clm_int STATUS_CONSTRUCTOR_FAILURE = CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;

using constructor_fn = clm_container_constructor_fn;
using invoke_fn = clm_container_invoke_fn;

namespace detail {

struct service_entry {
	clm_id id{0};
	clm_id context_id{0};
	clm_id singleton_instance_id{0};
	constructor_fn constructor{nullptr};
	clm_int lifetime{LIFETIME_SINGLETON};
	std::size_t active_calls{0};
	bool constructing{false};
	bool has_singleton_instance{false};
};

class container_registry {
public:
	static container_registry& instance()
	{
		static container_registry reg;
		return reg;
	}

	std::unique_lock<std::mutex> lock()
	{
		return std::unique_lock<std::mutex>(mtx_);
	}

	service_entry* find_locked(const clm_id id)
	{
		auto it = services_.find(id);
		return (it != services_.end()) ? it->second.get() : nullptr;
	}

	void add_locked(const clm_id id, std::unique_ptr<service_entry> entry)
	{
		services_.emplace(id, std::move(entry));
	}

	void remove_locked(const clm_id id)
	{
		services_.erase(id);
	}

private:
	std::mutex mtx_;
	std::unordered_map<clm_id, std::unique_ptr<service_entry>> services_;
};

inline bool instance_valid(const clm_id instance_id)
{
	const auto* block = clm::bpa::detail::find_block(instance_id);
	return block != nullptr && block->type == clm::bpa::CLM_BPA_TYPE_STRUCT;
}

} // namespace detail

inline clm_int register_service(
	const clm_id service_id,
	const clm_id* dependencies,
	const std::size_t dependency_count,
	const constructor_fn constructor,
	const clm_int lifetime,
	const clm_id context_id)
{
	if (!constructor || (dependency_count != 0 && !dependencies))
		return STATUS_INVALID_ARGUMENT;
	if (lifetime != LIFETIME_SINGLETON && lifetime != LIFETIME_TRANSIENT)
		return STATUS_UNSUPPORTED_LIFETIME;

	{
		auto lk = detail::container_registry::instance().lock();
		if (detail::container_registry::instance().find_locked(service_id))
			return STATUS_ALREADY_REGISTERED;
		for (std::size_t i = 0; i < dependency_count; ++i) {
			if (!detail::container_registry::instance().find_locked(dependencies[i]))
				return STATUS_MISSING_DEPENDENCY;
		}

		auto s = std::make_unique<detail::service_entry>();
		s->id = service_id;
		s->context_id = context_id;
		s->constructor = constructor;
		s->lifetime = lifetime;
		s->constructing = (lifetime == LIFETIME_SINGLETON);
		detail::container_registry::instance().add_locked(service_id, std::move(s));
	}

	if (lifetime == LIFETIME_TRANSIENT)
		return STATUS_SUCCESS;

	clm_id instance_id = 0;
	const clm_int construct_status = constructor(service_id, context_id, &instance_id);
	const bool valid = (construct_status == STATUS_SUCCESS) && detail::instance_valid(instance_id);

	{
		auto lk = detail::container_registry::instance().lock();
		auto* s = detail::container_registry::instance().find_locked(service_id);
		if (s) {
			s->constructing = false;
			if (valid) {
				s->singleton_instance_id = instance_id;
				s->has_singleton_instance = true;
			} else {
				detail::container_registry::instance().remove_locked(service_id);
			}
		}
	}

	if (!valid)
		return STATUS_CONSTRUCTOR_FAILURE;
	return STATUS_SUCCESS;
}

inline clm_int resolve(const clm_id service_id, clm_id* out_instance_id)
{
	if (!out_instance_id)
		return STATUS_INVALID_ARGUMENT;

	constructor_fn constructor = nullptr;
	clm_id context_id = 0;

	{
		auto lk = detail::container_registry::instance().lock();
		auto* s = detail::container_registry::instance().find_locked(service_id);
		if (!s)
			return STATUS_NOT_REGISTERED;

		if (s->lifetime == LIFETIME_SINGLETON) {
			if (s->constructing)
				return STATUS_BUSY;
			if (!s->has_singleton_instance || !detail::instance_valid(s->singleton_instance_id))
				return STATUS_CONSTRUCTOR_FAILURE;
			*out_instance_id = s->singleton_instance_id;
			return STATUS_SUCCESS;
		}

		++s->active_calls;
		constructor = s->constructor;
		context_id = s->context_id;
	}

	clm_id instance_id = 0;
	const clm_int construct_status = constructor(service_id, context_id, &instance_id);
	const bool valid = (construct_status == STATUS_SUCCESS) && detail::instance_valid(instance_id);

	{
		auto lk = detail::container_registry::instance().lock();
		auto* s = detail::container_registry::instance().find_locked(service_id);
		if (s && s->active_calls > 0)
			--s->active_calls;
	}

	if (!valid)
		return STATUS_CONSTRUCTOR_FAILURE;

	*out_instance_id = instance_id;
	return STATUS_SUCCESS;
}

inline clm_int invoke(
	const clm_id* dependencies,
	const std::size_t dependency_count,
	const invoke_fn function,
	const clm_id context_id)
{
	if (!function || (dependency_count != 0 && !dependencies))
		return STATUS_INVALID_ARGUMENT;

	{
		auto lk = detail::container_registry::instance().lock();
		std::size_t held = 0;
		for (; held < dependency_count; ++held) {
			auto* s = detail::container_registry::instance().find_locked(dependencies[held]);
			if (!s) {
				for (std::size_t r = 0; r < held; ++r) {
					auto* rs = detail::container_registry::instance().find_locked(dependencies[r]);
					if (rs && rs->active_calls > 0)
						--rs->active_calls;
				}
				return STATUS_MISSING_DEPENDENCY;
			}
			++s->active_calls;
		}
	}

	const clm_int result = function(context_id);

	{
		auto lk = detail::container_registry::instance().lock();
		for (std::size_t i = 0; i < dependency_count; ++i) {
			auto* s = detail::container_registry::instance().find_locked(dependencies[i]);
			if (s && s->active_calls > 0)
				--s->active_calls;
		}
	}

	return result;
}

inline clm_int unregister_service(const clm_id service_id)
{
	auto lk = detail::container_registry::instance().lock();
	auto* s = detail::container_registry::instance().find_locked(service_id);
	if (!s)
		return STATUS_NOT_REGISTERED;
	if (s->constructing || s->active_calls > 0)
		return STATUS_BUSY;

	if (s->has_singleton_instance && clm_bpa_delete(s->singleton_instance_id) != clm::bpa::CLM_BPA_SUCCESS)
		return STATUS_FAILURE;

	detail::container_registry::instance().remove_locked(service_id);
	return STATUS_SUCCESS;
}

} // namespace clm::container

static inline clm_int clm_container_register(
	const clm_id service_id,
	const clm_id* dependencies,
	const std::size_t dependency_count,
	const clm_container_constructor_fn constructor,
	const clm_int lifetime,
	const clm_id context_id)
{
	return clm::container::register_service(service_id, dependencies, dependency_count, constructor, lifetime, context_id);
}

static inline clm_int clm_container_resolve(const clm_id service_id, clm_id* out_instance_id)
{
	return clm::container::resolve(service_id, out_instance_id);
}

static inline clm_int clm_container_invoke(
	const clm_id* dependencies,
	const std::size_t dependency_count,
	const clm_container_invoke_fn function,
	const clm_id context_id)
{
	return clm::container::invoke(dependencies, dependency_count, function, context_id);
}

static inline clm_int clm_container_unregister(const clm_id service_id)
{
	return clm::container::unregister_service(service_id);
}

