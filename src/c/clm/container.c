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

#include "container.h"

clm_container_internal_service* clm_container_internal_services = NULL;
atomic_flag clm_container_internal_lock = ATOMIC_FLAG_INIT;

typedef enum clm_container_internal_frame_state {
    CLM_CONTAINER_FRAME_INACTIVE,
    CLM_CONTAINER_FRAME_STARTING,
    CLM_CONTAINER_FRAME_ACTIVE,
    CLM_CONTAINER_FRAME_ENDING
} clm_container_internal_frame_state;

static clm_container_internal_frame_state clm_container_scope_state = CLM_CONTAINER_FRAME_INACTIVE;
static clm_container_internal_frame_state clm_container_intern_state = CLM_CONTAINER_FRAME_INACTIVE;

static bool clm_container_internal_transitioning(void) {
    return clm_container_scope_state == CLM_CONTAINER_FRAME_STARTING ||
        clm_container_scope_state == CLM_CONTAINER_FRAME_ENDING ||
        clm_container_intern_state == CLM_CONTAINER_FRAME_STARTING ||
        clm_container_intern_state == CLM_CONTAINER_FRAME_ENDING;
}

void clm_container_internal_acquire(void) {
    while (atomic_flag_test_and_set_explicit(&clm_container_internal_lock, memory_order_acquire)) {}
}

void clm_container_internal_release(void) {
    atomic_flag_clear_explicit(&clm_container_internal_lock, memory_order_release);
}

clm_container_internal_service* clm_container_internal_find(const clm_id service_id) {
    for (clm_container_internal_service* service = clm_container_internal_services; service != NULL; service = service->next) {
        if (service->id == service_id) {
            return service;
        }
    }
    return NULL;
}

bool clm_container_internal_instance_valid(const clm_id instance_id) {
    const clm_bpa_block_node* block = clm_bpa_find_block(instance_id);
    return block != NULL && block->type == CLM_BPA_TYPE_STRUCT;
}

clm_int clm_container_register(
    const clm_id service_id,
    const clm_id* dependencies,
    const size_t dependency_count,
    const clm_container_constructor_fn constructor,
    const clm_int lifetime,
    const clm_id context_id
) {
    if (constructor == NULL || (dependency_count != 0 && dependencies == NULL)) {
        return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
    }
    if (lifetime != CLM_CONTAINER_LIFETIME_SINGLETON &&
        lifetime != CLM_CONTAINER_LIFETIME_TRANSIENT &&
        lifetime != CLM_CONTAINER_LIFETIME_SCOPED &&
        lifetime != CLM_CONTAINER_LIFETIME_INTERN) {
        return CLM_CONTAINER_STATUS_UNSUPPORTED_LIFETIME;
    }
    clm_container_internal_acquire();
    if (clm_container_internal_transitioning() ||
        (lifetime == CLM_CONTAINER_LIFETIME_SCOPED && clm_container_scope_state != CLM_CONTAINER_FRAME_INACTIVE) ||
        (lifetime == CLM_CONTAINER_LIFETIME_INTERN && clm_container_intern_state != CLM_CONTAINER_FRAME_INACTIVE)) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_BUSY;
    }
    if (clm_container_internal_find(service_id) != NULL) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_ALREADY_REGISTERED;
    }
    for (size_t index = 0; index < dependency_count; ++index) {
        if (clm_container_internal_find(dependencies[index]) == NULL) {
            clm_container_internal_release();
            return CLM_CONTAINER_STATUS_MISSING_DEPENDENCY;
        }
    }
    clm_container_internal_service* service = calloc(1, sizeof(*service));
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
    if (lifetime != CLM_CONTAINER_LIFETIME_SINGLETON) {
        return CLM_CONTAINER_REGISTRATION_SUCCESS;
    }
    clm_id instance_id = 0;
    const clm_int construct_status = constructor(service_id, context_id, &instance_id);
    const bool valid_instance = construct_status == CLM_CONTAINER_STATUS_SUCCESS && clm_container_internal_instance_valid(instance_id);
    clm_container_internal_acquire();
    service = clm_container_internal_find(service_id);
    if (service != NULL) {
        service->constructing = false;
        if (valid_instance) {
            service->singleton_instance_id = instance_id;
            service->has_singleton_instance = true;
        }
        else {
            clm_container_internal_service** link = &clm_container_internal_services;
            while (*link != NULL && *link != service) {
                link = &(*link)->next;
            }
            if (*link == service) {
                *link = service->next;
            }
        }
    }
    clm_container_internal_release();
    if (!valid_instance) {
        free(service);
        return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
    }
    return CLM_CONTAINER_REGISTRATION_SUCCESS;
}

clm_int clm_container_resolve(const clm_id service_id, clm_id* out_instance_id) {
    if (out_instance_id == NULL) {
        return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
    }
    clm_container_internal_acquire();
    clm_container_internal_service* service = clm_container_internal_find(service_id);
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
        if (!has_instance || !clm_container_internal_instance_valid(instance_id)) {
            return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
        }
        *out_instance_id = instance_id;
        return CLM_CONTAINER_STATUS_SUCCESS;
    }
    if (service->lifetime == CLM_CONTAINER_LIFETIME_SCOPED ||
        service->lifetime == CLM_CONTAINER_LIFETIME_INTERN) {
        const clm_container_internal_frame_state state = service->lifetime == CLM_CONTAINER_LIFETIME_SCOPED
                                                                ? clm_container_scope_state
                                                                : clm_container_intern_state;
        if (state == CLM_CONTAINER_FRAME_INACTIVE ||
            state == CLM_CONTAINER_FRAME_ENDING || service->constructing) {
            clm_container_internal_release();
            return CLM_CONTAINER_STATUS_BUSY;
        }
        const clm_id instance_id = service->singleton_instance_id;
        const bool has_instance = service->has_singleton_instance;
        clm_container_internal_release();
        if (!has_instance) {
            return state == CLM_CONTAINER_FRAME_STARTING
                       ? CLM_CONTAINER_STATUS_BUSY
                       : CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
        }
        if (!clm_container_internal_instance_valid(instance_id)) {
            return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
        }
        *out_instance_id = instance_id;
        return CLM_CONTAINER_STATUS_SUCCESS;
    }
    ++service->active_calls;
    const clm_container_constructor_fn constructor = service->constructor;
    const clm_id context_id = service->context_id;
    clm_container_internal_release();
    clm_id instance_id = 0;
    const clm_int construct_status = constructor(service_id, context_id, &instance_id);
    const bool valid_instance = construct_status == CLM_CONTAINER_STATUS_SUCCESS && clm_container_internal_instance_valid(instance_id);
    clm_container_internal_acquire();
    service = clm_container_internal_find(service_id);
    if (service != NULL && service->active_calls != 0) {
        --service->active_calls;
    }
    clm_container_internal_release();
    if (!valid_instance) {
        return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
    }
    *out_instance_id = instance_id;
    return CLM_CONTAINER_STATUS_SUCCESS;
}

clm_int clm_container_invoke(
    const clm_id* dependencies,
    const size_t dependency_count,
    const clm_container_invoke_fn function,
    const clm_id context_id) {
    if (function == NULL || (dependency_count != 0 && dependencies == NULL)) {
        return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
    }
    clm_container_internal_acquire();
    for (size_t held = 0; held < dependency_count; ++held) {
        clm_container_internal_service* service = clm_container_internal_find(dependencies[held]);
        if (service == NULL) {
            for (size_t rollback = 0; rollback < held; ++rollback) {
                service = clm_container_internal_find(dependencies[rollback]);
                if (service != NULL && service->active_calls != 0) {
                    --service->active_calls;
                }
            }
            clm_container_internal_release();
            return CLM_CONTAINER_STATUS_MISSING_DEPENDENCY;
        }
        if ((service->lifetime == CLM_CONTAINER_LIFETIME_SCOPED &&
                clm_container_scope_state != CLM_CONTAINER_FRAME_ACTIVE &&
                clm_container_scope_state != CLM_CONTAINER_FRAME_STARTING) ||
            (service->lifetime == CLM_CONTAINER_LIFETIME_INTERN &&
                clm_container_intern_state != CLM_CONTAINER_FRAME_ACTIVE &&
                clm_container_intern_state != CLM_CONTAINER_FRAME_STARTING)) {
            for (size_t rollback = 0; rollback < held; ++rollback) {
                service = clm_container_internal_find(dependencies[rollback]);
                if (service != NULL && service->active_calls != 0) {
                    --service->active_calls;
                }
            }
            clm_container_internal_release();
            return CLM_CONTAINER_STATUS_BUSY;
        }
        ++service->active_calls;
    }
    clm_container_internal_release();
    const clm_int result = function(context_id);
    clm_container_internal_acquire();
    for (size_t index = 0; index < dependency_count; ++index) {
        clm_container_internal_service* service = clm_container_internal_find(dependencies[index]);
        if (service != NULL && service->active_calls != 0) {
            --service->active_calls;
        }
    }
    clm_container_internal_release();
    return result;
}

clm_int clm_container_unregister(const clm_id service_id) {
    clm_container_internal_acquire();
    clm_container_internal_service** link = &clm_container_internal_services;
    while (*link != NULL && (*link)->id != service_id) {
        link = &(*link)->next;
    }
    clm_container_internal_service* service = *link;
    if (service == NULL) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_NOT_REGISTERED;
    }
    if (clm_container_internal_transitioning() || service->constructing ||
        service->active_calls != 0 ||
        (service->lifetime == CLM_CONTAINER_LIFETIME_SCOPED &&
            clm_container_scope_state != CLM_CONTAINER_FRAME_INACTIVE) ||
        (service->lifetime == CLM_CONTAINER_LIFETIME_INTERN &&
            clm_container_intern_state != CLM_CONTAINER_FRAME_INACTIVE)) {
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

static clm_container_internal_frame_state*
clm_container_internal_frame_state_for(const clm_int lifetime) {
    if (lifetime == CLM_CONTAINER_LIFETIME_SCOPED) {
        return &clm_container_scope_state;
    }
    if (lifetime == CLM_CONTAINER_LIFETIME_INTERN) {
        return &clm_container_intern_state;
    }
    return NULL;
}

static clm_int clm_container_internal_begin_frame(const clm_int lifetime) {
    clm_container_internal_frame_state* state = clm_container_internal_frame_state_for(lifetime);
    if (state == NULL) {
        return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
    }

    clm_container_internal_acquire();
    if (clm_container_internal_transitioning() ||
        *state != CLM_CONTAINER_FRAME_INACTIVE) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_BUSY;
    }
    size_t count = 0;
    for (const clm_container_internal_service* service = clm_container_internal_services; service != NULL; service = service->next) {
        if (service->lifetime == lifetime) {
            ++count;
        }
    }
    if (count > (size_t)-1 / sizeof(clm_container_internal_service*)) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_FAILURE;
    }
    clm_container_internal_service** services = count == 0
                                                    ? NULL
                                                    : (clm_container_internal_service**)calloc(count, sizeof(*services));
    if (count != 0 && services == NULL) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_FAILURE;
    }
    size_t index = 0;
    for (clm_container_internal_service* service = clm_container_internal_services; service != NULL; service = service->next) {
        if (service->lifetime == lifetime) {
            services[index++] = service;
        }
    }
    *state = CLM_CONTAINER_FRAME_STARTING;
    clm_container_internal_release();

    /* Dependencies must be registered before dependents; the service list is
     * newest-first, so constructing it in reverse preserves that order. */
    size_t constructed = 0;
    for (size_t remaining = count; remaining != 0; --remaining) {
        clm_container_internal_service* service = services[remaining - 1];
        clm_container_internal_acquire();
        service->constructing = true;
        const clm_container_constructor_fn constructor = service->constructor;
        const clm_id service_id = service->id;
        const clm_id context_id = service->context_id;
        clm_container_internal_release();

        clm_id instance_id = 0;
        const clm_int status = constructor(service_id, context_id, &instance_id);
        const bool valid_instance = status == CLM_CONTAINER_STATUS_SUCCESS && clm_container_internal_instance_valid(instance_id);

        clm_container_internal_acquire();
        service->constructing = false;
        if (valid_instance) {
            service->singleton_instance_id = instance_id;
            service->has_singleton_instance = true;
        }
        clm_container_internal_release();
        if (!valid_instance) {
            for (size_t rollback = count - constructed; rollback < count; ++rollback) {
                clm_container_internal_service* created = services[rollback];
                clm_container_internal_acquire();
                const bool has_instance = created->has_singleton_instance;
                const clm_id created_id = created->singleton_instance_id;
                created->has_singleton_instance = false;
                created->singleton_instance_id = 0;
                clm_container_internal_release();
                if (has_instance) {
                    (void)clm_bpa_delete(created_id);
                }
            }
            clm_container_internal_acquire();
            *state = CLM_CONTAINER_FRAME_INACTIVE;
            clm_container_internal_release();
            free(services);
            return CLM_CONTAINER_STATUS_CONSTRUCTOR_FAILURE;
        }
        ++constructed;
    }

    clm_container_internal_acquire();
    *state = CLM_CONTAINER_FRAME_ACTIVE;
    clm_container_internal_release();
    free(services);
    return CLM_CONTAINER_REGISTRATION_SUCCESS;
}

static clm_int clm_container_internal_end_frame(const clm_int lifetime) {
    clm_container_internal_frame_state* state = clm_container_internal_frame_state_for(lifetime);
    if (state == NULL) {
        return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
    }

    clm_container_internal_acquire();
    if (clm_container_internal_transitioning()) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_BUSY;
    }
    if (*state != CLM_CONTAINER_FRAME_ACTIVE) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_INVALID_ARGUMENT;
    }
    size_t count = 0;
    for (const clm_container_internal_service* service = clm_container_internal_services; service != NULL; service = service->next) {
        if (service->lifetime != lifetime) {
            continue;
        }
        if (service->constructing || service->active_calls != 0) {
            clm_container_internal_release();
            return CLM_CONTAINER_STATUS_BUSY;
        }
        ++count;
    }
    if (count > (size_t)-1 / sizeof(clm_container_internal_service*)) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_FAILURE;
    }
    clm_container_internal_service** services = count == 0
                                                    ? NULL
                                                    : (clm_container_internal_service**)calloc(
                                                        count, sizeof(*services));
    if (count != 0 && services == NULL) {
        clm_container_internal_release();
        return CLM_CONTAINER_STATUS_FAILURE;
    }
    size_t index = 0;
    for (clm_container_internal_service* service = clm_container_internal_services; service != NULL; service = service->next) {
        if (service->lifetime == lifetime) {
            services[index++] = service;
        }
    }
    *state = CLM_CONTAINER_FRAME_ENDING;
    clm_container_internal_release();

    clm_int result = CLM_CONTAINER_REGISTRATION_SUCCESS;
    for (size_t item = 0; item < count; ++item) {
        clm_container_internal_service* service = services[item];
        clm_container_internal_acquire();
        const bool has_instance = service->has_singleton_instance;
        const clm_id instance_id = service->singleton_instance_id;
        clm_container_internal_release();
        if (has_instance && clm_bpa_delete(instance_id) != CLM_BPA_SUCCESS) {
            result = CLM_CONTAINER_STATUS_FAILURE;
            continue;
        }
        clm_container_internal_acquire();
        service->has_singleton_instance = false;
        service->singleton_instance_id = 0;
        clm_container_internal_release();
    }

    clm_container_internal_acquire();
    bool instances_remain = false;
    for (const clm_container_internal_service* service = clm_container_internal_services; service != NULL; service = service->next)
        if (service->lifetime == lifetime && service->has_singleton_instance) {
            instances_remain = true;
            break;
        }
    *state = instances_remain
                 ? CLM_CONTAINER_FRAME_ACTIVE
                 : CLM_CONTAINER_FRAME_INACTIVE;
    clm_container_internal_release();
    free(services);
    return result;
}

clm_int clm_container_begin_scope(void) {
    return clm_container_internal_begin_frame(CLM_CONTAINER_LIFETIME_SCOPED);
}

clm_int clm_container_end_scope(void) {
    return clm_container_internal_end_frame(CLM_CONTAINER_LIFETIME_SCOPED);
}

clm_int clm_container_is_scope(void) {
    clm_container_internal_acquire();
    const bool active = clm_container_scope_state == CLM_CONTAINER_FRAME_ACTIVE;
    clm_container_internal_release();
    return active ? 1 : 0;
}

clm_int clm_container_begin_intern(void) {
    return clm_container_internal_begin_frame(CLM_CONTAINER_LIFETIME_INTERN);
}

clm_int clm_container_end_intern(void) {
    return clm_container_internal_end_frame(CLM_CONTAINER_LIFETIME_INTERN);
}

clm_int clm_container_is_intern(void) {
    clm_container_internal_acquire();
    const bool active = clm_container_intern_state == CLM_CONTAINER_FRAME_ACTIVE;
    clm_container_internal_release();
    return active ? 1 : 0;
}