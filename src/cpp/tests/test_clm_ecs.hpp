#pragma once

#include "test.hpp"
#include "../clm/ecs.hpp"

#include <stdlib.h>

#define CLM_ECS_TEST_POSITION 96301
#define CLM_ECS_TEST_RENDERABLE 96302
#define CLM_ECS_TEST_BLUEPRINT 96303
#define CLM_ECS_TEST_SYSTEM 96304

typedef struct clm_ecs_test_component {
    clm_id entity_id;
    clm_id component_id;
} clm_ecs_test_component;

typedef struct clm_ecs_test_query_state {
    size_t visits;
    clm_ecs_status mutation_status;
} clm_ecs_test_query_state;

static size_t clm_ecs_test_position_destroys;
static size_t clm_ecs_test_renderable_destroys;
static size_t clm_ecs_test_system_calls;

static void *clm_ecs_test_construct(
    clm_id entity_id, clm_id component_id, void *user)
{
    (void)user;
    clm_ecs_test_component *component =
        (clm_ecs_test_component *)malloc(sizeof(*component));
    if (component == NULL) {
        return NULL;
    }
    component->entity_id = entity_id;
    component->component_id = component_id;
    return component;
}

static void clm_ecs_test_destroy(void *instance, void *user)
{
    size_t *destroy_count = (size_t *)user;
    ++*destroy_count;
    free(instance);
}

static bool clm_ecs_test_visit(
    clm_id entity_id, const clm_id *component_ids,
    void *const *instances, size_t count, void *user)
{
    clm_ecs_test_query_state *state = (clm_ecs_test_query_state *)user;
    if (count != 1 || component_ids[0] != CLM_ECS_TEST_POSITION ||
        ((clm_ecs_test_component *)instances[0])->entity_id != entity_id) {
        return false;
    }
    ++state->visits;
    state->mutation_status = clm_ecs_attach_component(
        entity_id, CLM_ECS_TEST_RENDERABLE, NULL);
    return true;
}

static void clm_ecs_test_system(
    clm_id entity_id, const clm_id *component_ids,
    void *const *instances, size_t count, void *user)
{
    (void)entity_id;
    (void)component_ids;
    (void)instances;
    (void)count;
    (void)user;
    ++clm_ecs_test_system_calls;
}

static int clm_ecs_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - ECS");
    const clm_id blueprint_components[] = {
        CLM_ECS_TEST_POSITION, CLM_ECS_TEST_RENDERABLE};
    const clm_id query_components[] = {CLM_ECS_TEST_POSITION};
    clm_ecs_test_position_destroys = 0;
    clm_ecs_test_renderable_destroys = 0;
    clm_ecs_test_system_calls = 0;
    clm_ecs_reset();

    CLM_ASSERT(clm_ecs_register_component(CLM_ECS_TEST_POSITION,
        clm_ecs_test_construct, clm_ecs_test_destroy,
        &clm_ecs_test_position_destroys) == CLM_ECS_SUCCESS &&
        clm_ecs_register_component(CLM_ECS_TEST_RENDERABLE,
            clm_ecs_test_construct, clm_ecs_test_destroy,
            &clm_ecs_test_renderable_destroys) == CLM_ECS_SUCCESS &&
        clm_ecs_register_component(CLM_ECS_TEST_POSITION,
            clm_ecs_test_construct, clm_ecs_test_destroy,
            &clm_ecs_test_position_destroys) == CLM_ECS_DUPLICATE &&
        clm_ecs_register_entity(CLM_ECS_TEST_BLUEPRINT,
            blueprint_components, 2) == CLM_ECS_SUCCESS,
        "register component constructors and an entity blueprint");

    clm_id entity_id = 0;
    void *component = NULL;
    CLM_ASSERT(clm_ecs_create_entity(CLM_ECS_TEST_BLUEPRINT, &entity_id) ==
        CLM_ECS_SUCCESS && entity_id != 0 &&
        clm_ecs_get_component(entity_id, CLM_ECS_TEST_POSITION,
            &component) == CLM_ECS_SUCCESS && component != NULL &&
        ((clm_ecs_test_component *)component)->entity_id == entity_id &&
        clm_ecs_get_component(entity_id, CLM_ECS_TEST_RENDERABLE,
            &component) == CLM_ECS_SUCCESS &&
        clm_ecs_attach_component(entity_id, CLM_ECS_TEST_RENDERABLE, NULL) ==
            CLM_ECS_DUPLICATE,
        "create an entity with owned components and reject duplicates");

    CLM_ASSERT(clm_ecs_remove_component(entity_id,
        CLM_ECS_TEST_RENDERABLE) == CLM_ECS_SUCCESS &&
        clm_ecs_test_renderable_destroys == 1 &&
        clm_ecs_attach_component(entity_id, CLM_ECS_TEST_RENDERABLE,
            &component) == CLM_ECS_SUCCESS && component != NULL,
        "remove and reattach a component with destructor ownership");

    clm_ecs_test_query_state query_state = {0, CLM_ECS_SUCCESS};
    size_t matched = 0;
    size_t processed = 0;
    CLM_ASSERT(clm_ecs_query(query_components, 1, clm_ecs_test_visit,
        &query_state, &matched) == CLM_ECS_SUCCESS && matched == 1 &&
        query_state.visits == 1 &&
        query_state.mutation_status == CLM_ECS_IN_USE &&
        clm_ecs_register_system(CLM_ECS_TEST_SYSTEM, query_components, 1,
            clm_ecs_test_system, NULL) == CLM_ECS_SUCCESS &&
        clm_ecs_run_system(CLM_ECS_TEST_SYSTEM, &processed) == CLM_ECS_SUCCESS &&
        processed == 1 && clm_ecs_test_system_calls == 1,
        "query matching entities and execute a system without query mutation");

    CLM_ASSERT(clm_ecs_delete_entity(entity_id) == CLM_ECS_SUCCESS &&
        clm_ecs_test_position_destroys == 1 &&
        clm_ecs_test_renderable_destroys == 2 &&
        clm_ecs_get_component(entity_id, CLM_ECS_TEST_POSITION,
            &component) == CLM_ECS_NOT_FOUND,
        "destroy all component instances when deleting an entity");
    clm_ecs_reset();

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}


static int test_clm_ecs(void)
{
    return clm_ecs_tests();
}

