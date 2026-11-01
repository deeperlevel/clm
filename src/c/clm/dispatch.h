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
#include <stddef.h>
#include <stdint.h>

typedef enum clm_dispatch_status {
    CLM_DISPATCH_SUCCESS = 0,
    CLM_DISPATCH_INVALID = -1,
    CLM_DISPATCH_DUPLICATE = -2,
    CLM_DISPATCH_NOT_FOUND = -3,
    CLM_DISPATCH_NO_MEMORY = -4,
    CLM_DISPATCH_RESOLVE_FAILED = -5
} clm_dispatch_status;

typedef enum clm_dispatch_kind {
    CLM_DISPATCH_COMMAND = 1,
    CLM_DISPATCH_EVENT = 2,
    CLM_DISPATCH_QUERY = 3,
    CLM_DISPATCH_EXCEPTION = 4
} clm_dispatch_kind;

typedef struct clm_dispatch_chain {
    uint64_t id;
    uint64_t correlation_id;
    uint64_t causation_id;
} clm_dispatch_chain;

typedef struct clm_dispatch_envelope {
    clm_dispatch_chain chain;
    clm_id message_id;
    const void *message;
    clm_dispatch_kind kind;
} clm_dispatch_envelope;

/** Resolves a service ID to a borrowed pointer; ownership remains with caller. */
typedef clm_dispatch_status (*clm_dispatch_resolver_fn)(
    clm_id service_id, void **out_service, void *user);

/** Handles a message with resolved dependency pointers in registration order. */
typedef clm_dispatch_status (*clm_dispatch_handler_fn)(
    const void *message, void *result, void *const dependencies[],
    size_t dependency_count, void *user);

/** Observes a typed message envelope before its handler is invoked. */
typedef void (*clm_dispatch_logger_fn)(
    const clm_dispatch_envelope *envelope, void *user);

/** Initializes the dispatcher's synchronization state. 
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_init(void);

/** Removes all handlers and loggers; call only when no operation is active. */
void clm_dispatch_destroy(void);

/** Installs a dependency resolver and its user context. 
 * @param resolver Input parameter described above.
 * @param user Writable output or in/out pointer.
 */
void clm_dispatch_set_resolver(clm_dispatch_resolver_fn resolver, void *user);

/** Registers one message handler and copies its ordered dependency ID list. 
 * @param message_id CLM ID identifying the message.
 * @param dependencies Input data or buffer; not modified.
 * @param dependency_count Input parameter described above.
 * @param callback Callback to invoke.
 * @param user Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_register_handler(
    clm_id message_id, const clm_id *dependencies, size_t dependency_count,
    clm_dispatch_handler_fn callback, void *user);

/** Removes a handler by message ID. 
 * @param message_id CLM ID identifying the message.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_unregister_handler(clm_id message_id);

/** Registers the logger for one command, event, query, or exception kind. 
 * @param kind Input parameter described above.
 * @param callback Callback to invoke.
 * @param user Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_register_logger(
    clm_dispatch_kind kind, clm_dispatch_logger_fn callback, void *user);

/** Removes the logger registered for a message kind. 
 * @param kind Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_unregister_logger(clm_dispatch_kind kind);

/** Starts a new correlation chain with a fresh message ID. 
 * @return Computed result described in the summary above.
 */
clm_dispatch_chain clm_dispatch_start_new(void);

/** Creates a child chain entry linked to its parent's correlation and ID. 
 * @param parent Input parameter described above.
 * @return Computed result described in the summary above.
 */
clm_dispatch_chain clm_dispatch_next(clm_dispatch_chain parent);

/** Resolves dependencies and invokes a handler without logging. 
 * @param message_id CLM ID identifying the message.
 * @param message Input data or buffer; not modified.
 * @param result Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_invoke(
    clm_id message_id, const void *message, void *result);

/** Executes a command, optionally logging it and linking it to a parent chain. 
 * @param message_id CLM ID identifying the message.
 * @param message Input data or buffer; not modified.
 * @param result Writable output or in/out pointer.
 * @param log Input parameter described above.
 * @param parent Input data or buffer; not modified.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_execute(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent);

/** Emits an event through its registered logger and handler. 
 * @param message_id CLM ID identifying the message.
 * @param message Input data or buffer; not modified.
 * @param result Writable output or in/out pointer.
 * @param log Input parameter described above.
 * @param parent Input data or buffer; not modified.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_emit(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent);

/** Runs a query through its registered logger and handler. 
 * @param message_id CLM ID identifying the message.
 * @param message Input data or buffer; not modified.
 * @param result Writable output or in/out pointer.
 * @param log Input parameter described above.
 * @param parent Input data or buffer; not modified.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_query(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent);

/** Routes a caught exception through its registered logger and handler. 
 * @param message_id CLM ID identifying the message.
 * @param message Input data or buffer; not modified.
 * @param result Writable output or in/out pointer.
 * @param log Input parameter described above.
 * @param parent Input data or buffer; not modified.
 * @return Success or error status, as described in the summary above.
 */
clm_dispatch_status clm_dispatch_caught(
    clm_id message_id, const void *message, void *result, bool log,
    const clm_dispatch_chain *parent);

/* Define CLM_DISPATCH_IMPLEMENTATION in one translation unit, or define
 * CLM_IMPLEMENTATION once for all single-header modules.
 */
