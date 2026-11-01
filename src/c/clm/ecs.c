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

#include "ecs.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct clm_private_ecs_component {
    clm_id id;
    clm_ecs_constructor_fn constructor;
    clm_ecs_destructor_fn destructor;
    void *user;
    struct clm_private_ecs_component *next;
} clm_private_ecs_component;

typedef struct clm_private_ecs_blueprint {
    clm_id id;
    clm_id *component_ids;
    size_t component_count;
    struct clm_private_ecs_blueprint *next;
} clm_private_ecs_blueprint;

typedef struct clm_private_ecs_instance {
    clm_id component_id;
    void *value;
    clm_private_ecs_component *registration;
} clm_private_ecs_instance;

typedef struct clm_private_ecs_entity {
    clm_id id;
    clm_id blueprint_id;
    clm_private_ecs_instance *components;
    size_t component_count;
    struct clm_private_ecs_entity *next;
} clm_private_ecs_entity;

typedef struct clm_private_ecs_system {
    clm_id id;
    clm_id *component_ids;
    size_t component_count;
    clm_ecs_system_fn callback;
    void *user;
    struct clm_private_ecs_system *next;
} clm_private_ecs_system;

static clm_private_ecs_component *clm_private_ecs_components;
static clm_private_ecs_blueprint *clm_private_ecs_blueprints;
static clm_private_ecs_entity *clm_private_ecs_entities;
static clm_private_ecs_system *clm_private_ecs_systems;
static clm_id clm_private_ecs_next_instance = 1;
static bool clm_private_ecs_query_active;
static bool clm_private_ecs_reset_active;

static clm_private_ecs_component *clm_private_ecs_find_component(clm_id id)
{
    for (clm_private_ecs_component *component = clm_private_ecs_components;
         component != NULL; component = component->next) {
        if (component->id == id) {
            return component;
        }
    }
    return NULL;
}

static clm_private_ecs_blueprint *clm_private_ecs_find_blueprint(clm_id id)
{
    for (clm_private_ecs_blueprint *blueprint = clm_private_ecs_blueprints;
         blueprint != NULL; blueprint = blueprint->next) {
        if (blueprint->id == id) {
            return blueprint;
        }
    }
    return NULL;
}

static clm_private_ecs_entity *clm_private_ecs_find_entity(clm_id id)
{
    for (clm_private_ecs_entity *entity = clm_private_ecs_entities;
         entity != NULL; entity = entity->next) {
        if (entity->id == id) {
            return entity;
        }
    }
    return NULL;
}

static bool clm_private_ecs_entity_has_component(
    const clm_private_ecs_entity *entity, clm_id component_id)
{
    for (size_t index = 0; index < entity->component_count; ++index) {
        if (entity->components[index].component_id == component_id) {
            return true;
        }
    }
    return false;
}

static clm_ecs_status clm_private_ecs_copy_component_ids(
    const clm_id *component_ids, size_t count, clm_id **out_copy)
{
    if (out_copy == NULL || (count != 0 && component_ids == NULL)) {
        return CLM_ECS_INVALID;
    }
    *out_copy = NULL;
    if (count > ((size_t)-1) / sizeof(**out_copy)) {
        return CLM_ECS_INVALID;
    }
    for (size_t index = 0; index < count; ++index) {
        if (clm_private_ecs_find_component(component_ids[index]) == NULL) {
            return CLM_ECS_NOT_COMPONENT;
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (component_ids[previous] == component_ids[index]) {
                return CLM_ECS_DUPLICATE;
            }
        }
    }
    if (count == 0) {
        return CLM_ECS_SUCCESS;
    }
    *out_copy = (clm_id *)malloc(count * sizeof(**out_copy));
    if (*out_copy == NULL) {
        return CLM_ECS_NO_MEMORY;
    }
    memcpy(*out_copy, component_ids, count * sizeof(**out_copy));
    return CLM_ECS_SUCCESS;
}

static void clm_private_ecs_dispose_entity(clm_private_ecs_entity *entity)
{
    for (size_t index = 0; index < entity->component_count; ++index) {
        clm_private_ecs_instance *instance = &entity->components[index];
        if (instance->registration->destructor != NULL) {
            instance->registration->destructor(
                instance->value, instance->registration->user);
        }
    }
    free(entity->components);
    free(entity);
}

clm_ecs_status clm_ecs_register_component(
    clm_id component_id, clm_ecs_constructor_fn constructor,
    clm_ecs_destructor_fn destructor, void *user){
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return CLM_ECS_IN_USE;
    }
    if (constructor == NULL) {
        return CLM_ECS_INVALID;
    }
    if (clm_private_ecs_find_component(component_id) != NULL ||
        clm_private_ecs_find_blueprint(component_id) != NULL) {
        return CLM_ECS_DUPLICATE;
    }

    clm_private_ecs_component *component =
        (clm_private_ecs_component *)calloc(1, sizeof(*component));
    if (component == NULL) {
        return CLM_ECS_NO_MEMORY;
    }
    component->id = component_id;
    component->constructor = constructor;
    component->destructor = destructor;
    component->user = user;
    component->next = clm_private_ecs_components;
    clm_private_ecs_components = component;
    return CLM_ECS_SUCCESS;
}

clm_ecs_status clm_ecs_register_entity(
    const clm_id entity_blueprint_id,
    const clm_id *component_ids,
    const size_t count){
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return CLM_ECS_IN_USE;
    }
    if (clm_private_ecs_find_component(entity_blueprint_id) != NULL ||
        clm_private_ecs_find_blueprint(entity_blueprint_id) != NULL) {
        return CLM_ECS_DUPLICATE;
    }

    clm_id *component_copy = NULL;
    const clm_ecs_status status = clm_private_ecs_copy_component_ids(
        component_ids, count, &component_copy);
    if (status != CLM_ECS_SUCCESS) {
        return status;
    }
    clm_private_ecs_blueprint *blueprint =
        calloc(1, sizeof(*blueprint));
    if (blueprint == NULL) {
        free(component_copy);
        return CLM_ECS_NO_MEMORY;
    }
    blueprint->id = entity_blueprint_id;
    blueprint->component_ids = component_copy;
    blueprint->component_count = count;
    blueprint->next = clm_private_ecs_blueprints;
    clm_private_ecs_blueprints = blueprint;
    return CLM_ECS_SUCCESS;
}

clm_ecs_status clm_ecs_create_entity(
    const clm_id blueprint_id, clm_id *out_instance_id){
    if (out_instance_id == NULL) {
        return CLM_ECS_INVALID;
    }
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return CLM_ECS_IN_USE;
    }
    clm_private_ecs_blueprint *blueprint =
        clm_private_ecs_find_blueprint(blueprint_id);
    if (blueprint == NULL) {
        return CLM_ECS_NOT_FOUND;
    }
    if (clm_private_ecs_next_instance == 0) {
        return CLM_ECS_INVALID;
    }

    clm_private_ecs_entity *entity = calloc(1, sizeof(*entity));
    if (entity == NULL) {
        return CLM_ECS_NO_MEMORY;
    }
    entity->id = clm_private_ecs_next_instance++;
    entity->blueprint_id = blueprint_id;
    if (blueprint->component_count != 0) {
        entity->components = (clm_private_ecs_instance *)calloc(
            blueprint->component_count,
            sizeof(*entity->components));
        if (entity->components == NULL) {
            free(entity);
            return CLM_ECS_NO_MEMORY;
        }
    }

    for (size_t index = 0; index < blueprint->component_count; ++index) {
        clm_private_ecs_component *component =
            clm_private_ecs_find_component(blueprint->component_ids[index]);
        void *value = component->constructor(entity->id, component->id, component->user);
        if (value == NULL) {
            clm_private_ecs_dispose_entity(entity);
            return CLM_ECS_NO_MEMORY;
        }
        clm_private_ecs_instance *instance =
            &entity->components[entity->component_count++];
        instance->component_id = component->id;
        instance->value = value;
        instance->registration = component;
    }

    entity->next = clm_private_ecs_entities;
    clm_private_ecs_entities = entity;
    *out_instance_id = entity->id;
    return CLM_ECS_SUCCESS;
}

clm_ecs_status clm_ecs_delete_entity(const clm_id entity_instance_id){
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return CLM_ECS_IN_USE;
    }
    clm_private_ecs_entity **link = &clm_private_ecs_entities;
    while (*link != NULL && (*link)->id != entity_instance_id) {
        link = &(*link)->next;
    }
    clm_private_ecs_entity *entity = *link;
    if (entity == NULL) {
        return CLM_ECS_NOT_FOUND;
    }
    *link = entity->next;
    clm_private_ecs_dispose_entity(entity);
    return CLM_ECS_SUCCESS;
}

clm_ecs_status clm_ecs_attach_component(
    const clm_id entity_instance_id,
    const clm_id component_id,
    void **out_instance){
    if (out_instance != NULL) {
        *out_instance = NULL;
    }
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return CLM_ECS_IN_USE;
    }
    clm_private_ecs_entity *entity =
        clm_private_ecs_find_entity(entity_instance_id);
    if (entity == NULL) {
        return CLM_ECS_NOT_FOUND;
    }
    clm_private_ecs_component *component =
        clm_private_ecs_find_component(component_id);
    if (component == NULL) {
        return CLM_ECS_NOT_COMPONENT;
    }
    if (clm_private_ecs_entity_has_component(entity, component_id)) {
        return CLM_ECS_DUPLICATE;
    }
    if (entity->component_count >= ((size_t)-1) / sizeof(*entity->components)) {
        return CLM_ECS_NO_MEMORY;
    }
    const size_t next_count = entity->component_count + 1;
    if (next_count > ((size_t)-1) / sizeof(*entity->components)) {
        return CLM_ECS_NO_MEMORY;
    }

    void *value = component->constructor(
        entity_instance_id, component_id, component->user);
    if (value == NULL) {
        return CLM_ECS_NO_MEMORY;
    }
    clm_private_ecs_instance *components =
        realloc(entity->components, next_count * sizeof(*components));
    if (components == NULL) {
        if (component->destructor != NULL) {
            component->destructor(value, component->user);
        }
        return CLM_ECS_NO_MEMORY;
    }
    entity->components = components;
    clm_private_ecs_instance *instance = &entity->components[entity->component_count++];
    instance->component_id = component_id;
    instance->value = value;
    instance->registration = component;
    if (out_instance != NULL) {
        *out_instance = value;
    }
    return CLM_ECS_SUCCESS;
}

clm_ecs_status clm_ecs_remove_component(
    const clm_id entity_instance_id,
    const clm_id component_id){
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return CLM_ECS_IN_USE;
    }
    clm_private_ecs_entity *entity = clm_private_ecs_find_entity(entity_instance_id);
    if (entity == NULL) {
        return CLM_ECS_NOT_FOUND;
    }
    size_t index = 0;
    while (index < entity->component_count &&
        entity->components[index].component_id != component_id) {
        ++index;
    }
    if (index == entity->component_count) {
        return CLM_ECS_NOT_COMPONENT;
    }

    const clm_private_ecs_instance removed = entity->components[index];
    size_t remaining = entity->component_count - index - 1;
    if (remaining != 0) {
        memmove(&entity->components[index], &entity->components[index + 1],
            remaining * sizeof(*entity->components));
    }
    --entity->component_count;
    if (removed.registration->destructor != NULL) {
        removed.registration->destructor(removed.value, removed.registration->user);
    }
    return CLM_ECS_SUCCESS;
}

clm_ecs_status clm_ecs_get_component(
    const clm_id entity_instance_id,
    const clm_id component_id,
    void **out_instance){
    if (out_instance == NULL) {
        return CLM_ECS_INVALID;
    }
    *out_instance = NULL;
    const clm_private_ecs_entity *entity =
        clm_private_ecs_find_entity(entity_instance_id);
    if (entity == NULL) {
        return CLM_ECS_NOT_FOUND;
    }
    for (size_t index = 0; index < entity->component_count; ++index) {
        if (entity->components[index].component_id == component_id) {
            *out_instance = entity->components[index].value;
            return CLM_ECS_SUCCESS;
        }
    }
    return CLM_ECS_NOT_COMPONENT;
}

clm_ecs_status clm_ecs_register_system(
    const clm_id system_id,
    const clm_id *component_ids,
    const size_t count,
    const clm_ecs_system_fn callback,
    void *user){
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return CLM_ECS_IN_USE;
    }
    if (callback == NULL) {
        return CLM_ECS_INVALID;
    }
    for (clm_private_ecs_system *current = clm_private_ecs_systems;
         current != NULL; current = current->next) {
        if (current->id == system_id) {
            return CLM_ECS_DUPLICATE;
        }
    }

    clm_id *component_copy = NULL;
    const clm_ecs_status status = clm_private_ecs_copy_component_ids(
        component_ids, count, &component_copy);
    if (status != CLM_ECS_SUCCESS) {
        return status;
    }
    clm_private_ecs_system *system =
        calloc(1, sizeof(*system));
    if (system == NULL) {
        free(component_copy);
        return CLM_ECS_NO_MEMORY;
    }
    system->id = system_id;
    system->component_ids = component_copy;
    system->component_count = count;
    system->callback = callback;
    system->user = user;
    system->next = clm_private_ecs_systems;
    clm_private_ecs_systems = system;
    return CLM_ECS_SUCCESS;
}

static clm_ecs_status clm_private_ecs_validate_query(
    const clm_id *component_ids, size_t count)
{
    if (count != 0 && component_ids == NULL) {
        return CLM_ECS_INVALID;
    }
    for (size_t index = 0; index < count; ++index) {
        if (clm_private_ecs_find_component(component_ids[index]) == NULL) {
            return CLM_ECS_NOT_COMPONENT;
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (component_ids[previous] == component_ids[index]) {
                return CLM_ECS_DUPLICATE;
            }
        }
    }
    return CLM_ECS_SUCCESS;
}

clm_ecs_status clm_ecs_query(
    const clm_id *component_ids,
    const size_t count,
    const clm_ecs_query_fn visitor,
    void *user,
    size_t *matched){
    if (visitor == NULL) {
        return CLM_ECS_INVALID;
    }
    if (clm_private_ecs_query_active) {
        return CLM_ECS_IN_USE;
    }
    clm_ecs_status status =
        clm_private_ecs_validate_query(component_ids, count);
    if (status != CLM_ECS_SUCCESS) {
        return status;
    }
    if (count > ((size_t)-1) / sizeof(void *)) {
        return CLM_ECS_INVALID;
    }

    size_t total = 0;
    clm_private_ecs_query_active = true;
    for (clm_private_ecs_entity *entity = clm_private_ecs_entities;
         entity != NULL; entity = entity->next) {
        void **instances = count == 0
            ? NULL
            : (void **)malloc(count * sizeof(*instances));
        if (count != 0 && instances == NULL) {
            clm_private_ecs_query_active = false;
            return CLM_ECS_NO_MEMORY;
        }
        bool matches = true;
        for (size_t index = 0; index < count; ++index) {
            status = clm_ecs_get_component(
                entity->id, component_ids[index], &instances[index]);
            if (status != CLM_ECS_SUCCESS) {
                matches = false;
                break;
            }
        }
        if (matches) {
            ++total;
            bool keep_visiting = visitor(
                entity->id, component_ids, instances, count, user);
            free(instances);
            if (!keep_visiting) {
                clm_private_ecs_query_active = false;
                if (matched != NULL) {
                    *matched = total;
                }
                return CLM_ECS_SUCCESS;
            }
        } else {
            free(instances);
        }
    }
    clm_private_ecs_query_active = false;
    if (matched != NULL) {
        *matched = total;
    }
    return CLM_ECS_SUCCESS;
}

typedef struct clm_private_ecs_system_runner {
    clm_private_ecs_system *system;
    size_t processed;
} clm_private_ecs_system_runner;

static bool clm_private_ecs_run_system_for_entity(
    const clm_id entity_id,
    const clm_id *component_ids,
    void *const *instances,
    const size_t count, void *user)
{
    clm_private_ecs_system_runner *runner =
        (clm_private_ecs_system_runner *)user;
    runner->system->callback(entity_id, component_ids, instances, count,
        runner->system->user);
    ++runner->processed;
    return true;
}

clm_ecs_status clm_ecs_run_system(const clm_id system_id, size_t *processed){
    clm_private_ecs_system *system = clm_private_ecs_systems;
    while (system != NULL && system->id != system_id) {
        system = system->next;
    }
    if (system == NULL) {
        return CLM_ECS_NOT_FOUND;
    }
    clm_private_ecs_system_runner runner = {system, 0};
    clm_ecs_status status = clm_ecs_query(
        system->component_ids, system->component_count,
        clm_private_ecs_run_system_for_entity, &runner, NULL);
    if (status == CLM_ECS_SUCCESS && processed != NULL) {
        *processed = runner.processed;
    }
    return status;
}

void clm_ecs_reset(void){
    if (clm_private_ecs_query_active || clm_private_ecs_reset_active) {
        return;
    }
    clm_private_ecs_reset_active = true;
    while (clm_private_ecs_entities != NULL) {
        clm_private_ecs_entity *entity = clm_private_ecs_entities;
        clm_private_ecs_entities = entity->next;
        clm_private_ecs_dispose_entity(entity);
    }
    while (clm_private_ecs_components != NULL) {
        clm_private_ecs_component *next = clm_private_ecs_components->next;
        free(clm_private_ecs_components);
        clm_private_ecs_components = next;
    }
    while (clm_private_ecs_blueprints != NULL) {
        clm_private_ecs_blueprint *next = clm_private_ecs_blueprints->next;
        free(clm_private_ecs_blueprints->component_ids);
        free(clm_private_ecs_blueprints);
        clm_private_ecs_blueprints = next;
    }
    while (clm_private_ecs_systems != NULL) {
        clm_private_ecs_system *next = clm_private_ecs_systems->next;
        free(clm_private_ecs_systems->component_ids);
        free(clm_private_ecs_systems);
        clm_private_ecs_systems = next;
    }
    clm_private_ecs_next_instance = 1;
    clm_private_ecs_reset_active = false;
}

