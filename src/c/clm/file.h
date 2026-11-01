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
#include "path.h"
#include "bpa.h"
#include "string.h"
#include "gen.h"
#include "stringtify.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <utime.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <linux/fs.h>
#endif
#endif

#define CLM_FILE_OPERATION_SUCCESS 0
#define CLM_FILE_OPERATION_FAILURE -1
#define CLM_FILE_OPERATION_UNSUPPORTED -2

/** @file
 * File API. Define CLM_FILE_IMPLEMENTATION in exactly one C translation unit
 * to provide the shared ID-to-native-resource registry.
 * Path IDs reference UTF-8 BPA strings; open-file IDs identify independent native
 * resources. _as accepts raw UTF-8 paths, _to exports caller-owned data, and
 * _from consumes caller-owned data. Native handle representation remains private.
 * Existing BPA binary blocks have fixed capacity; text outputs are existing CLM
 * strings or string arrays. Whole-file outputs are prepared before publication.
 * Size/position queries use 64-bit values; callers synchronize registry access.
 * Status functions return SUCCESS/FAILURE/UNSUPPORTED; exists and is_open are
 * direct predicates returning 1, 0, or -1. Streaming transfers expose partial
 * progress and writes are not transactional. Closed IDs remain retired until
 * clm_file_shutdown; shutdown attempts every close before releasing registry state.
 * Text input/output in memory is UTF-8; encoding selects the file representation.
 * BOM variants require/emit the matching BOM; append never inserts a BOM into
 * existing content. UTF16 compatibility names mean explicit little-endian UTF-16.
 * Text conversion rejects malformed input and embedded NUL. Line helpers split
 * CRLF, LF, and CR, omit delimiters, and do not invent a final empty line; append
 * line helpers append exactly one LF, encoded for the selected file encoding.
 * Whole-file calls open and close internally; they do not register open-file IDs.
 * Native opens reject final-component links/reparse points rather than follow
 * them implicitly. Windows creation inherits parent ACLs; POSIX uses mode 0600.
 * Registry teardown is explicit; it is not automatically tied to BPA shutdown.
 */

#define CLM_FILE_TEXT_ENCODING_ASCII 0
#define CLM_FILE_TEXT_ENCODING_UTF8 1
#define CLM_FILE_TEXT_ENCODING_UTF8_BOM 2
#define CLM_FILE_TEXT_ENCODING_UTF16 3
#define CLM_FILE_TEXT_ENCODING_UTF16_BOM 4
#define CLM_FILE_TEXT_ENCODING_UTF16_LE CLM_FILE_TEXT_ENCODING_UTF16
#define CLM_FILE_TEXT_ENCODING_UTF16_LE_BOM CLM_FILE_TEXT_ENCODING_UTF16_BOM
#define CLM_FILE_TEXT_ENCODING_UTF16_BE 5
#define CLM_FILE_TEXT_ENCODING_UTF16_BE_BOM 6

#define CLM_FILE_OPEN_MODE_READ 1
#define CLM_FILE_OPEN_MODE_WRITE 2
#define CLM_FILE_OPEN_MODE_APPEND 4
#define CLM_FILE_OPEN_MODE_CREATE 8
#define CLM_FILE_OPEN_MODE_TRUNCATE 16
#define CLM_FILE_OPEN_MODE_EXCLUSIVE 32

#define CLM_FILE_SEEK_BEGIN 0
#define CLM_FILE_SEEK_CURRENT 1
#define CLM_FILE_SEEK_END 2

#define CLM_FILE_ATTRIBUTE_READ_ONLY     (UINT64_C(1) << 0)
#define CLM_FILE_ATTRIBUTE_HIDDEN        (UINT64_C(1) << 1)
#define CLM_FILE_ATTRIBUTE_SYSTEM        (UINT64_C(1) << 2)
#define CLM_FILE_ATTRIBUTE_ARCHIVE       (UINT64_C(1) << 3)
#define CLM_FILE_ATTRIBUTE_TEMPORARY     (UINT64_C(1) << 4)
#define CLM_FILE_ATTRIBUTE_OFFLINE       (UINT64_C(1) << 5)
#define CLM_FILE_ATTRIBUTE_ENCRYPTED     (UINT64_C(1) << 6)
#define CLM_FILE_ATTRIBUTE_COMPRESSED    (UINT64_C(1) << 7)
#define CLM_FILE_ATTRIBUTE_REPARSE_POINT (UINT64_C(1) << 8)
#define CLM_FILE_ATTRIBUTE_SYMBOLIC_LINK (UINT64_C(1) << 9)

#define CLM_FILE_TIME_CREATION 1u
#define CLM_FILE_TIME_MODIFICATION 2u
#define CLM_FILE_TIME_ACCESS 4u

/** Metadata for the selected file entry or link target.
 * Masks distinguish unsupported timestamps/attributes from zero or false values.
 * size is logical bytes, not allocated disk usage; timestamps have time_t precision.
 */
typedef struct {
    clm_path_info path;
    clm_ulong size;
    clm_ulong attributes;
    clm_ulong supported_attributes;
    time_t creation_time;
    time_t modification_time;
    time_t access_time;
    clm_uint supported_times;
} clm_file_info;

/** Opaque native file state; callers operate on open-file IDs, not this layout. */
struct clm_file;

#if defined(_WIN32)
typedef HANDLE clm_file_internal_handle;
#define CLM_FILE_INTERNAL_INVALID INVALID_HANDLE_VALUE
#else
typedef int clm_file_internal_handle;
#define CLM_FILE_INTERNAL_INVALID (-1)
#endif

typedef struct clm_file_internal_node {
    clm_id id;
    clm_int mode;
    bool eof;
    clm_file_internal_handle handle;
    struct clm_file_internal_node* next;
} clm_file_internal_node;

extern clm_file_internal_node* clm_file_internal_registry;

int clm_file_internal_valid_path(const char* path);

clm_file_internal_node* clm_file_internal_find(clm_id id);

int clm_file_internal_close(clm_file_internal_handle handle);

clm_file_internal_handle clm_file_internal_open(const char* path, int mode);

int clm_file_internal_size(clm_file_internal_handle handle, clm_ulong* size);

int clm_file_internal_seek(clm_file_internal_handle handle, clm_long offset, int origin, clm_ulong* position);

int clm_file_internal_transfer(clm_file_internal_handle handle, void* buffer, size_t size,
                                             int writing, size_t* transferred);

int clm_file_internal_truncate(clm_file_internal_handle handle, clm_ulong size);

clm_bpa_block_node* clm_file_internal_binary(clm_id id, size_t size);

unsigned char* clm_file_internal_read_all(const char* path, size_t* size);

int clm_file_internal_write_all(const char* path, const void* bytes, size_t size, int append);

int clm_file_internal_string(clm_id id, const char* text, size_t size);

int clm_file_internal_array(clm_id id, const char* packed, size_t bytes, clm_ulong count);

time_t clm_file_internal_time_from_native(
#if defined(_WIN32)
    FILETIME value
#else
    time_t value
#endif
);

int clm_file_internal_info(const char* path, bool follow, clm_file_info* out);

int clm_file_internal_attribute(const char* path, clm_ulong attribute, bool* out);

int clm_file_internal_set_attributes(const char* path, clm_ulong attributes, clm_ulong mask);

int clm_file_internal_set_times(const char* path, time_t creation, time_t modification,
                                             time_t access, clm_uint mask);

char* clm_file_internal_decode(const unsigned char* bytes, size_t size, int encoding, size_t* length);

unsigned char* clm_file_internal_encode(const char* text, size_t length, int encoding, size_t* size);

int clm_file_internal_text_write(const char* path, int encoding, const char* text, size_t size, int append, int line);

char* clm_file_internal_lines(const char* text, size_t length, size_t* bytes, clm_ulong* count);

int clm_file_internal_unlink(const char* path);

char* clm_file_internal_temp_name(const char* base, const char* suffix);

char* clm_file_internal_parent(const char* path);

int clm_file_internal_register(clm_id id, const char* path, int mode);

int clm_file_internal_binary_read(const char* path, clm_id destination, void* buffer,
                                               size_t capacity, size_t* size, bool managed);

int clm_file_internal_text_read(const char* path, int encoding, clm_id destination,
    char* buffer, size_t capacity, size_t* size, clm_ulong* count, int kind);

int clm_file_internal_content_write(const char* path, clm_id content, size_t size, int encoding, int append, int line);

int clm_file_internal_predicate(const char* path, int directory);

int clm_file_internal_time(const char* path, time_t* out, unsigned field);

int clm_file_internal_copy_file(const char* source, const char* destination, bool overwrite);

int clm_file_internal_move(const char* source, const char* destination, bool replace);

int clm_file_internal_positioned(clm_id id, clm_ulong offset, void* content, size_t size,
                                               size_t* transferred, bool writing);

int clm_file_internal_temporary(clm_id id, const char* base, clm_id destination,
                                              char* buffer, size_t capacity, size_t* required, bool managed);

int clm_file_internal_atomic(const char* path, const void* content, size_t size);

/** Test whether a path resolves to an existing regular file, following links.
 * @param path ID of the UTF-8 file path string, not an open-file ID.
 * @return 1 for a file, 0 for absent/non-file, -1 for query errors.
 */
clm_int clm_file_exists(clm_id path);

/** Test regular-file existence using a raw UTF-8 path.
 * @param path NUL-terminated path; a directory or dangling link is not an existing file.
 * @return 1 for a file, 0 for absent/non-file, -1 for query errors.
 */
clm_int clm_file_exists_as(const char* path);

/** Read whole-file binary bytes into an existing fixed-capacity BPA block.
 * @param path ID of the source file path string.
 * @param[out] content ID of a writable binary block; insufficient capacity is an error.
 * @param[out] size Non-NULL number of bytes read, assigned only on complete success.
 * @return Operation status; no text decoding or implicit block resizing.
 */
clm_int clm_file_read_all(clm_id path, clm_id content, size_t* size);

/** Read an entire binary file using a raw UTF-8 path and BPA destination.
 * @param path NUL-terminated source file path.
 * @param[out] content ID of an existing writable binary block; output is not text.
 * @param[out] size Non-NULL complete byte count; outputs are unchanged on failure.
 * @return Operation status; internally opened resources must be closed.
 */
clm_int clm_file_read_all_as(const char* path, clm_id content, size_t* size);

/** Read whole-file binary bytes into caller-owned memory without truncation.
 * @param path NUL-terminated UTF-8 source path.
 * @param[out] content Writable binary buffer; NULL with zero capacity measures only.
 * @param capacity Available buffer bytes; binary data has no added terminator.
 * @param[out] size Mandatory actual byte count, or required capacity on insufficient capacity/size query.
 * @return Operation status; an insufficient buffer must remain unchanged.
 * @note Files may change between size queries and reads; no filesystem snapshot is promised.
 */
clm_int clm_file_read_all_to(const char* path, void* content, size_t capacity, size_t* size);

/** Decode a complete text file into an existing CLM UTF-8 string.
 * @param path ID of the source file path string.
 * @param encoding CLM_FILE_TEXT_ENCODING_* source representation and BOM policy.
 * @param[out] content ID of an existing string destination, not a binary block.
 * @param[out] size Non-NULL decoded UTF-8 byte length excluding NUL, not encoded file size.
 * @return Operation status; malformed input and embedded NUL are errors.
 */
clm_int clm_file_read_all_text(clm_id path, clm_int encoding, clm_id content, size_t* size);

/** Decode whole-file text using a raw UTF-8 source path.
 * @param path NUL-terminated source path.
 * @param encoding Declared file encoding; BOM variants require the matching BOM.
 * @param[out] content ID of an existing CLM string receiving UTF-8 text without a BOM.
 * @param[out] size Non-NULL decoded UTF-8 byte count excluding NUL.
 * @return Operation status; publish outputs only after successful decoding.
 */
clm_int clm_file_read_all_text_as(const char* path, clm_int encoding, clm_id content, size_t* size);

/** Decode a text file into caller-owned NUL-terminated UTF-8 memory.
 * @param path NUL-terminated UTF-8 source path.
 * @param encoding Source-file encoding; the output buffer is always UTF-8.
 * @param[out] content Writable text buffer; NULL with zero capacity measures only.
 * @param capacity Byte capacity including room for the UTF-8 NUL terminator.
 * @param[out] required_size Mandatory required byte capacity including NUL, also on insufficient capacity.
 * @return Operation status; no partial characters or truncated output.
 */
clm_int clm_file_read_all_text_to(const char* path, clm_int encoding, char* content, size_t capacity, size_t* required_size);

/** Decode text lines into an existing CLM string array, excluding line delimiters.
 * @param path ID of the source file path string.
 * @param encoding Source-file encoding and BOM policy.
 * @param[out] content ID of an existing output string array, replaced only on success.
 * @param[out] line_count Non-NULL number of decoded lines; an empty file has zero lines.
 * @return Operation status; recognize CRLF, LF, and CR without a phantom final line.
 */
clm_int clm_file_read_all_text_lines(clm_id path, clm_int encoding, clm_id content, clm_ulong* line_count);

/** Read decoded lines using a raw UTF-8 source path.
 * @param path NUL-terminated source file path.
 * @param encoding File encoding; decoded lines are UTF-8 strings without delimiters.
 * @param[out] content ID of an existing output string array.
 * @param[out] line_count Non-NULL successful line count; preserve blank interior lines.
 * @return Operation status; failures do not publish partial arrays.
 */
clm_int clm_file_read_all_text_lines_as(const char* path, clm_int encoding, clm_id content, clm_ulong* line_count);

/** Export decoded lines as NUL-separated UTF-8 strings with one extra final NUL.
 * @param path NUL-terminated UTF-8 source path.
 * @param encoding Source-file encoding and BOM policy.
 * @param[out] content Writable packed buffer; NULL with zero capacity measures only.
 * @param capacity Byte capacity including every terminator.
 * @param[out] required_size Mandatory required capacity; report it on insufficient capacity.
 * @param[out] line_count Mandatory line count; disambiguates blank lines from the final sentinel.
 * @return Operation status; empty lists require one NUL byte, never truncate.
 */
clm_int clm_file_read_all_text_lines_to(const char* path, clm_int encoding, char* content, size_t capacity, size_t* required_size, clm_ulong* line_count);

/** Create or truncate a regular file and write exactly the requested binary bytes.
 * @param path ID of the destination path string, not an open-file ID.
 * @param content ID of a binary BPA source block; size must fit its capacity.
 * @param size Number of bytes to write; no text encoding or terminator processing.
 * @return Operation status; filesystem failures may leave partial output.
 * @note This is not atomic replacement; use write_all_atomic when that contract is required.
 */
clm_int clm_file_write_all(clm_id path, clm_id content, size_t size);

/** Write whole-file binary contents using a raw UTF-8 destination and BPA source.
 * @param path NUL-terminated destination path; create or truncate the file.
 * @param content ID of an existing binary source block.
 * @param size Exact source byte count; zero creates/truncates an empty file.
 * @return Operation status; the whole-file helper owns and closes its native resource.
 */
clm_int clm_file_write_all_as(const char* path, clm_id content, size_t size);

/** Write a whole binary file from caller-owned bytes.
 * @param path NUL-terminated UTF-8 destination path; create or truncate.
 * @param content Read-only source memory; NULL permitted only when size is zero.
 * @param size Number of readable source bytes.
 * @return Operation status; no registered open-file ID is created.
 */
clm_int clm_file_write_all_from(const char* path, const void* content, size_t size);

/** Encode in-memory UTF-8 text into a newly created or truncated text file.
 * @param path ID of the destination path string.
 * @param encoding Target-file encoding and BOM policy.
 * @param content ID of a UTF-8 string source; not an already UTF-16 encoded block.
 * @param size Number of UTF-8 source bytes excluding NUL; must end on a code-point boundary.
 * @return Operation status; conversion errors must be checked before truncation.
 */
clm_int clm_file_write_all_text(clm_id path, clm_int encoding, clm_id content, size_t size);

/** Write encoded whole-file text using a raw UTF-8 destination path.
 * @param path NUL-terminated destination path.
 * @param encoding Target file representation; BOM variants emit the matching BOM.
 * @param content ID of a CLM UTF-8 source string.
 * @param size UTF-8 source byte count, not the resulting encoded byte count.
 * @return Operation status; rejects invalid UTF-8, embedded NUL, and unsupported encodings.
 */
clm_int clm_file_write_all_text_as(const char* path, clm_int encoding, clm_id content, size_t size);

/** Encode caller-owned UTF-8 bytes and write a complete text file.
 * @param path NUL-terminated UTF-8 destination path; create or truncate.
 * @param encoding Target-file encoding and BOM policy.
 * @param content Read-only UTF-8 source; NUL termination is not required within size.
 * @param size Exact UTF-8 source byte count excluding a terminator.
 * @return Operation status; source remains caller-owned and is not modified.
 */
clm_int clm_file_write_all_text_from(const char* path, clm_int encoding, const char* content, size_t size);

/** Append binary bytes by path, opening and closing internally.
 * @param path ID of the destination file path string; create if absent.
 * @param content ID of a binary source BPA block.
 * @param size Exact source byte count; no encoding or newline processing.
 * @return Operation status; does not use or alter a registered open-file cursor.
 */
clm_int clm_file_append(clm_id path, clm_id content, size_t size);

/** Append whole-call binary bytes using a raw UTF-8 path and BPA source.
 * @param path NUL-terminated destination path; create if absent.
 * @param content ID of an existing binary source block.
 * @param size Number of readable bytes bounded by source capacity.
 * @return Operation status; no registered handle is created.
 */
clm_int clm_file_append_as(const char* path, clm_id content, size_t size);

/** Append caller-owned binary data using a raw UTF-8 path.
 * @param path NUL-terminated destination path.
 * @param content Read-only binary input; NULL only for a zero-byte transfer.
 * @param size Exact source byte count.
 * @return Operation status; concurrent whole-record atomicity is not promised.
 */
clm_int clm_file_append_from(const char* path, const void* content, size_t size);

/** Encode UTF-8 text and append it by path without inserting a mid-file BOM.
 * @param path ID of the destination text file path string.
 * @param encoding Target-file representation; existing BOM/encoding must be compatible.
 * @param content ID of the UTF-8 string source.
 * @param size Exact UTF-8 source byte count ending at a code-point boundary.
 * @return Operation status; create absent files using the selected BOM policy.
 */
clm_int clm_file_append_text(clm_id path, clm_int encoding, clm_id content, size_t size);

/** Append encoded text using a raw UTF-8 destination and a CLM string source.
 * @param path NUL-terminated destination file path.
 * @param encoding Target-file encoding and compatible BOM policy.
 * @param content ID of a UTF-8 source string; not already encoded file bytes.
 * @param size UTF-8 source byte count excluding NUL.
 * @return Operation status; does not append an implicit newline.
 */
clm_int clm_file_append_text_as(const char* path, clm_int encoding, clm_id content, size_t size);

/** Append encoded text from caller-owned UTF-8 memory.
 * @param path NUL-terminated UTF-8 destination path.
 * @param encoding Target file encoding; incompatible existing BOMs are errors.
 * @param content Read-only UTF-8 source buffer.
 * @param size Source bytes excluding NUL; no terminator is required within the bound.
 * @return Operation status; invalid text must be rejected before writing.
 */
clm_int clm_file_append_text_from(const char* path, clm_int encoding, const char* content, size_t size);

/** Append encoded UTF-8 text followed by exactly one LF line terminator.
 * @param path ID of the destination file path string.
 * @param encoding Target-file encoding; LF is encoded in the same representation.
 * @param content ID of the UTF-8 text source.
 * @param size Exact UTF-8 source bytes; an existing trailing newline is not removed.
 * @return Operation status; append processing never inserts a BOM mid-file.
 */
clm_int clm_file_append_line_text(clm_id path, clm_int encoding, clm_id content, size_t size);

/** Append one LF-terminated text record using a raw UTF-8 path.
 * @param path NUL-terminated destination path.
 * @param encoding Target-file encoding and BOM policy.
 * @param content ID of the UTF-8 text source.
 * @param size Number of UTF-8 source bytes excluding NUL.
 * @return Operation status; multi-write record atomicity is not promised.
 */
clm_int clm_file_append_line_text_as(const char* path, clm_int encoding, clm_id content, size_t size);

/** Append caller-owned UTF-8 text and exactly one encoded LF.
 * @param path NUL-terminated UTF-8 destination path.
 * @param encoding Target-file representation.
 * @param content Read-only UTF-8 source buffer.
 * @param size Exact source byte count; excludes the LF added by this helper.
 * @return Operation status; validates source text and existing encoding first.
 */
clm_int clm_file_append_line_text_from(const char* path, clm_int encoding, const char* content, size_t size);

/** Open a file and register its resource under a caller-selected unused ID.
 * @param id Unused open-file ID; existing registrations must not be overwritten.
 * @param path ID of the UTF-8 file path string; separate from the open-file identity.
 * @param mode Access flags plus optional CREATE/TRUNCATE/EXCLUSIVE disposition bits.
 * @return Operation status; exclusive requires create, truncation requires write.
 * @note No access flags is invalid; append implies write and excludes truncation.
 */
clm_int clm_file_open(clm_id id, clm_id path, clm_int mode);

/** Open a raw UTF-8 path under an explicit caller-selected open-file ID.
 * @param id Unused open-file ID to register only after native open succeeds.
 * @param path NUL-terminated file path; retained path text is not the handle identity.
 * @param mode CLM_FILE_OPEN_MODE_* access/disposition flags.
 * @return Operation status; do not leak a native resource if registration fails.
 */
clm_int clm_file_open_as(clm_id id, const char* path, clm_int mode);

/** Read up to a bounded number of bytes through an open-file cursor.
 * @param id Readable open-file ID, not a file path ID.
 * @param[out] content ID of an existing writable binary block with at least size bytes.
 * @param size Maximum number of bytes to read; no terminator is added.
 * @param[out] bytes_read Non-NULL actual byte count; zero at EOF is a successful read.
 * @return Operation status; advance by bytes transferred, not by the requested count.
 * @note Partial I/O error output/count semantics must be defined before implementation.
 */
clm_int clm_file_read(clm_id id, clm_id content, size_t size, size_t* bytes_read);

/** Read through an open-file cursor into caller-owned binary memory.
 * @param id Readable open-file ID.
 * @param[out] content Writable memory with at least size bytes; NULL only for zero size.
 * @param size Maximum transfer size and destination capacity in bytes.
 * @param[out] bytes_read Non-NULL actual transfer count; short reads are permitted.
 * @return Operation status; distinguish EOF from I/O failure.
 */
clm_int clm_file_read_to(clm_id id, void* content, size_t size, size_t* bytes_read);

/** Write bytes at the current open-file cursor from a BPA binary block.
 * @param id Writable open-file ID; append mode uses native append positioning.
 * @param content ID of a binary source block with at least size readable bytes.
 * @param size Requested byte count; no encoding or newline conversion.
 * @param[out] bytes_written Non-NULL actual transfer count, including partial I/O progress.
 * @return Operation status; cursor follows bytes written and failures do not roll back.
 */
clm_int clm_file_write(clm_id id, clm_id content, size_t size, size_t* bytes_written);

/** Write caller-owned binary bytes through the current open-file cursor.
 * @param id Writable open-file ID.
 * @param content Read-only source memory; NULL permitted only for a zero-byte write.
 * @param size Number of readable source bytes.
 * @param[out] bytes_written Non-NULL actual count, including partial progress on error.
 * @return Operation status; validate access mode before modifying the file.
 */
clm_int clm_file_write_from(clm_id id, const void* content, size_t size, size_t* bytes_written);

/** Append binary bytes through an open resource registered in append mode.
 * @param id Append-mode open-file ID; not a path string reference.
 * @param content ID of a readable binary source block.
 * @param size Requested byte count, bounded by the source block capacity.
 * @param[out] bytes_written Non-NULL actual byte count; cursor follows the appended bytes.
 * @return Operation status; separate calls need not form an atomic multi-write record.
 */
clm_int clm_file_append_open(clm_id id, clm_id content, size_t size, size_t* bytes_written);

/** Append caller-owned binary bytes through an append-mode open-file ID.
 * @param id Append-mode open-file ID.
 * @param content Read-only source memory with at least size readable bytes.
 * @param size Requested byte count, independent of text encoding.
 * @param[out] bytes_written Non-NULL actual transfer count, including partial progress.
 * @return Operation status; appends occur at native EOF, not a stale cached size.
 */
clm_int clm_file_append_open_from(clm_id id, const void* content, size_t size, size_t* bytes_written);

/** Query a seekable open-file resource's current absolute byte position.
 * @param id Open-file ID, not the original path ID.
 * @param[out] index Non-NULL 64-bit position result, assigned only on success.
 * @return Operation status; non-seekable resources return an explicit error.
 */
clm_int clm_file_tell(clm_id id, clm_ulong* index);

/** Close the resource owned by a registered open-file ID.
 * @param id Open-file ID; closing by path is intentionally not supported.
 * @return Operation status; report late flush/close errors.
 * @note The implemented close policy must retire a consumed native handle even when close reports an error.
 */
clm_int clm_file_close(clm_id id);

/** Query logical regular-file byte size by path, not through an open handle.
 * @param path ID of the file path string; use get_open_size for open-file IDs.
 * @param[out] size Non-NULL unsigned 64-bit result; not allocated disk usage.
 * @return Operation status; output is unchanged on failure.
 */
clm_int clm_file_get_size(clm_id path, clm_ulong* size);

/** Query logical file size using a raw UTF-8 path.
 * @param path NUL-terminated regular-file path; no open-file ID is required.
 * @param[out] size Non-NULL 64-bit byte count, avoiding platform size_t truncation.
 * @return Operation status; missing or non-file inputs are errors.
 */
clm_int clm_file_get_size_as(const char* path, clm_ulong* size);

/** Query actual file creation time when supported by the filesystem.
 * @param id ID of the file path string, not an open-file ID.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; unavailable birth time returns UNSUPPORTED, never Unix ctime.
 */
clm_int clm_file_get_creation_time(clm_id id, time_t* out);

/** Query creation time through a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL timestamp output with native time_t precision.
 * @return Operation status; unsupported metadata is distinct from a zero timestamp.
 */
clm_int clm_file_get_creation_time_as(const char* path, time_t* out);

/** Query the filesystem-reported last modification timestamp.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL native timestamp result, unchanged on failure.
 * @return Operation status; no assumption of nanosecond precision.
 */
clm_int clm_file_get_modification_time(clm_id id, time_t* out);

/** Query last modification time using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; missing entries and query errors must be distinguished.
 */
clm_int clm_file_get_modification_time_as(const char* path, time_t* out);

/** Query a filesystem-reported access timestamp, which may be deferred or disabled.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; not a guarantee of recent application access.
 */
clm_int clm_file_get_access_time(clm_id id, time_t* out);

/** Query access time through a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL native timestamp result, assigned only on success.
 * @return Operation status; respect native mount/access-time policies.
 */
clm_int clm_file_get_access_time_as(const char* path, time_t* out);

/** Query hidden-file classification under the native Path metadata policy.
 * @param id ID of the file path string; inspect the entry without following a link.
 * @param[out] out Non-NULL Boolean result.
 * @return Operation status; native attributes or POSIX dot naming apply.
 */
clm_int clm_file_is_hidden(clm_id id, bool* out);

/** Query native hidden classification using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result, unchanged on failure.
 * @return Operation status; missing entries are not silently false.
 */
clm_int clm_file_is_hidden_as(const char* path, bool* out);

/** Inspect the native read-only attribute, not effective write-access permissions.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unavailable native attributes return UNSUPPORTED.
 */
clm_int clm_file_is_read_only(clm_id id, bool* out);

/** Inspect a read-only attribute through a raw UTF-8 path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result; ACLs and native access checks are separate.
 * @return Operation status; unsupported concepts remain distinct from false.
 */
clm_int clm_file_is_read_only_as(const char* path, bool* out);

/** Inspect native filesystem compression metadata, not a file format signature.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unsupported compression metadata returns UNSUPPORTED.
 */
clm_int clm_file_is_compressed(clm_id id, bool* out);

/** Inspect compression metadata using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result; unrelated to ZIP or other encoded file formats.
 * @return Operation status; native capability limitations are explicit.
 */
clm_int clm_file_is_compressed_as(const char* path, bool* out);

/** Inspect the native archive attribute, not whether the file contains an archive.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unsupported platform attributes are explicit.
 */
clm_int clm_file_is_archive(clm_id id, bool* out);

/** Inspect an archive attribute using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result; not an archive-file decoder.
 * @return Operation status; unsupported metadata must not be reported as false.
 */
clm_int clm_file_is_archive_as(const char* path, bool* out);

/** Exclusively create a new empty regular file, then close it.
 * @param path ID of the target path string; existing entries are errors.
 * @return Operation status; no parent creation or open-file registration.
 */
clm_int clm_file_create(clm_id path);

/** Exclusively create and close a new file using a raw UTF-8 path.
 * @param path NUL-terminated target path; the caller retains ownership.
 * @return Operation status; existing entries and missing parents fail.
 */
clm_int clm_file_create_as(const char* path);

/** Copy regular-file contents to another path without following an unexpected destination link.
 * @param source ID of the source file path string.
 * @param destination ID of the destination path string, not an output string.
 * @param overwrite Whether an existing regular-file destination may be replaced.
 * @return Operation status; identical file identities must be rejected.
 * @note Metadata preservation and cleanup of a partially copied destination require explicit policy.
 */
clm_int clm_file_copy(clm_id source, clm_id destination, bool overwrite);

/** Copy a regular file using raw UTF-8 source and destination paths.
 * @param source NUL-terminated source path.
 * @param destination NUL-terminated destination path; not a writable output buffer.
 * @param overwrite Whether an existing regular file may be replaced; directories are errors.
 * @return Operation status; never silently copy a file onto itself.
 */
clm_int clm_file_copy_as(const char* source, const char* destination, bool overwrite);

/** Move or rename a file without replacing an existing destination.
 * @param source ID of the existing source file path string.
 * @param destination ID of the destination path string; neither ID is remapped.
 * @return Operation status; cross-filesystem moves fail instead of implicit copy/delete.
 */
clm_int clm_file_move(clm_id source, clm_id destination);

/** Move or rename a file using raw UTF-8 paths.
 * @param source NUL-terminated source file path.
 * @param destination NUL-terminated destination path; existing entries are errors.
 * @return Operation status; no implicit overwrite or parent creation.
 */
clm_int clm_file_move_as(const char* source, const char* destination);

/** Delete a file entry; remove a symbolic link itself rather than its target.
 * @param path ID of the file or file-link path string; directories are rejected.
 * @return Operation status; missing-entry policy must be explicit.
 */
clm_int clm_file_delete(clm_id path);

/** Delete a file entry using a raw UTF-8 path.
 * @param path NUL-terminated file or file-link path; never a directory tree.
 * @return Operation status; do not close unrelated open-file IDs.
 */
clm_int clm_file_delete_as(const char* path);

/** Test whether an open-file ID is registered and currently owns a native resource.
 * @param id Open-file ID, not a path-string ID.
 * @return 1 if open, 0 if unregistered/closed, -1 on registry errors.
 */
clm_int clm_file_is_open(clm_id id);

/** Move the open-file cursor using a signed 64-bit offset.
 * @param id Open-file ID with seek support.
 * @param offset Signed displacement relative to origin; overflow and negative results fail.
 * @param origin CLM_FILE_SEEK_BEGIN, CLM_FILE_SEEK_CURRENT, or CLM_FILE_SEEK_END.
 * @return Operation status; seeking beyond EOF does not itself extend the file.
 */
clm_int clm_file_seek(clm_id id, clm_long offset, clm_int origin);

/** Inspect end-of-file status observed by the previous sequential read.
 * @param id Readable open-file ID.
 * @param[out] out Non-NULL Boolean EOF result; not a prediction of future file growth.
 * @return Operation status; I/O errors must not be reported as EOF.
 */
clm_int clm_file_eof(clm_id id, bool* out);

/** Flush user-space write buffers to the operating system without promising durability.
 * @param id Writable open-file ID.
 * @return Operation status; late buffered-write failures must be reported.
 */
clm_int clm_file_flush(clm_id id);

/** Request native durable synchronization of an open file after flushing buffers.
 * @param id Writable open-file ID; the handle remains open.
 * @return Operation status; native durability limitations or lack of support are explicit.
 * @note File synchronization alone does not guarantee directory-entry durability.
 */
clm_int clm_file_sync(clm_id id);

/** Query current logical size through an open resource rather than its original path.
 * @param id Open-file ID; renaming its path must not invalidate this query.
 * @param[out] size Non-NULL 64-bit byte count, assigned only on success.
 * @return Operation status; do not rely on a stale cached size.
 */
clm_int clm_file_get_open_size(clm_id id, clm_ulong* size);

/** Set logical length through a writable open-file resource.
 * @param id Writable open-file ID; append-only restrictions must be checked.
 * @param size New 64-bit logical byte length; native representability is validated.
 * @return Operation status; cursor position remains unchanged, even if beyond new EOF.
 * @note Extension zero-fill, sparse allocation, and partial-error rules require native documentation.
 */
clm_int clm_file_truncate(clm_id id, clm_ulong size);

/** Read bytes at an absolute offset without changing the sequential cursor.
 * @param id Readable open-file ID.
 * @param offset Unsigned 64-bit absolute file offset; validate native representability.
 * @param content ID of an existing writable binary BPA block with sufficient capacity.
 * @param size Maximum number of bytes to read, not a terminator-inclusive text size.
 * @param[out] bytes_read Non-NULL actual byte count; short reads and EOF are not automatic errors.
 * @return Operation status; partial reads on I/O failure need defined output/count behavior.
 */
clm_int clm_file_read_at(clm_id id, clm_ulong offset, clm_id content, size_t size, size_t* bytes_read);

/** Read at an absolute offset into caller-owned memory without moving the cursor.
 * @param id Readable open-file ID.
 * @param offset Unsigned 64-bit absolute offset.
 * @param[out] content Writable buffer containing at least size bytes; NULL only when size is zero.
 * @param size Maximum transfer size and destination capacity in bytes.
 * @param[out] bytes_read Non-NULL actual byte count, including a partial successful read.
 * @return Operation status; distinguish zero-byte EOF from query failure.
 */
clm_int clm_file_read_at_to(clm_id id, clm_ulong offset, void* content, size_t size, size_t* bytes_read);

/** Write bytes at an absolute offset without changing the sequential cursor.
 * @param id Writable open-file ID; append-mode handles must be rejected for positioned writes.
 * @param offset Unsigned 64-bit destination offset.
 * @param content ID of an existing binary source BPA block.
 * @param size Number of source bytes to write, bounded by the source block capacity.
 * @param[out] bytes_written Non-NULL actual byte count, including partial I/O progress.
 * @return Operation status; an error does not undo bytes already written.
 */
clm_int clm_file_write_at(clm_id id, clm_ulong offset, clm_id content, size_t size, size_t* bytes_written);

/** Write caller-owned bytes at an absolute offset without moving the cursor.
 * @param id Writable non-append open-file ID.
 * @param offset Unsigned 64-bit absolute file offset.
 * @param content Read-only source buffer with at least size readable bytes.
 * @param size Number of bytes to transfer.
 * @param[out] bytes_written Non-NULL actual transfer count, including partial progress.
 * @return Operation status; caller retains source-buffer ownership.
 */
clm_int clm_file_write_at_from(clm_id id, clm_ulong offset, const void* content, size_t size, size_t* bytes_written);

/** Lock a byte range of an open file under native advisory/mandatory lock semantics.
 * @param id Open-file ID with the access mode required for the selected lock.
 * @param offset Unsigned 64-bit start offset.
 * @param length Unsigned byte length; zero means from offset through future EOF.
 * @param exclusive True for exclusive locking; false for a shared lock.
 * @param wait Whether to block waiting for a conflicting lock to be released.
 * @return Operation status; unsupported or unrepresentable ranges fail explicitly.
 * @note Lock ownership, process/thread interaction, and close behavior are platform-specific.
 */
clm_int clm_file_lock(clm_id id, clm_ulong offset, clm_ulong length, bool exclusive, bool wait);

/** Release a previously acquired byte-range lock.
 * @param id Open-file ID that owns the lock under the platform's ownership rules.
 * @param offset Unsigned 64-bit lock start offset.
 * @param length Byte length, or zero for the to-EOF range convention.
 * @return Operation status; do not silently unlock unrelated ranges.
 */
clm_int clm_file_unlock(clm_id id, clm_ulong offset, clm_ulong length);

/** Query file metadata and capability masks without modifying the source path text.
 * @param path ID of the file or file-link path string.
 * @param follow_symlinks Whether metadata describes the target rather than the entry itself.
 * @param[out] out Non-NULL metadata result, unchanged on query failure.
 * @return Operation status; optional fields have explicit support masks.
 */
clm_int clm_file_get_info(clm_id path, bool follow_symlinks, clm_file_info* out);

/** Query file metadata using a raw UTF-8 path.
 * @param path NUL-terminated file or file-link path.
 * @param follow_symlinks Whether to inspect the target instead of a link entry.
 * @param[out] out Non-NULL metadata result; unsupported fields are marked in masks.
 * @return Operation status; missing-entry and size-field validity policies need definition.
 */
clm_int clm_file_get_info_as(const char* path, bool follow_symlinks, clm_file_info* out);

/** Set supported mutable file attributes without replacing unrelated attributes.
 * @param path ID of the target file path string.
 * @param attributes Desired values for mask-selected attribute bits.
 * @param mask Bits to update; identity/classification bits are not mutable attributes.
 * @return Operation status; unsupported requested changes fail explicitly.
 * @note A read-only attribute is not an ACL or effective write-access decision.
 */
clm_int clm_file_set_attributes(clm_id path, clm_ulong attributes, clm_ulong mask);

/** Set selected mutable file attributes using a raw UTF-8 path.
 * @param path NUL-terminated file path.
 * @param attributes Desired selected attribute values.
 * @param mask Attribute bits to change; unsupported bits are errors.
 * @return Operation status; native partial-update behavior needs documentation.
 */
clm_int clm_file_set_attributes_as(const char* path, clm_ulong attributes, clm_ulong mask);

/** Set selected file timestamps without substituting metadata-change time for creation.
 * @param path ID of the target file path string.
 * @param creation_time Requested creation timestamp when selected.
 * @param modification_time Requested modification timestamp when selected.
 * @param access_time Requested access timestamp when selected.
 * @param mask CLM_FILE_TIME_* fields to update; other fields remain unchanged.
 * @return Operation status; unavailable creation-time changes return UNSUPPORTED.
 */
clm_int clm_file_set_times(clm_id path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask);

/** Set selected timestamps using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param creation_time Requested creation time when selected.
 * @param modification_time Requested modification time when selected.
 * @param access_time Requested access time when selected.
 * @param mask CLM_FILE_TIME_* selection bits; zero requests no changes.
 * @return Operation status; native partial-update behavior must be documented.
 */
clm_int clm_file_set_times_as(const char* path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask);

/** Exclusively create a private temporary file and register its open read/write resource.
 * @param id Caller-selected unused open-file ID.
 * @param directory ID of an existing parent directory path string.
 * @param[out] path ID of an existing string destination for the created path.
 * @return Operation status; close and remove the new entry if publication fails.
 * @note Successful callers must close the open-file ID and remove the file explicitly.
 */
clm_int clm_file_create_temporary(clm_id id, clm_id directory, clm_id path);

/** Create and open a temporary file inside a raw UTF-8 directory path.
 * @param id Caller-selected unused open-file ID for the new resource.
 * @param directory NUL-terminated existing parent directory path.
 * @param[out] path ID of an existing string receiving the new file path.
 * @return Operation status; no implicit parent creation.
 */
clm_int clm_file_create_temporary_as(clm_id id, const char* directory, clm_id path);

/** Create and open a temporary file, exporting its path into caller-owned memory.
 * @param id Unused open-file ID; size-query mode must not register it.
 * @param directory NUL-terminated existing parent directory path.
 * @param[out] path Writable UTF-8 output; NULL with zero capacity measures only.
 * @param capacity Available byte capacity including the terminator.
 * @param[out] required_size Mandatory required byte capacity; no creation on insufficient capacity.
 * @return Operation status; size-query mode must have no creation side effects.
 */
clm_int clm_file_create_temporary_to(clm_id id, const char* directory, char* path, size_t capacity, size_t* required_size);

/** Replace whole-file bytes through a temporary sibling and native atomic rename.
 * @param path ID of the destination file path string.
 * @param content ID of an existing binary source BPA block.
 * @param size Number of readable source bytes.
 * @return Operation status; lack of atomic replacement support is explicit.
 * @note Atomic visibility is not crash durability; preservation of metadata needs policy.
 */
clm_int clm_file_write_all_atomic(clm_id path, clm_id content, size_t size);

/** Atomically replace file bytes using a raw UTF-8 destination and BPA source.
 * @param path NUL-terminated destination file path.
 * @param content ID of a binary source block containing at least size bytes.
 * @param size Exact source byte count; zero replaces the file with an empty file.
 * @return Operation status; native replacement and link policies must be specified.
 */
clm_int clm_file_write_all_atomic_as(const char* path, clm_id content, size_t size);

/** Atomically replace a file from caller-owned binary data.
 * @param path NUL-terminated destination file path.
 * @param content Read-only source buffer; NULL only for a zero-byte replacement.
 * @param size Number of readable source bytes.
 * @return Operation status; clean up staging files on failure.
 */
clm_int clm_file_write_all_atomic_from(const char* path, const void* content, size_t size);

/** Registers a global stringtify interceptor for file object writes/appends.
 * It replaces default rendering; returning CLM_STRINGTIFY_USE_DEFAULT selects
 * the normal text. NULL clears it. The callback and user must stay valid while
 * writes may run. Independent of the console interceptor; files never get ANSI color.
 */
void clm_file_intercept_object_writes(clm_stringtify_interceptor_fn interceptor, void* user);

/** Clears the global interceptor for file object writes/appends. */
void clm_file_do_not_intercept_object_writes(void);

/* Object writers: render a CLM object with clm_stringtify and write it as text.
 * write_* creates or truncates the file; append_* adds to the end (created when
 * absent, no mid-file BOM, no added newline). Each has a clm_id path form and an
 * _as raw UTF-8 path form; encoding is a CLM_FILE_TEXT_ENCODING_* value.
 * Text is produced before the file is opened, so a render failure leaves it unchanged.
 * Objects stay caller-owned. Returns SUCCESS, or FAILURE for invalid IDs/paths/
 * encodings, render errors, or I/O errors.
 *
 * block/inspect_block: string blocks write as raw text from block; inspect_block
 *   instead quotes and escapes strings (debug form), like other block types.
 * dictionary, storage, table (aligned, bordered), tree: readable indented text.
 * tree_as_yaml/tree_as_json: object/array roots through clm_string_yaml/json;
 *   they ignore the global interceptor.
 */
clm_int clm_file_write_block(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_block_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_write_dictionary(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_dictionary_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_write_storage(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_storage_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_write_table(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_table_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_write_tree(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_tree_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_write_inspect_block(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_inspect_block_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_write_tree_as_yaml(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_tree_as_yaml_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_write_tree_as_json(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_write_tree_as_json_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_block(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_block_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_dictionary(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_dictionary_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_storage(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_storage_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_table(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_table_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_tree(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_tree_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_inspect_block(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_inspect_block_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_tree_as_yaml(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_tree_as_yaml_as(const char* path, clm_int encoding, clm_id id);
clm_int clm_file_append_tree_as_json(clm_id path, clm_int encoding, clm_id id);
clm_int clm_file_append_tree_as_json_as(const char* path, clm_int encoding, clm_id id);

/** Close every remaining native resource and release all active and retired IDs.
 * @return SUCCESS when all closes succeed, otherwise FAILURE after attempting every close.
 * @note Callers must synchronize access; IDs may be reused only after this global teardown.
 */
clm_int clm_file_shutdown(void);