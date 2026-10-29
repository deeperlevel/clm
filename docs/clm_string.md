# CLM - String

## 1. Purpose and scope

`clm_string` provides string and string-array operations backed by BPA and BPAA. Strings are stored as managed BPA string blocks; arrays of strings are stored as BPAA string arrays. The module owns copies of string content and uses logical IDs instead of exposing stored buffers for routine operations.

The API is procedural and source-first. Applications may use a language's native string and collection types when appropriate, while retaining these operations where CLM IDs, BPA ownership, or cross-language consistency are useful.

## 2. Encoding configuration

`clm_string` stores validated Unicode text as **UTF-8 internally** in C. Ordinary string indexing and length calculations count Unicode code points, not bytes or grapheme clusters. The module's preferred *external byte encoding* is runtime configurable and defaults to UTF-8; it does not alter existing stored Unicode strings.

```c
clm_string_set_default_encoding(CLM_STRING_ENCODING_UTF8);
clm_byte encoding = clm_string_get_default_encoding();
```

Currently supported explicit external encodings are **UTF-8**, **Latin-1 (ISO-8859-1)**, and **UTF-16LE**. They are not arbitrary operating-system ANSI code pages. `clm_string_new(id, text)` accepts UTF-8 C-string input regardless of the runtime default. Each new string records the current default as its preferred *byte-export encoding*; existing strings retain their preference when the default changes.

```text
string.get_bytes(id, destination_buffer, byte_capacity) → encoded_byte_count
string.from_bytes(id, encoding, source_buffer, exact_byte_length) → status
string.from_enc(id, encoding, source_buffer, exact_byte_length) → status
string.change_encoding(id, encoding) → status
```

`get_bytes` emits exactly the encoded bytes (no terminator or BOM), and supports querying the required byte count with no buffer. `from_bytes` decodes into UTF-8 text and creates or replaces the specified string ID; it sets the string's preferred byte-export encoding to the input encoding. `from_enc` also decodes an exact-length byte sequence, but follows the `from_from` constructor behavior: a new string uses the runtime default encoding, while an existing string keeps its current preference. Both reject invalid input, unsupported encodings, and embedded NUL/U+0000. `change_encoding` updates the string's preferred byte-export encoding without changing text; it fails if the string cannot be represented in the target encoding. The runtime default is shared state and is not thread-safe to change during concurrent operations.

## 3. Results and error behavior

The C reference uses BPA results:

| Result | Meaning |
|---|---|
| `CLM_BPA_SUCCESS` (`0`) | General operation succeeded |
| `CLM_BPA_GET_SUCCESS` (`1`) | A string or array value was retrieved |
| `CLM_BPA_SET_SUCCESS` (`2`) | A string-array item was assigned |
| `CLM_BPA_FAILURE` (`-1`) | Missing resource, wrong resource type, invalid argument/index, invalid encoding, or allocation failure |

Operations returning a count or length return a nonnegative `clm_int` on success and `CLM_BPA_FAILURE` on failure. Boolean creation/deletion/search operations return `true` or `false`. A search returning an index uses `CLM_BPA_FAILURE` for both no match and invalid input.

## 4. String blocks

### 4.1 Lifecycle and assignment

| Operation | Behavior |
|---|---|
| `clm_string_new(id, text)` | Creates a BPA string block, copies UTF-8 input, and records the runtime default preferred export encoding. Returns a boolean. |
| `clm_string_delete(id)` | Deletes the resource only if it exists and is a BPA string block. |
| `clm_string_set(id, text)` | Replaces the full value; returns its character count. |
| `clm_string_set_from(id, text)` | Alias of `clm_string_set`. |
| `clm_string_length(id)` | Returns the character count in Unicode code points. |

Creation rejects null text, duplicate IDs, invalid UTF-8, and allocation failure. On an initialization failure after block creation, the newly created block is removed. `set` and later edits preserve the existing string if validation or replacement allocation fails.

### 4.2 Editing and searching

| Operation | Behavior and result |
|---|---|
| `clm_string_append(id, suffix)` | Appends suffix; returns the number of characters appended. |
| `clm_string_append_line(id, suffix)` | Appends suffix and one LF (`\n`); count includes the newline. |
| `clm_string_insert(id, index, text)` | Inserts before the character at `index`; `index == length` appends. Returns inserted character count. |
| `clm_string_remove(id, index, length)` | Removes a character range. Returns the number removed; a zero-length removal succeeds with `0`. |
| `clm_string_contains(id, search)` | Returns whether the string contains the byte sequence of a valid search string. |
| `clm_string_replace(id, search, replacement)` | Replaces all non-overlapping occurrences and returns the number replaced. |

Indices are zero-based and always count Unicode code points; they are independent of the preferred byte-export encoding. Search and replacement text must be valid UTF-8. An empty search is invalid for replacement; an empty replacement is allowed. A valid empty `search` passed to `contains` matches any existing string.

The C implementation performs byte-sequence matching. Because both the stored text and UTF-8 search/replacement strings are validated, a valid multi-byte search sequence matches on its encoding boundaries. Replacement is global, left-to-right, and non-overlapping.

`clm_string_replace_from(id, search, replacement)` is an alias of `clm_string_replace`.

### 4.3 Unicode slicing

```text
string.slice(source_string_id, start, length, destination_string_id)
string.slice_borrow(source_string_id, start, length, &text, &byte_length)
```

`slice` creates a **new, independently owned CLM String** containing the requested range of Unicode code points, using zero-based `start` and `length` (not byte offsets). The destination BPA string ID must not already exist. A zero-length slice at the string end is valid; negative or out-of-range slices fail. The implementation resolves UTF-8 byte boundaries before copying, so it never divides a multibyte sequence. The resulting string stores UTF-8 internally and retains the **source string's preferred byte-export encoding**, even when the current runtime default differs. The source string is not modified, and the destination supports all ordinary `clm_string` operations.

`slice_borrow` returns a direct UTF-8 view and its byte length without allocating or copying. The returned range is **not necessarily NUL-terminated at its end**. It does not retain the source block; the source must remain live and unmodified for the entire time the view is used. Any replacement/edit or deletion of the source invalidates the view. Use `slice` when an independent lifetime is needed.

Example:

```text
string.new(MESSAGE, "Hello, 世界!")
string.slice(MESSAGE, start=7, length=2, destination_string_id=WORD)
# WORD is "世界"; it can now be appended, searched, or encoded independently.
```

### 4.4 Copy and decode constructors

These constructors create a missing destination string or update an existing
string block. If the destination ID already names a non-string BPA resource,
the operation fails without changing it.

| Operation | Behavior |
|---|---|
| `clm_string_from(destination_id, source_id)` | Copies a string block's UTF-8 text and preferred export encoding. The source must be a valid string block. |
| `clm_string_from_from(destination_id, text)` | Copies NUL-terminated UTF-8 text. A new string uses the runtime default encoding; an existing string keeps its preference. |
| `clm_string_from_enc(destination_id, encoding, bytes, byte_length)` | Decodes an exact-length UTF-8, Latin-1, or UTF-16LE byte sequence, then applies `clm_string_from_from` behavior. |
| `clm_string_from_from_enc(destination_id, encoding, bytes, byte_length)` | Alias of `clm_string_from_enc`. |

These functions return `CLM_BPA_SUCCESS` or `CLM_BPA_FAILURE`, not a character
count. The encoding-aware constructors accept bytes without a terminator or
BOM; malformed sequences, unsupported encodings, and embedded U+0000 fail.
`clm_string_from` copies the source's preferred export encoding, whereas
`clm_string_from_enc` treats the input encoding only as a decoding instruction.
For `from_enc`, a newly created destination uses the current default encoding,
and an existing destination retains its preference.

## 5. String arrays

String arrays use a BPAA array ID and store independent copies of each string. Operations which supply or receive an item use the ID of an existing BPA string block; they do not return pointers into the array.

### 5.1 Lifecycle and item access

| Operation | Behavior |
|---|---|
| `clm_string_array_new(array_id, reserved)` | Creates a string array with initial reserved capacity and the configured default encoding. |
| `clm_string_array_delete(array_id)` | Deletes only if the ID names a string array. |
| `clm_string_array_count(array_id)` | Returns item count or failure. |
| `clm_string_array_get(array_id, index, destination_string_id)` | Copies an item into an existing BPA string block. |
| `clm_string_array_set(array_id, index, source_string_id)` | Replaces an existing item with a BPA string's value. |
| `clm_string_array_insert(array_id, index, source_string_id)` | Inserts a copy; index may equal count. |
| `clm_string_array_append(array_id, source_string_id)` | Appends a copy. |
| `clm_string_array_remove(array_id, index)` | Removes an item and compacts following elements. |
| `clm_string_array_clear(array_id)` | Removes all items but retains reserved capacity. |
| `clm_string_array_find(array_id, source_string_id)` | Returns the first exact bytewise match index or failure. |

The array and string block IDs are in separate registries. A source block must be a BPA string block, and its content must be valid under the array's encoding. The source and array keep independent copies: later modification or deletion of the source does not alter an array item.

### 5.2 Indexed string operations

These operations edit the string stored at an array index without first copying it to a separate BPA string block:

| Operation | Behavior and result |
|---|---|
| `clm_string_array_length_at(array_id, index)` | Returns the item's character count. |
| `clm_string_array_contains_at(array_id, index, search)` | Returns whether the item contains a valid search string. |
| `clm_string_array_append_at(array_id, index, suffix)` | Appends text and returns appended character count. |
| `clm_string_array_append_line_at(array_id, index, suffix)` | Appends text plus LF; count includes LF. |
| `clm_string_array_insert_at(array_id, index, character_index, text)` | Inserts by Unicode code-point index. |
| `clm_string_array_remove_at(array_id, index, character_index, length)` | Removes the requested character range. |
| `clm_string_array_replace_at(array_id, index, search, replacement)` | Replaces all non-overlapping matches and returns match count. |

String arrays also use validated UTF-8 internal text and Unicode code-point indexing. Invalid indices, invalid text, unsupported encodings, and allocation failures return failure without replacing the original item.

### 5.3 Sorting, cursor, and deck

| Operation | Behavior |
|---|---|
| `clm_string_array_sort(array_id)` | Sorts lexicographically by encoded bytes and resets cursor/deck state. |
| `clm_string_array_shuffle(array_id)` | Shuffles items using the C library random generator; resets cursor and deck. |
| `clm_string_array_move_first/previous/next/last(array_id)` | Moves the BPAA shared cursor, subject to normal bounds. |
| `clm_string_array_move_to(array_id, index)` | Moves the cursor to an existing item. |
| `clm_string_array_get_current(array_id, destination_string_id)` | Copies the item at the current cursor into an existing BPA string block. |
| `clm_string_array_draw_at(array_id, deck_index, destination_string_id)` | Copies a selected undrawn item to a BPA string block and removes it from the active deck. |
| `clm_string_array_draw(array_id, destination_string_id)` | Draws the next item, shuffling a new cycle when the active deck is exhausted. |

Cursor state is shared by all users of an array. Drawing does not remove array items or change count. Within one deck cycle, an item is drawn at most once; after exhaustion, `draw` reshuffles and starts another cycle. Drawing and rearranging are not cryptographically random.

Sort is bytewise, not locale-aware or Unicode collation. UTF-8 bytewise ordering is deterministic but is not a user-language linguistic sort.

## 6. Memory and ownership

String blocks and individual array strings are copied into BPA's internal memory pool. String edits prepare replacement content before releasing the old value, so failure does not invalidate the current value. Temporary edit buffers also use the pool and are returned after the operation.

String-array backing storage is managed by BPAA. Removing, clearing, or deleting an array releases its owned strings according to the BPAA lifecycle. IDs remain the stable handles; pointers into implementation storage are not part of this API.

The module inherits BPA/BPAA limitations: global registries are not thread-safe, IDs can be reused after deletion, and shutdown invalidates live IDs. See [CLM Block Pool Allocation](./clm_bpa.md) for registry, pool, and cleanup contracts.

## 7. Examples

```text
string.new(MESSAGE, "Hello")
string.append(MESSAGE, ", world")
string.insert(MESSAGE, 5, " there")
string.replace(MESSAGE, "world", "CLM")
string.length(MESSAGE)
```

```text
string.new(FRUIT, "pear")
string.new(ITEM, "apple")
string.array.new(FRUITS, reserved=4)
string.array.append(FRUITS, FRUIT)
string.array.insert(FRUITS, index=0, source=ITEM)
string.array.sort(FRUITS)
string.array.get(FRUITS, index=0, destination=ITEM)
```

The examples use conceptual operation names; individual language bindings may use their native call syntax while preserving these behaviors.

## 8. Suggested conformance checks

- Creation, replacement, append, append-line, insert, remove, contains, length, and replace-all.
- UTF-8 internal storage, valid multi-byte scalars, invalid/overlong sequences, and code-point indexing.
- Runtime preferred external byte encodings: UTF-8, Latin-1, UTF-16LE; decode/encode round trips and unsupported-character rejection.
- Empty strings, empty suffixes/replacements, and rejected empty replacement search.
- Missing IDs, wrong BPA/BPAA resource types, duplicate IDs, and deletion safety.
- String array create/get/set/insert/append/find/remove/clear/delete and ownership independence.
- Indexed array-string edits, sort order, cursor bounds, and invalid index handling.
- Deck draws without replacement per cycle, reset/shuffle behavior, and count preservation.
- Slicing preserves Unicode code-point boundaries and source export-encoding preference; zero-length, invalid ranges, duplicate destination IDs, and independent ownership.
- Allocation failure leaves the prior block/item value unchanged.
