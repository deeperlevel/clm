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

#include "dispatch.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "thread.h"

typedef struct clm_private_dispatch_handler {
    clm_id id;
    clm_id *dependencies;
    size_t dependency_count;
    clm_dispatch_handler_fn callback;
    void *user;
    struct clm_private_dispatch_handler *next;
} clm_private_dispatch_handler;

static once_flag clm_private_dispatch_once = ONCE_FLAG_INIT;
static mtx_t clm_private_dispatch_lock;
static clm_private_dispatch_handler *clm_private_dispatch_handlers;
static clm_dispatch_logger_fn clm_private_dispatch_loggers[4];
static void *clm_private_dispatch_logger_users[4];
static clm_dispatch_resolver_fn clm_private_dispatch_resolver;
static void *clm_private_dispatch_resolver_user;
static atomic_uint_fast64_t clm_private_dispatch_next_id = 1;

static void clm_private_dispatch_init_lock(void)
{
    (void)mtx_init(&clm_private_dispatch_lock, mtx_plain);
}

clm_dispatch_status clm_dispatch_init(void)
{
    call_once(&clm_private_dispatch_once, clm_private_dispatch_init_lock);
    return CLM_DISPATCH_SUCCESS;
}

void clm_dispatch_destroy(void)
{
    clm_dispatch_init();
    mtx_lock(&clm_private_dispatch_lock);
    clm_private_dispatch_handler *handler = clm_private_dispatch_handlers;
    clm_private_dispatch_handlers = NULL;
    memset(clm_private_dispatch_loggers, 0,
        sizeof clm_private_dispatch_loggers);
    memset(clm_private_dispatch_logger_users, 0,
        sizeof clm_private_dispatch_logger_users);
    clm_private_dispatch_resolver = NULL;
    clm_private_dispatch_resolver_user = NULL;
    mtx_unlock(&clm_private_dispatch_lock);

    while (handler != NULL) {
        clm_private_dispatch_handler *next = handler->next;
        free(handler->dependencies);
        free(handler);
        handler = next;
    }
}

void clm_dispatch_set_resolver(clm_dispatch_resolver_fn resolver, void *user)
{
    clm_dispatch_init();
    mtx_lock(&clm_private_dispatch_lock);
    clm_private_dispatch_resolver = resolver;
    clm_private_dispatch_resolver_user = user;
    mtx_unlock(&clm_private_dispatch_lock);
}

clm_dispatch_status clm_dispatch_register_handler(
    clm_id message_id, const clm_id *dependencies, size_t dependency_count,
    clm_dispatch_handler_fn callback, void *user)
{
    if (callback == NULL || (dependency_count != 0 && dependencies == NULL) ||
        dependency_count > SIZE_MAX / sizeof(*dependencies)) {
        return CLM_DISPATCH_INVALID;
    }
    clm_dispatch_init();

    clm_private_dispatch_handler *handler =
        (clm_private_dispatch_handler *)calloc(1, sizeof(*handler));
    if (handler == NULL) {
        return CLM_DISPATCH_NO_MEMORY;
    }
    if (dependency_count != 0) {
        handler->dependencies = (clm_id *)malloc(
            dependency_count * sizeof(*handler->dependencies));
        if (handler->dependencies == NULL) {
            free(handler);
            return CLM_DISPATCH_NO_MEMORY;
        }
        memcpy(handler->dependencies, dependencies,
            dependency_count * sizeof(*handler->dependencies));
    }
    handler->id = message_id;
    handler->dependency_count = dependency_count;
    handler->callback = callback;
    handler->user = user;

    mtx_lock(&clm_private_dispatch_lock);
    for (clm_private_dispatch_handler *current =
             clm_private_dispatch_handlers;
         current != NULL; current = current->next) {
        if (current->id == message_id) {
            mtx_unlock(&clm_private_dispatch_lock);
            free(handler->dependencies);
            free(handler);
            return CLM_DISPATCH_DUPLICATE;
        }
    }
    handler->next = clm_private_dispatch_handlers;
    clm_private_dispatch_handlers = handler;
    mtx_unlock(&clm_private_dispatch_lock);
    return CLM_DISPATCH_SUCCESS;
}

clm_dispatch_status clm_dispatch_unregister_handler(clm_id message_id)
{
    clm_dispatch_init();
    mtx_lock(&clm_private_dispatch_lock);
    clm_private_dispatch_handler **link = &clm_private_dispatch_handlers;
    while (*link != NULL && (*link)->id != message_id) {
        link = &(*link)->next;
    }
    clm_private_dispatch_handler *handler = *link;
    if (handler != NULL) {
        *link = handler->next;
    }
    mtx_unlock(&clm_private_dispatch_lock);

    if (handler == NULL) {
        return CLM_DISPATCH_NOT_FOUND;
    }
    free(handler->dependencies);
    free(handler);
    return CLM_DISPATCH_SUCCESS;
}

clm_dispatch_status clm_dispatch_register_logger(
    clm_dispatch_kind kind, clm_dispatch_logger_fn callback, void *user)
{
    if (kind < CLM_DISPATCH_COMMAND || kind > CLM_DISPATCH_EXCEPTION ||
        callback == NULL) {
        return CLM_DISPATCH_INVALID;
    }
    clm_dispatch_init();
    size_t index = (size_t)kind - 1;
    mtx_lock(&clm_private_dispatch_lock);
    if (clm_private_dispatch_loggers[index] != NULL) {
        mtx_unlock(&clm_private_dispatch_lock);
        return CLM_DISPATCH_DUPLICATE;
    }
    clm_private_dispatch_loggers[index] = callback;
    clm_private_dispatch_logger_users[index] = user;
    mtx_unlock(&clm_private_dispatch_lock);
    return CLM_DISPATCH_SUCCESS;
}

clm_dispatch_status clm_dispatch_unregister_logger(clm_dispatch_kind kind)
{
    if (kind < CLM_DISPATCH_COMMAND || kind > CLM_DISPATCH_EXCEPTION) {
        return CLM_DISPATCH_INVALID;
    }
    clm_dispatch_init();
    size_t index = (size_t)kind - 1;
    mtx_lock(&clm_private_dispatch_lock);
    if (clm_private_dispatch_loggers[index] == NULL) {
        mtx_unlock(&clm_private_dispatch_lock);
        return CLM_DISPATCH_NOT_FOUND;
    }
    clm_private_dispatch_loggers[index] = NULL;
    clm_private_dispatch_logger_users[index] = NULL;
    mtx_unlock(&clm_private_dispatch_lock);
    return CLM_DISPATCH_SUCCESS;
}

clm_dispatch_chain clm_dispatch_start_new(void)
{
    uint64_t id = atomic_fetch_add(&clm_private_dispatch_next_id, 1);
    clm_dispatch_chain chain = {id, id, 0};
    return chain;
}

clm_dispatch_chain clm_dispatch_next(clm_dispatch_chain parent)
{
    uint64_t id = atomic_fetch_add(&clm_private_dispatch_next_id, 1);
    clm_dispatch_chain chain = {id, parent.correlation_id, parent.id};
    return chain;
}

clm_dispatch_status clm_dispatch_invoke(
    clm_id message_id, const void *message, void *result)
{
    clm_dispatch_init();
    mtx_lock(&clm_private_dispatch_lock);
    clm_private_dispatch_handler *handler = clm_private_dispatch_handlers;
    while (handler != NULL && handler->id != message_id) {
        handler = handler->next;
    }
    if (handler == NULL) {
        mtx_unlock(&clm_private_dispatch_lock);
        return CLM_DISPATCH_NOT_FOUND;
    }

    size_t dependency_count = handler->dependency_count;
    clm_id *dependency_ids = NULL;
    if (dependency_count != 0) {
        dependency_ids = (clm_id *)malloc(
            dependency_count * sizeof(*dependency_ids));
        if (dependency_ids == NULL) {
            mtx_unlock(&clm_private_dispatch_lock);
            return CLM_DISPATCH_NO_MEMORY;
        }
        memcpy(dependency_ids, handler->dependencies,
            dependency_count * sizeof(*dependency_ids));
    }
    clm_dispatch_handler_fn callback = handler->callback;
    void *handler_user = handler->user;
    clm_dispatch_resolver_fn resolver = clm_private_dispatch_resolver;
    void *resolver_user = clm_private_dispatch_resolver_user;
    mtx_unlock(&clm_private_dispatch_lock);

    void **dependencies = NULL;
    if (dependency_count != 0) {
        dependencies = (void **)calloc(dependency_count, sizeof(*dependencies));
        if (dependencies == NULL) {
            free(dependency_ids);
            return CLM_DISPATCH_NO_MEMORY;
        }
    }
    for (size_t index = 0; index < dependency_count; ++index) {
        if (resolver == NULL ||
            resolver(dependency_ids[index], &dependencies[index],
                resolver_user) != CLM_DISPATCH_SUCCESS) {
            free(dependency_ids);
            free(dependencies);
            return CLM_DISPATCH_RESOLVE_FAILED;
        }
    }

    clm_dispatch_status status = callback(
        message, result, dependencies, dependency_count, handler_user);
    free(dependency_ids);
    free(dependencies);
    return status;
}

static clm_dispatch_status clm_private_dispatch_dispatch(
    clm_dispatch_kind kind, clm_id message_id, const void *message,
    void *result, bool log, const clm_dispatch_chain *parent)
{
    clm_dispatch_logger_fn logger = NULL;
    void *logger_user = NULL;
    if (log) {
        clm_dispatch_init();
        mtx_lock(&clm_private_dispatch_lock);
        size_t index = (size_t)kind - 1;
        logger = clm_private_dispatch_loggers[index];
        logger_user = clm_private_dispatch_logger_users[index];
        mtx_unlock(&clm_private_dispatch_lock);
    }
    if (logger != NULL) {
        clm_dispatch_chain chain = parent == NULL
            ? clm_dispatch_start_new()
            : clm_dispatch_next(*parent);
        clm_dispatch_envelope envelope = {chain, message_id, message, kind};
        logger(&envelope, logger_user);
    }
    return clm_dispatch_invoke(message_id, message, result);
}

clm_dispatch_status clm_dispatch_execute(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent)
{
    return clm_private_dispatch_dispatch(CLM_DISPATCH_COMMAND, message_id,
        message, result, log, parent);
}

clm_dispatch_status clm_dispatch_emit(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent)
{
    return clm_private_dispatch_dispatch(CLM_DISPATCH_EVENT, message_id,
        message, result, log, parent);
}

clm_dispatch_status clm_dispatch_query(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent)
{
    return clm_private_dispatch_dispatch(CLM_DISPATCH_QUERY, message_id,
        message, result, log, parent);
}

clm_dispatch_status clm_dispatch_caught(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent)
{
    return clm_private_dispatch_dispatch(CLM_DISPATCH_EXCEPTION, message_id,
        message, result, log, parent);
}
