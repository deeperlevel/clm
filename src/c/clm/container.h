#pragma once

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

#include "base.h"
#include "bpa.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

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

/** Constructs one service instance and writes its BPA STRUCT block ID.
 * Dependencies are NOT passed to the constructor. It resolves them explicitly
 * with clm_container_resolve(). On failure, the constructor must clean up any
 * partially created BPA resources. On success, out_instance_id must identify
 * a newly created CLM_BPA_TYPE_STRUCT block. If construction fails or returns
 * an invalid instance, the constructor remains responsible for partial cleanup.
 */
typedef clm_int (*clm_container_constructor_fn)(
	clm_id service_id,
	clm_id context_id,
	clm_id* out_instance_id);

/** Function invoked after its declared dependencies are verified registered.
 * The callback resolves any services it needs itself; no IDs are injected.
 */
typedef clm_int (*clm_container_invoke_fn)(clm_id context_id);

typedef struct clm_container_internal_service {
	clm_id id;
	clm_id context_id;
	clm_id singleton_instance_id;
	clm_container_constructor_fn constructor;
	clm_int lifetime;
	size_t active_calls;
	bool constructing;
	bool has_singleton_instance;
	struct clm_container_internal_service* next;
} clm_container_internal_service;

extern clm_container_internal_service* clm_container_internal_services;
extern atomic_flag clm_container_internal_lock;

#ifdef CLM_CONTAINER_IMPLEMENTATION
clm_container_internal_service* clm_container_internal_services = NULL;
atomic_flag clm_container_internal_lock = ATOMIC_FLAG_INIT;
#endif

static inline void clm_container_internal_acquire(void)
{
	while (atomic_flag_test_and_set_explicit(&clm_container_internal_lock,
		memory_order_acquire)) { }
}

static inline void clm_container_internal_release(void)
{
	atomic_flag_clear_explicit(&clm_container_internal_lock,
		memory_order_release);
}

static inline clm_container_internal_service* clm_container_internal_find(
	const clm_id service_id)
{
	for (clm_container_internal_service* service =
		clm_container_internal_services; service != NULL; service = service->next)
		if (service->id == service_id)
			return service;
	return NULL;
}

static inline bool clm_container_internal_instance_valid(const clm_id instance_id)
{
	const clm_bpa_block_node* block = clm_bpa_find_block(instance_id);
	return block != NULL && block->type == CLM_BPA_TYPE_STRUCT;
}

/** Registers a service after validating that every dependency is registered.
 * dependencies is used only for validation and is not retained or injected.
 * SINGLETON invokes the constructor immediately. TRANSIENT invokes it on
 * each resolve. SCOPED is reserved and returns UNSUPPORTED_LIFETIME.
 * The container owns a singleton BPA block and deletes it on unregister.
 * Transient instance blocks are owned by the resolve caller and must be deleted
 * with clm_bpa_delete() when no longer needed.
 */
static inline clm_int clm_container_register(
	const clm_id service_id,
	const clm_id* dependencies,
	const size_t dependency_count,
	const clm_container_constructor_fn constructor,
	const clm_int lifetime,
	const clm_id context_id)
{
	if (constructor == NULL || (dependency_count != 0 && dependencies == NULL))
		return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
	if (lifetime != CLM_CONTAINER_LIFETIME_SINGLETON &&
		lifetime != CLM_CONTAINER_LIFETIME_TRANSIENT)
		return CLM_CONTAINER_STATUS_UNSUPPORTED_LIFETIME;
	clm_container_internal_acquire();
	if (clm_container_internal_find(service_id) != NULL) {
		clm_container_internal_release();
		return CLM_CONTAINER_STATUS_ALREADY_REGISTERED;
	}
	for (size_t index = 0; index < dependency_count; ++index)
		if (clm_container_internal_find(dependencies[index]) == NULL) {
			clm_container_internal_release();
			return CLM_CONTAINER_STATUS_MISSING_DEPENDENCY;
		}
	clm_container_internal_service* service =
		(clm_container_internal_service*)calloc(1, sizeof(*service));
	if (service == NULL) {
		clm_container_internal_release();
		return CLM_CONTAINER_STATUS_FAILURE;
	}
	service->id = service_id;
	service->context_id = context_id;
	service->constructor = constructor;
	service->lifetime = lifetime;
	service->constructing = lifetime == CLM_CONTAINER_LIFETIME_SINGLETON;
	service->next = clm_container_internal_services;
	clm_container_internal_services = service;
	clm_container_internal_release();
	if (lifetime == CLM_CONTAINER_LIFETIME_TRANSIENT)
		return CLM_CONTAINER_REGISTRATION_SUCCESS;
	clm_id instance_id = 0;
	const clm_int construct_status = constructor(service_id, context_id,
		&instance_id);
	const bool valid_instance = construct_status == CLM_CONTAINER_STATUS_SUCCESS &&
		clm_container_internal_instance_valid(instance_id);
	clm_container_internal_acquire();
	service = clm_container_internal_find(service_id);
	if (service != NULL) {
		service->constructing = false;
		if (valid_instance) {
			service->singleton_instance_id = instance_id;
			service->has_singleton_instance = true;
		} else {
			clm_container_internal_service** link =
				&clm_container_internal_services;
			while (*link != NULL && *link != service)
				link = &(*link)->next;
			if (*link == service)
				*link = service->next;
		}
	}
	clm_container_internal_release();
	if (!valid_instance) {
		free(service);
		return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
	}
	return CLM_CONTAINER_REGISTRATION_SUCCESS;
}

/** Resolves a service to a BPA STRUCT instance ID.
 * SINGLETON returns the eagerly constructed shared instance. TRANSIENT calls
 * the constructor on every resolve; the caller owns and deletes that block.
 * The constructor runs without the registry lock and may resolve dependencies.
 * Recursive resolution of a singleton while it is constructing returns BUSY.
 */
static inline clm_int clm_container_resolve(
	const clm_id service_id,
	clm_id* out_instance_id)
{
	if (out_instance_id == NULL)
		return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
	clm_container_internal_acquire();
	clm_container_internal_service* service =
		clm_container_internal_find(service_id);
	if (service == NULL) {
		clm_container_internal_release();
		return CLM_CONTAINER_STATUS_NOT_REGISTERED;
	}
	if (service->lifetime == CLM_CONTAINER_LIFETIME_SINGLETON) {
		if (service->constructing) {
			clm_container_internal_release();
			return CLM_CONTAINER_STATUS_BUSY;
		}
		const clm_id instance_id = service->singleton_instance_id;
		const bool has_instance = service->has_singleton_instance;
		clm_container_internal_release();
		if (!has_instance || !clm_container_internal_instance_valid(instance_id))
			return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
		*out_instance_id = instance_id;
		return CLM_CONTAINER_STATUS_SUCCESS;
	}
	++service->active_calls;
	const clm_container_constructor_fn constructor = service->constructor;
	const clm_id context_id = service->context_id;
	clm_container_internal_release();
	clm_id instance_id = 0;
	const clm_int construct_status = constructor(service_id, context_id,
		&instance_id);
	const bool valid_instance = construct_status == CLM_CONTAINER_STATUS_SUCCESS &&
		clm_container_internal_instance_valid(instance_id);
	clm_container_internal_acquire();
	service = clm_container_internal_find(service_id);
	if (service != NULL && service->active_calls != 0)
		--service->active_calls;
	clm_container_internal_release();
	if (!valid_instance) {
		return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
	}
	*out_instance_id = instance_id;
	return CLM_CONTAINER_STATUS_SUCCESS;
}

/** Runs function after verifying every declared dependency is registered.
 * The IDs are validation-only; function resolves anything it needs itself.
 */
static inline clm_int clm_container_invoke(
	const clm_id* dependencies,
	const size_t dependency_count,
	const clm_container_invoke_fn function,
	const clm_id context_id)
{
	if (function == NULL || (dependency_count != 0 && dependencies == NULL))
		return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
	clm_container_internal_acquire();
	size_t held = 0;
	for (; held < dependency_count; ++held) {
		clm_container_internal_service* service =
			clm_container_internal_find(dependencies[held]);
		if (service == NULL) {
			for (size_t rollback = 0; rollback < held; ++rollback) {
				service = clm_container_internal_find(dependencies[rollback]);
				if (service != NULL && service->active_calls != 0)
					--service->active_calls;
			}
			clm_container_internal_release();
			return CLM_CONTAINER_STATUS_MISSING_DEPENDENCY;
		}
		++service->active_calls;
	}
	clm_container_internal_release();
	const clm_int result = function(context_id);
	clm_container_internal_acquire();
	for (size_t index = 0; index < dependency_count; ++index) {
		clm_container_internal_service* service =
			clm_container_internal_find(dependencies[index]);
		if (service != NULL && service->active_calls != 0)
			--service->active_calls;
	}
	clm_container_internal_release();
	return result;
}

/** Removes a service registration.
 * A service currently constructing or resolving cannot be unregistered.
 * The container deletes an owned singleton BPA block; transient blocks remain
 * caller-owned. The caller must ensure no users retain a singleton ID.
 */
static inline clm_int clm_container_unregister(const clm_id service_id)
{
	clm_container_internal_acquire();
	clm_container_internal_service** link = &clm_container_internal_services;
	while (*link != NULL && (*link)->id != service_id)
		link = &(*link)->next;
	clm_container_internal_service* service = *link;
	if (service == NULL) {
		clm_container_internal_release();
		return CLM_CONTAINER_STATUS_NOT_REGISTERED;
	}
	if (service->constructing || service->active_calls != 0) {
		clm_container_internal_release();
		return CLM_CONTAINER_STATUS_BUSY;
	}
	if (service->has_singleton_instance &&
		clm_bpa_delete(service->singleton_instance_id) != CLM_BPA_SUCCESS) {
		clm_container_internal_release();
		return CLM_CONTAINER_STATUS_FAILURE;
	}
	*link = service->next;
	clm_container_internal_release();
	free(service);
	return CLM_CONTAINER_UNREGISTER_SUCCESS;
}
