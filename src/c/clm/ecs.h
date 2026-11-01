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
#include <stdbool.h>

typedef enum clm_ecs_status {
    CLM_ECS_SUCCESS = 0,
    CLM_ECS_INVALID = -1,
    CLM_ECS_DUPLICATE = -2,
    CLM_ECS_NOT_FOUND = -3,
    CLM_ECS_NOT_COMPONENT = -4,
    CLM_ECS_IN_USE = -5,
    CLM_ECS_NO_MEMORY = -6
} clm_ecs_status;

/** Constructs component storage for a specific entity instance and type. */
typedef void *(*clm_ecs_constructor_fn)(
    clm_id entity_instance_id, clm_id component_id, void *user);

/** Releases component storage created by its registered constructor. */
typedef void (*clm_ecs_destructor_fn)(void *instance, void *user);

/** Processes one entity matching a system's required component set. */
typedef void (*clm_ecs_system_fn)(
    clm_id entity_instance_id, const clm_id *component_ids,
    void *const *instances, size_t component_count, void *user);

/** Visits a matching entity; return false to stop the query successfully. */
typedef bool (*clm_ecs_query_fn)(
    clm_id entity_instance_id, const clm_id *component_ids,
    void *const *instances, size_t component_count, void *user);

/** Registers a component type and its construction and destruction callbacks. 
 * @param component_id CLM ID identifying the component.
 * @param constructor Input parameter described above.
 * @param destructor Input parameter described above.
 * @param user Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_register_component(
    clm_id component_id, clm_ecs_constructor_fn constructor,
    clm_ecs_destructor_fn destructor, void *user);

/** Registers an entity blueprint by copying its component type IDs. 
 * @param entity_blueprint_id CLM ID identifying the entity blueprint.
 * @param component_ids Input data or buffer; not modified.
 * @param count Number of elements or items.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_register_entity(
    clm_id entity_blueprint_id, const clm_id *component_ids, size_t count);

/** Creates an entity instance and constructs each blueprint component. 
 * @param blueprint_id CLM ID identifying the blueprint.
 * @param out_instance_id Receives the instance id.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_create_entity(
    clm_id blueprint_id, clm_id *out_instance_id);

/** Deletes an entity and destroys all its attached components. 
 * @param entity_instance_id CLM ID identifying the entity instance.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_delete_entity(clm_id entity_instance_id);

/** Constructs and attaches a component not already present on the entity. 
 * @param entity_instance_id CLM ID identifying the entity instance.
 * @param component_id CLM ID identifying the component.
 * @param out_instance Receives the instance.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_attach_component(
    clm_id entity_instance_id, clm_id component_id, void **out_instance);

/** Removes and destroys one attached component. 
 * @param entity_instance_id CLM ID identifying the entity instance.
 * @param component_id CLM ID identifying the component.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_remove_component(
    clm_id entity_instance_id, clm_id component_id);

/** Returns a borrowed pointer to an attached component instance. 
 * @param entity_instance_id CLM ID identifying the entity instance.
 * @param component_id CLM ID identifying the component.
 * @param out_instance Receives the instance.
 * @return A borrowed pointer to an attached component instance.
 */
clm_ecs_status clm_ecs_get_component(
    clm_id entity_instance_id, clm_id component_id, void **out_instance);

/** Registers a system and copies its required component type IDs. 
 * @param system_id CLM ID identifying the system.
 * @param component_ids Input data or buffer; not modified.
 * @param count Number of elements or items.
 * @param callback Callback to invoke.
 * @param user Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_register_system(
    clm_id system_id, const clm_id *component_ids, size_t count,
    clm_ecs_system_fn callback, void *user);

/** Visits entities containing every requested component type. 
 * @param component_ids Input data or buffer; not modified.
 * @param count Number of elements or items.
 * @param visitor Input parameter described above.
 * @param user Writable output or in/out pointer.
 * @param matched Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_query(
    const clm_id *component_ids, size_t count, clm_ecs_query_fn visitor,
    void *user, size_t *matched);

/** Runs a registered system and optionally returns the processed entity count. 
 * @param system_id CLM ID identifying the system.
 * @param processed Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_ecs_status clm_ecs_run_system(clm_id system_id, size_t *processed);

/** Destroys entities and releases registrations; ignored during active queries. */
void clm_ecs_reset(void);

/* Define CLM_ECS_IMPLEMENTATION in one translation unit, or define
 * CLM_IMPLEMENTATION once for all single-header modules.
 */
