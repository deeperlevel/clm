#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "base.h"
#include "bpa.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#define CLM_CONTAINER_LIFETIME_SINGLETON 0
#define CLM_CONTAINER_LIFETIME_TRANSIENT 1
/** One cached instance per active process-wide scope frame. */
#define CLM_CONTAINER_LIFETIME_SCOPED 2
/** One cached instance per active process-wide intern frame. */
#define CLM_CONTAINER_LIFETIME_INTERN 3

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


void clm_container_internal_acquire(void);

void clm_container_internal_release(void);

clm_container_internal_service* clm_container_internal_find(const clm_id service_id);

bool clm_container_internal_instance_valid(const clm_id instance_id);

/** Registers a service after validating that every dependency is registered.
 * dependencies is used only for validation and is not retained or injected.
 * SINGLETON invokes the constructor immediately. TRANSIENT invokes it on each
 * resolve. SCOPED and INTERN are constructed at their matching begin call and
 * remain cached until the matching end call. Register scoped services only
 * while their frame is inactive. The container owns singleton and frame
 * instances; transient instance blocks are owned by the resolve caller and
 * must be deleted with clm_bpa_delete() when no longer needed.
 * @param service_id CLM ID identifying the service.
 * @param dependencies Input data or buffer; not modified.
 * @param dependency_count Input parameter described above.
 * @param constructor Input parameter described above.
 * @param lifetime Input parameter described above.
 * @param context_id CLM ID identifying the context.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_container_register(
	const clm_id service_id,
	const clm_id* dependencies,
	const size_t dependency_count,
	const clm_container_constructor_fn constructor,
	const clm_int lifetime,
	const clm_id context_id
);

/** Resolves a service to a BPA STRUCT instance ID.
 * SINGLETON returns the eagerly constructed shared instance. TRANSIENT calls
 * the constructor on every resolve; the caller owns and deletes that block.
 * The constructor runs without the registry lock and may resolve dependencies.
 * Recursive resolution of a singleton while it is constructing returns BUSY.
 * @param service_id CLM ID identifying the service.
 * @param out_instance_id Receives the instance id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_container_resolve(
	const clm_id service_id,
	clm_id* out_instance_id
);

/** Runs function after verifying every declared dependency is registered.
 * The IDs are validation-only; function resolves anything it needs itself.
 * @param dependencies Input data or buffer; not modified.
 * @param dependency_count Input parameter described above.
 * @param function Input parameter described above.
 * @param context_id CLM ID identifying the context.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_container_invoke(
	const clm_id* dependencies,
	const size_t dependency_count,
	const clm_container_invoke_fn function,
	const clm_id context_id
);

/** Removes a service registration.
 * A service currently constructing or resolving cannot be unregistered.
 * The container deletes an owned singleton BPA block; transient blocks remain
 * caller-owned. The caller must ensure no users retain a singleton ID.
 * @param service_id CLM ID identifying the service.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_container_unregister(const clm_id service_id);

/** Starts the process-wide scope frame and constructs registered SCOPED services.
 * Repeated or overlapping frame transitions return CLM_CONTAINER_STATUS_BUSY.
 * Constructor failure rolls back instances created by this call.
 * @return SUCCESS, FAILURE, CONSTRUCTOR_FAILURE, or BUSY.
 */
clm_int clm_container_begin_scope();

/** Destroys this scope's SCOPED instances and ends the frame.
 * It returns BUSY while a declared dependency invocation is active.
 * @return SUCCESS, INVALID_ARGUMENT if no scope is active, BUSY, or FAILURE.
 */
clm_int clm_container_end_scope();

/** Reports whether a process-wide scope frame is active.
 * @return 1 if active; otherwise 0.
 */
clm_int clm_container_is_scope();

/** Starts the process-wide intern frame and constructs registered INTERN services.
 * Repeated or overlapping frame transitions return CLM_CONTAINER_STATUS_BUSY.
 * Constructor failure rolls back instances created by this call.
 * @return SUCCESS, FAILURE, CONSTRUCTOR_FAILURE, or BUSY.
 */
clm_int clm_container_begin_intern();

/** Destroys this intern frame's INTERN instances and ends the frame.
 * It returns BUSY while a declared dependency invocation is active.
 * @return SUCCESS, INVALID_ARGUMENT if no intern frame is active, BUSY, or FAILURE.
 */
clm_int clm_container_end_intern();

/** Reports whether a process-wide intern frame is active.
 * @return 1 if active; otherwise 0.
 */
clm_int clm_container_is_intern();