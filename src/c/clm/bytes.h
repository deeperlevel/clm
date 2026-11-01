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

/* Binary-safe byte views and growable byte buffers.
 *
 * A view borrows memory; a buffer owns it. A buffer holds the window
 * [offset, offset + length) of its allocation, so data can be appended at the
 * tail (for example from clm_net_receive) and consumed from the front (for
 * example after clm_net_send) without shifting bytes on every call.
 * Pointers and views taken from a buffer are invalidated by any call that
 * changes it. A zero-initialized clm_bytes_buffer is a valid empty buffer.
 * No function treats NUL specially unless it says so.
 */

#include "base.h"

#include <stdbool.h>
#include <stddef.h>

#define CLM_BYTES_SUCCESS ((clm_int)0)
/** A search found no match; not an error. */
#define CLM_BYTES_NOT_FOUND ((clm_int)1)
#define CLM_BYTES_FAILURE ((clm_int)-1)
#define CLM_BYTES_INVALID_ARGUMENT ((clm_int)-2)
#define CLM_BYTES_NO_MEMORY ((clm_int)-3)
/** The operation would grow a buffer past its limit; the buffer is unchanged. */
#define CLM_BYTES_LIMIT_EXCEEDED ((clm_int)-4)
#define CLM_BYTES_OUT_OF_RANGE ((clm_int)-5)
#define CLM_BYTES_BUFFER_TOO_SMALL ((clm_int)-6)

/** Borrowed read-only bytes; data may be NULL only when length is 0. */
typedef struct {
    const unsigned char* data;
    size_t length;
} clm_bytes_view;

/** Owned growable bytes. Treat the fields as private; use the functions. */
typedef struct {
    unsigned char* storage;
    size_t offset;
    size_t length;
    size_t capacity;
    size_t limit;
} clm_bytes_buffer;

/** Initializes an empty buffer, optionally reserving capacity.
 * @param initial_capacity Bytes to allocate up front; 0 allocates nothing.
 * @return SUCCESS, INVALID_ARGUMENT, or NO_MEMORY.
 */
clm_int clm_bytes_buffer_init(clm_bytes_buffer* buffer, size_t initial_capacity);

/** Frees the storage and resets the buffer to empty. Does not erase contents;
 * call clm_bytes_buffer_wipe first for secrets. Safe on a zeroed buffer. */
void clm_bytes_buffer_dispose(clm_bytes_buffer* buffer);

/** Sets length to zero and keeps the allocation. */
clm_int clm_bytes_buffer_clear(clm_bytes_buffer* buffer);

/** Overwrites the whole allocation with zeros, then clears the buffer.
 * Copies made by earlier growth are not erased. */
clm_int clm_bytes_buffer_wipe(clm_bytes_buffer* buffer);

/** Caps the number of bytes the buffer may hold; 0 removes the cap.
 * Use it to bound downloads. Appends and prepares past the cap fail with
 * LIMIT_EXCEEDED. Fails with LIMIT_EXCEEDED if it already holds more. */
clm_int clm_bytes_buffer_set_limit(clm_bytes_buffer* buffer, size_t limit);

/** Returns the held bytes as a view; empty for NULL. */
clm_bytes_view clm_bytes_buffer_view(const clm_bytes_buffer* buffer);

/** Returns the number of held bytes; 0 for NULL. */
size_t clm_bytes_buffer_length(const clm_bytes_buffer* buffer);

/** Ensures room for additional bytes after the current data. */
clm_int clm_bytes_buffer_reserve(clm_bytes_buffer* buffer, size_t additional);

/** Exposes writable tail space for zero-copy reads such as clm_net_receive.
 * Ensures at least min_free bytes, then reports the whole free tail, clipped
 * to the limit. Write up to *out_free bytes there, then call commit.
 * @param out_tail Receives the write pointer (NULL when *out_free is 0).
 * @param out_free Receives the writable byte count.
 * @return SUCCESS, LIMIT_EXCEEDED, NO_MEMORY, or INVALID_ARGUMENT.
 */
clm_int clm_bytes_buffer_prepare(
    clm_bytes_buffer* buffer, size_t min_free,
    unsigned char** out_tail, size_t* out_free);

/** Adds bytes written into the prepared tail to the buffer.
 * @return SUCCESS, or OUT_OF_RANGE if written exceeds the prepared space.
 */
clm_int clm_bytes_buffer_commit(clm_bytes_buffer* buffer, size_t written);

/** Drops count bytes from the front, for example after a partial send.
 * @return SUCCESS, or OUT_OF_RANGE if count exceeds the length.
 */
clm_int clm_bytes_buffer_consume(clm_bytes_buffer* buffer, size_t count);

/** Copies count bytes from the front into dest and drops them.
 * @return SUCCESS, OUT_OF_RANGE, or INVALID_ARGUMENT.
 */
clm_int clm_bytes_buffer_read(clm_bytes_buffer* buffer, void* dest, size_t count);

/** Appends bytes; data may overlap the buffer's own contents.
 * @return SUCCESS, LIMIT_EXCEEDED, NO_MEMORY, or INVALID_ARGUMENT.
 */
clm_int clm_bytes_buffer_append(
    clm_bytes_buffer* buffer, const void* data, size_t length);
clm_int clm_bytes_buffer_append_view(
    clm_bytes_buffer* buffer, clm_bytes_view view);
clm_int clm_bytes_buffer_append_byte(clm_bytes_buffer* buffer, unsigned char value);
clm_int clm_bytes_buffer_append_fill(
    clm_bytes_buffer* buffer, unsigned char value, size_t count);
/** Appends the characters of a NUL-terminated string, without the NUL. */
clm_int clm_bytes_buffer_append_string(clm_bytes_buffer* buffer, const char* text);

/** Inserts bytes before index; index == length appends.
 * @return SUCCESS, OUT_OF_RANGE, LIMIT_EXCEEDED, NO_MEMORY, or INVALID_ARGUMENT.
 */
clm_int clm_bytes_buffer_insert(
    clm_bytes_buffer* buffer, size_t index, const void* data, size_t length);

/** Removes count bytes starting at index.
 * @return SUCCESS, or OUT_OF_RANGE.
 */
clm_int clm_bytes_buffer_remove(clm_bytes_buffer* buffer, size_t index, size_t count);

/** Sets the length, filling new bytes with fill. Shrinking keeps the old
 * bytes in the allocation until overwritten or wiped. */
clm_int clm_bytes_buffer_resize(
    clm_bytes_buffer* buffer, size_t new_length, unsigned char fill);

/** Appends the whole contents of a file.
 * The size is checked against the limit before any data is read, so a capped
 * buffer never reads an oversized file.
 * @param path NUL-terminated UTF-8 path.
 * @return SUCCESS, FAILURE for file errors, LIMIT_EXCEEDED, or NO_MEMORY.
 */
clm_int clm_bytes_buffer_load_file(clm_bytes_buffer* buffer, const char* path);

/** Returns a view of a NUL-terminated string without its NUL; empty for NULL. */
clm_bytes_view clm_bytes_view_from_string(const char* text);

/** Selects length bytes starting at offset.
 * @return SUCCESS, or OUT_OF_RANGE (out is unchanged).
 */
clm_int clm_bytes_view_slice(
    clm_bytes_view view, size_t offset, size_t length, clm_bytes_view* out);

/** Copies a view into dest.
 * @param copied Receives the bytes copied, or the needed size when too small; may be NULL.
 * @return SUCCESS, BUFFER_TOO_SMALL (dest unchanged), or INVALID_ARGUMENT.
 */
clm_int clm_bytes_view_copy(
    clm_bytes_view view, void* dest, size_t capacity, size_t* copied);

bool clm_bytes_view_equals(clm_bytes_view a, clm_bytes_view b);

/** Lexicographic comparison of unsigned bytes; negative, zero, or positive. */
int clm_bytes_view_compare(clm_bytes_view a, clm_bytes_view b);

/** Equality whose running time depends only on the lengths, for secrets.
 * Views of different length are unequal immediately. */
bool clm_bytes_view_equals_constant_time(clm_bytes_view a, clm_bytes_view b);

bool clm_bytes_view_starts_with(clm_bytes_view view, clm_bytes_view prefix);
bool clm_bytes_view_ends_with(clm_bytes_view view, clm_bytes_view suffix);

/** Finds the first occurrence of needle; an empty needle matches at 0.
 * @param out_index Receives the offset on SUCCESS; may be NULL.
 * @return SUCCESS or NOT_FOUND.
 */
clm_int clm_bytes_view_find(
    clm_bytes_view view, clm_bytes_view needle, size_t* out_index);

/** Finds the first occurrence of one byte value.
 * @return SUCCESS or NOT_FOUND.
 */
clm_int clm_bytes_view_find_byte(
    clm_bytes_view view, unsigned char value, size_t* out_index);

/** Writes the view to a file, creating or truncating it.
 * @return SUCCESS or FAILURE.
 */
clm_int clm_bytes_view_save_file(clm_bytes_view view, const char* path);

/** Appends the view to a file, creating it if absent.
 * @return SUCCESS or FAILURE.
 */
clm_int clm_bytes_view_append_to_file(clm_bytes_view view, const char* path);
