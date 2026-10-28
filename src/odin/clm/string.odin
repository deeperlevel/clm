package clm

import "core:strings"
import "core:strconv"
import "core:math/rand"

// Text is always validated UTF-8. Encodings describe external byte representations.
// Native string inputs use their exact length (not C's strlen); embedded NUL is
// rejected. Byte exports use []u8; nil queries size, and omit NUL/BOM. Borrowed
// string views expire when their BPA owner is modified or deleted.
CLM_STRING_ENCODING_UTF8 :: u8(1)
CLM_STRING_ENCODING_LATIN1 :: u8(2)
CLM_STRING_ENCODING_UTF16LE :: u8(3)

// Configure before concurrent use; existing blocks retain their preference.
clm_string_global_default_encoding := CLM_STRING_ENCODING_UTF8

clm_string_encoding_supported :: proc(encoding: u8) -> bool {
	return encoding == CLM_STRING_ENCODING_UTF8 ||
	       encoding == CLM_STRING_ENCODING_LATIN1 ||
	       encoding == CLM_STRING_ENCODING_UTF16LE
}

/*
Changes the preferred byte-export encoding for subsequently created strings.
Does not change Unicode text or existing strings' preferences.
*/
clm_string_set_default_encoding :: proc(encoding: u8) -> i32 {
	if !clm_string_encoding_supported(encoding) { return CLM_BPA_FAILURE }
	clm_string_global_default_encoding = encoding
	return CLM_BPA_SUCCESS
}

/*
Returns the runtime default byte-export encoding (initially UTF-8).
*/
clm_string_get_default_encoding :: proc() -> u8 {
	return clm_string_global_default_encoding
}

clm_string_utf8_next :: proc(text: []u8, length: int, offset: ^int) -> bool {
	if offset == nil || length < 0 || length > len(text) || offset^ < 0 || offset^ >= length { return false }
	start := offset^
	first := text[start]
	if first < 0x80 { offset^ += 1; return true }
	n: int
	cp: u32
	switch {
	case first >= 0xc2 && first <= 0xdf: n = 2; cp = u32(first & 0x1f)
	case first >= 0xe0 && first <= 0xef: n = 3; cp = u32(first & 0x0f)
	case first >= 0xf0 && first <= 0xf4: n = 4; cp = u32(first & 7)
	case: return false
	}
	if n > length-start { return false }
	for j in 1..<n {
		b := text[start+j]
		if b & 0xc0 != 0x80 { return false }
		cp = (cp << 6) | u32(b & 0x3f)
	}
	if (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
	   (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff { return false }
	offset^ = start+n
	return true
}

clm_string_measure :: proc(text: string, byte_length: int, encoding: u8, character_count: ^int) -> i32 {
	if character_count == nil || encoding != CLM_STRING_ENCODING_UTF8 ||
	   byte_length < 0 || byte_length > len(text) { return CLM_BPA_FAILURE }
	count, off := 0, 0
	for off < byte_length {
		if !clm_string_utf8_next(transmute([]u8)text, byte_length, &off) { return CLM_BPA_FAILURE }
		count += 1
		if count > 0x7fff_ffff { return CLM_BPA_FAILURE }
	}
	character_count^ = count
	return CLM_BPA_SUCCESS
}

clm_string_offset :: proc(text: string, byte_length: int, encoding: u8, character_index: int, byte_offset: ^int) -> i32 {
	if byte_offset == nil || encoding != CLM_STRING_ENCODING_UTF8 ||
	   byte_length < 0 || byte_length > len(text) || character_index < 0 { return CLM_BPA_FAILURE }
	off := 0
	for i in 0..<character_index {
		if !clm_string_utf8_next(transmute([]u8)text, byte_length, &off) { return CLM_BPA_FAILURE }
	}
	byte_offset^ = off
	return CLM_BPA_SUCCESS
}

clm_string_valid :: proc(text: string) -> bool {
	n: int
	return !strings.contains(text, "\x00") &&
	       clm_string_measure(text, len(text), CLM_STRING_ENCODING_UTF8, &n) == CLM_BPA_SUCCESS
}

clm_string_get_block :: proc(id: idkey, block: ^^clm_bpa_block_node, text: ^string, byte_length, character_count: ^int) -> i32 {
	if block == nil || text == nil || byte_length == nil || character_count == nil { return CLM_BPA_FAILURE }
	found := clm_bpa_find_block(id)
	if found == nil || found.type != CLM_BPA_TYPE_STRING ||
	   found.length > 0x7fff_ffff || (found.length != 0 && found.data == nil) { return CLM_BPA_FAILURE }
	value := ""
	if found.length != 0 { value = string(([^]u8)(found.data)[:int(found.length)]) }
	if clm_string_measure(value, len(value), CLM_STRING_ENCODING_UTF8, character_count) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	block^ = found
	text^ = value
	byte_length^ = len(value)
	return CLM_BPA_SUCCESS
}

clm_string_view :: proc(id: idkey) -> (text: string, characters: int, ok: bool) {
	b: ^clm_bpa_block_node
	n: int
	ok = clm_string_get_block(id, &b, &text, &n, &characters) == CLM_BPA_SUCCESS
	return
}

clm_string_temp_alloc :: proc(bytes: int) -> []u8 {
	if bytes <= 0 { return nil }
	p := clm_bpa_pool_alloc(bytes)
	if p == nil { return nil }
	return ([^]u8)(p)[:bytes]
}

clm_string_temp_free :: proc(text: []u8) {
	clm_bpa_pool_free(raw_data(text), len(text))
}

clm_string_replace_bytes :: proc(id: idkey, text: string, byte_length: int) -> i32 {
	if byte_length < 0 || byte_length > len(text) || byte_length >= 0xffff_ffff { return CLM_BPA_FAILURE }
	buffer := clm_string_temp_alloc(byte_length+1)
	if buffer == nil { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(buffer)
	copy(buffer, transmute([]u8)text[:byte_length])
	buffer[byte_length] = 0
	if clm_bpa_replace(id, raw_data(buffer), u32(len(buffer))) < 0 { return CLM_BPA_FAILURE }
	return CLM_BPA_SUCCESS
}

/*
Creates a string BPA block and initializes it with a UTF-8 string.
Returns: true on success; false for invalid text, duplicate ID, or allocation failure.
*/
clm_string_new :: proc(id: idkey, text: string) -> bool {
	if !clm_string_valid(text) || clm_bpa_create(id, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS { return false }
	block := clm_bpa_find_block(id)
	if block == nil || clm_string_replace_bytes(id, text, len(text)) != CLM_BPA_SUCCESS {
		clm_bpa_delete(id)
		return false
	}
	block.encoding = clm_string_get_default_encoding()
	return true
}

/*
Deletes a string block without affecting blocks of other types.
Returns: true when deleted; false if missing or not a string block.
*/
clm_string_delete :: proc(id: idkey) -> bool {
	block := clm_bpa_find_block(id)
	return block != nil && block.type == CLM_BPA_TYPE_STRING && clm_bpa_delete(id) == CLM_BPA_SUCCESS
}

/*
Replaces a string block's value; returns its character count or failure.
*/
clm_string_set :: proc(id: idkey, text: string) -> i32 {
	block := clm_bpa_find_block(id)
	if block == nil || block.type != CLM_BPA_TYPE_STRING || !clm_string_valid(text) { return CLM_BPA_FAILURE }
	n: int
	clm_string_measure(text, len(text), CLM_STRING_ENCODING_UTF8, &n)
	if clm_string_replace_bytes(id, text, len(text)) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return i32(n)
}

// Shared edit engine keeps character indexing and allocation failure atomic for
// both independent strings and array-owned strings.
clm_string_edit :: proc(text: string, index, length: i32, inserted: string) -> (buffer: []u8, count: i32) {
	count = CLM_BPA_FAILURE
	n, added, first, last: int
	if index < 0 || length < 0 || !clm_string_valid(inserted) ||
	   clm_string_measure(text, len(text), CLM_STRING_ENCODING_UTF8, &n) != CLM_BPA_SUCCESS ||
	   int(index) > n || int(length) > n-int(index) { return }
	clm_string_measure(inserted, len(inserted), CLM_STRING_ENCODING_UTF8, &added)
	clm_string_offset(text, len(text), CLM_STRING_ENCODING_UTF8, int(index), &first)
	clm_string_offset(text, len(text), CLM_STRING_ENCODING_UTF8, int(index)+int(length), &last)
	bytes := len(text)-(last-first)
	if len(inserted) > 0x7fff_ffff-bytes { return }
	buffer = clm_string_temp_alloc(bytes+len(inserted)+1)
	if buffer == nil { return }
	copy(buffer, transmute([]u8)text[:first])
	copy(buffer[first:], transmute([]u8)inserted)
	copy(buffer[first+len(inserted):], transmute([]u8)text[last:])
	buffer[len(buffer)-1] = 0
	count = i32(added)
	return
}

/*
Inserts text at a character index; returns inserted character count or failure.
*/
clm_string_insert :: proc(id: idkey, index: i32, inserted: string) -> i32 {
	text, _, ok := clm_string_view(id)
	if !ok { return CLM_BPA_FAILURE }
	buffer, count := clm_string_edit(text, index, 0, inserted)
	if count < 0 { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(buffer)
	if clm_bpa_replace(id, raw_data(buffer), u32(len(buffer))) < 0 { return CLM_BPA_FAILURE }
	return count
}

/*
Appends text and returns the number of characters appended, or CLM_BPA_FAILURE.
Character counts and indices are Unicode code points.
*/
clm_string_append :: proc(id: idkey, suffix: string) -> i32 {
	n := clm_string_length(id)
	if n < 0 { return CLM_BPA_FAILURE }
	return clm_string_insert(id, n, suffix)
}

/*
Appends text followed by a newline; returns appended character count or failure.
*/
clm_string_append_line :: proc(id: idkey, suffix: string) -> i32 {
	if !clm_string_valid(suffix) || len(suffix) >= 0x7fff_ffff { return CLM_BPA_FAILURE }
	buffer := clm_string_temp_alloc(len(suffix)+2)
	if buffer == nil { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(buffer)
	copy(buffer, transmute([]u8)suffix)
	buffer[len(suffix)] = '\n'
	return clm_string_append(id, string(buffer[:len(suffix)+1]))
}

/*
Removes a character range; returns the number removed, or CLM_BPA_FAILURE.
*/
clm_string_remove :: proc(id: idkey, index, length: i32) -> i32 {
	text, n, ok := clm_string_view(id)
	if !ok || index < 0 || length < 0 || int(index) > n || int(length) > n-int(index) { return CLM_BPA_FAILURE }
	if length == 0 { return 0 }
	buffer, count := clm_string_edit(text, index, length, "")
	if count < 0 { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(buffer)
	if clm_bpa_replace(id, raw_data(buffer), u32(len(buffer))) < 0 { return CLM_BPA_FAILURE }
	return length
}

/*
Returns whether a string block contains a valid search string.
*/
clm_string_contains :: proc(id: idkey, search: string) -> bool {
	text, _, ok := clm_string_view(id)
	return ok && clm_string_valid(search) && strings.contains(text, search)
}

/*
Returns string length in Unicode code points, or failure.
*/
clm_string_length :: proc(id: idkey) -> i32 {
	_, n, ok := clm_string_view(id)
	if !ok { return CLM_BPA_FAILURE }
	return i32(n)
}

clm_string_replace_buffer :: proc(text, search, replacement: string) -> (buffer: []u8, count: i32) {
	count = CLM_BPA_FAILURE
	if search == "" || !clm_string_valid(text) || !clm_string_valid(search) || !clm_string_valid(replacement) { return }
	count = 0
	for rest := text; true; {
		i := strings.index(rest, search)
		if i < 0 { break }
		count += 1
		rest = rest[i+len(search):]
	}
	if count == 0 { return }
	growth := len(replacement)-len(search)
	if growth > 0 && int(count) > (0x7fff_ffff-len(text))/growth { count = CLM_BPA_FAILURE; return }
	size := len(text)+int(count)*growth
	buffer = clm_string_temp_alloc(size+1)
	if buffer == nil { count = CLM_BPA_FAILURE; return }
	pos := 0
	rest := text
	for {
		i := strings.index(rest, search)
		if i < 0 { break }
		copy(buffer[pos:], transmute([]u8)rest[:i]); pos += i
		copy(buffer[pos:], transmute([]u8)replacement); pos += len(replacement)
		rest = rest[i+len(search):]
	}
	copy(buffer[pos:], transmute([]u8)rest)
	buffer[size] = 0
	return
}

/*
Replaces every non-overlapping occurrence; returns replacements made or failure.
An empty search string is invalid. The replacement may be empty.
*/
clm_string_replace :: proc(id: idkey, search, replacement: string) -> i32 {
	text, _, ok := clm_string_view(id)
	if !ok { return CLM_BPA_FAILURE }
	buffer, count := clm_string_replace_buffer(text, search, replacement)
	if count <= 0 { return count }
	defer clm_string_temp_free(buffer)
	if clm_bpa_replace(id, raw_data(buffer), u32(len(buffer))) < 0 { return CLM_BPA_FAILURE }
	return count
}

/*
Decodes one validated UTF-8 scalar value.
*/
clm_string_utf8_decode_one :: proc(data: []u8, size: int, offset: ^int, scalar: ^u32) -> i32 {
	if offset == nil || scalar == nil { return CLM_BPA_FAILURE }
	begin := offset^
	end := begin
	if !clm_string_utf8_next(data, size, &end) { return CLM_BPA_FAILURE }
	cp := u32(data[begin])
	if end-begin > 1 {
		cp &= (1 << u32(7-(end-begin)))-1
		for j in begin+1..<end { cp = (cp << 6) | u32(data[j] & 0x3f) }
	}
	scalar^ = cp
	offset^ = end
	return CLM_BPA_SUCCESS
}

/*
Encodes one Unicode scalar as UTF-8. Caller provides 4 writable bytes.
*/
clm_string_utf8_encode_one :: proc(cp: u32, out: []u8) -> int {
	n := 1
	if cp >= 0x80 { n = 2 }
	if cp >= 0x800 { n = 3 }
	if cp >= 0x10000 { n = 4 }
	if cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || len(out) < n { return 0 }
	switch n {
	case 1: out[0] = u8(cp)
	case 2: out[0] = u8(0xc0 | (cp >> 6)); out[1] = u8(0x80 | (cp & 0x3f))
	case 3: out[0] = u8(0xe0 | (cp >> 12)); out[1] = u8(0x80 | ((cp >> 6) & 0x3f)); out[2] = u8(0x80 | (cp & 0x3f))
	case 4: out[0] = u8(0xf0 | (cp >> 18)); out[1] = u8(0x80 | ((cp >> 12) & 0x3f)); out[2] = u8(0x80 | ((cp >> 6) & 0x3f)); out[3] = u8(0x80 | (cp & 0x3f))
	}
	return n
}

// Successful decode returns a pool-owned NUL-terminated buffer and byte length.
// Release with clm_string_temp_free; no output is published on failure.
/*
Convert bytes to validated UTF-8 in an allocated temporary buffer.
Rejects embedded U+0000 because BPA currently stores NUL-terminated strings.
Release the returned buffer with clm_string_temp_free.
*/
clm_string_decode_bytes :: proc(encoding: u8, input: []u8, output: ^[]u8, output_length: ^int) -> i32 {
	if output == nil || output_length == nil || !clm_string_encoding_supported(encoding) ||
	   len(input) > 0x7fff_ffff || (encoding == CLM_STRING_ENCODING_UTF16LE && len(input)%2 != 0) { return CLM_BPA_FAILURE }
	size := len(input)
	allocation := size+1
	if encoding == CLM_STRING_ENCODING_LATIN1 { allocation = size*2+1 }
	if encoding == CLM_STRING_ENCODING_UTF16LE { allocation = (size/2)*3+1 }
	if allocation > 0x8000_0000 { return CLM_BPA_FAILURE }
	result := clm_string_temp_alloc(allocation)
	if result == nil { return CLM_BPA_FAILURE }
	used, off := 0, 0
	for off < size {
		cp: u32
		switch encoding {
		case CLM_STRING_ENCODING_UTF8:
			if clm_string_utf8_decode_one(input, size, &off, &cp) < 0 { clm_string_temp_free(result); return CLM_BPA_FAILURE }
		case CLM_STRING_ENCODING_LATIN1:
			cp = u32(input[off]); off += 1
		case CLM_STRING_ENCODING_UTF16LE:
			cp = u32(input[off]) | (u32(input[off+1]) << 8); off += 2
			if cp >= 0xd800 && cp <= 0xdbff {
				if off+1 >= size { clm_string_temp_free(result); return CLM_BPA_FAILURE }
				tail := u32(input[off]) | (u32(input[off+1]) << 8)
				if tail < 0xdc00 || tail > 0xdfff { clm_string_temp_free(result); return CLM_BPA_FAILURE }
				cp = 0x10000+((cp-0xd800) << 10)+(tail-0xdc00); off += 2
			} else if cp >= 0xdc00 && cp <= 0xdfff { clm_string_temp_free(result); return CLM_BPA_FAILURE }
		}
		if cp == 0 { clm_string_temp_free(result); return CLM_BPA_FAILURE }
		used += clm_string_utf8_encode_one(cp, result[used:])
	}
	result[used] = 0
	output^ = result
	output_length^ = used
	return CLM_BPA_SUCCESS
}

/*
Exports the Unicode text into bytes in its preferred external encoding.
No trailing NUL or BOM is included. Returns byte count, or -1 on failure.
A nil buffer queries the required byte count without writing; len(buffer) is capacity.
Fails without writing when output capacity is insufficient.
*/
clm_string_get_bytes :: proc(id: idkey, buffer: []u8) -> i32 {
	text, _, ok := clm_string_view(id)
	block := clm_bpa_find_block(id)
	if !ok || !clm_string_encoding_supported(block.encoding) { return CLM_BPA_FAILURE }
	required, off := 0, 0
	for off < len(text) {
		cp: u32
		before := off
		if clm_string_utf8_decode_one(transmute([]u8)text, len(text), &off, &cp) < 0 || cp == 0 { return CLM_BPA_FAILURE }
		step := off-before
		if block.encoding == CLM_STRING_ENCODING_LATIN1 {
			if cp > 255 { return CLM_BPA_FAILURE }
			step = 1
		} else if block.encoding == CLM_STRING_ENCODING_UTF16LE {
			step = 2
			if cp >= 0x10000 { step = 4 }
		}
		if required > 0x7fff_ffff-step { return CLM_BPA_FAILURE }
		required += step
	}
	if raw_data(buffer) == nil { return i32(required) }
	if len(buffer) < required { return CLM_BPA_FAILURE }
	if block.encoding == CLM_STRING_ENCODING_UTF8 { copy(buffer, transmute([]u8)text); return i32(required) }
	off = 0
	pos := 0
	for off < len(text) {
		cp: u32
		clm_string_utf8_decode_one(transmute([]u8)text, len(text), &off, &cp)
		if block.encoding == CLM_STRING_ENCODING_LATIN1 { buffer[pos] = u8(cp); pos += 1
		} else if cp < 0x10000 {
			buffer[pos] = u8(cp); buffer[pos+1] = u8(cp >> 8); pos += 2
		} else {
			cp -= 0x10000
			hi, lo := u32(0xd800) | (cp >> 10), u32(0xdc00) | (cp & 0x3ff)
			buffer[pos] = u8(hi); buffer[pos+1] = u8(hi >> 8)
			buffer[pos+2] = u8(lo); buffer[pos+3] = u8(lo >> 8); pos += 4
		}
	}
	return i32(required)
}

/*
Decode external byte sequence into an existing string, or create a new string.
Buffer length is exact (not NUL-terminated). Internal storage remains UTF-8.
If replacing, preserve resource identity; update preferred export encoding.
Fails on invalid UTF-8, UTF-16, embedded U+0000, or allocation failure.
*/
clm_string_from_bytes :: proc(id: idkey, encoding: u8, buffer: []u8) -> i32 {
	converted: []u8
	n: int
	if clm_string_decode_bytes(encoding, buffer, &converted, &n) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(converted)
	block := clm_bpa_find_block(id)
	created := block == nil
	if created {
		if clm_bpa_create(id, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
		block = clm_bpa_find_block(id)
	}
	if block == nil || block.type != CLM_BPA_TYPE_STRING ||
	   clm_bpa_set_string(id, raw_data(converted), u32(n+1)) < 0 {
		if created { clm_bpa_delete(id) }
		return CLM_BPA_FAILURE
	}
	block.encoding = encoding
	return CLM_BPA_SUCCESS
}

/*
Changes the preferred encoding for subsequent byte exports.
Text remains unchanged and stored as UTF-8. Does NOT transcode in place.
Fails if the current text cannot be represented in the new encoding.
*/
clm_string_change_encoding :: proc(id: idkey, new_encoding: u8) -> i32 {
	block := clm_bpa_find_block(id)
	if block == nil || block.type != CLM_BPA_TYPE_STRING || !clm_string_encoding_supported(new_encoding) { return CLM_BPA_FAILURE }
	old := block.encoding
	block.encoding = new_encoding
	if clm_string_get_bytes(id, nil) < 0 { block.encoding = old; return CLM_BPA_FAILURE }
	return CLM_BPA_SUCCESS
}

// Native borrowed views combine C's pointer and byte-length outputs into string.
/*
Borrows a UTF-8 byte view of a Unicode code-point range.
The view is not NUL-terminated at its end and remains valid only while the
source string is live and unmodified. It does not retain the source block.
source_id: ID of the source string.
start: Zero-based first Unicode code point to include.
length: Number of Unicode code points to borrow.
text: Receives a borrowed string; len(text^) is the byte length.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/range.
*/
clm_string_slice_borrow :: proc(source_id: idkey, start, length: i32, text: ^string) -> i32 {
	if text == nil { return CLM_BPA_FAILURE }
	text^ = ""
	value, n, ok := clm_string_view(source_id)
	if !ok || start < 0 || length < 0 || int(start) > n || int(length) > n-int(start) { return CLM_BPA_FAILURE }
	first, last: int
	clm_string_offset(value, len(value), CLM_STRING_ENCODING_UTF8, int(start), &first)
	clm_string_offset(value, len(value), CLM_STRING_ENCODING_UTF8, int(start)+int(length), &last)
	text^ = value[first:last]
	return CLM_BPA_SUCCESS
}

/*
Creates an independent CLM string containing a Unicode code-point slice.
The range is [start, start + length), with strict bounds. A zero-length
range at the end is valid. Output is UTF-8 internally, and inherits the
source's preferred byte-export encoding. The destination ID must be unused.
Returns: CLM_BPA_SUCCESS on success; CLM_BPA_FAILURE otherwise.
*/
clm_string_slice :: proc(source_id: idkey, start, length: i32, destination_id: idkey) -> i32 {
	text: string
	if clm_bpa_is_existing(destination_id) || clm_string_slice_borrow(source_id, start, length, &text) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	preference := clm_bpa_find_block(source_id).encoding
	if !clm_string_new(destination_id, text) { return CLM_BPA_FAILURE }
	if clm_string_change_encoding(destination_id, preference) != CLM_BPA_SUCCESS {
		clm_string_delete(destination_id)
		return CLM_BPA_FAILURE
	}
	return CLM_BPA_SUCCESS
}

clm_string_array_get_node :: proc(id: idkey, array: ^^clm_bpaa_array_node) -> i32 {
	if array == nil { return CLM_BPA_FAILURE }
	found := clm_bpaa_find_array(id)
	if found == nil || found.type != CLM_BPA_TYPE_STRING || found.encoding != CLM_STRING_ENCODING_UTF8 { return CLM_BPA_FAILURE }
	array^ = found
	return CLM_BPA_SUCCESS
}

clm_string_array_validate_block :: proc(array: ^clm_bpaa_array_node, block_id: idkey) -> i32 {
	_, _, ok := clm_string_view(block_id)
	if !ok { return CLM_BPA_FAILURE }
	return CLM_BPA_SUCCESS
}

clm_string_array_replace_bytes :: proc(array: ^clm_bpaa_array_node, index: i32, text: string, byte_length: int) -> i32 {
	if array == nil || index < 0 || u32(index) >= array.count || byte_length < 0 ||
	   byte_length > len(text) || byte_length > 0x7fff_ffff ||
	   !clm_string_valid(text[:byte_length]) { return CLM_BPA_FAILURE }
	buffer := clm_string_temp_alloc(byte_length+1)
	if buffer == nil { return CLM_BPA_FAILURE }
	copy(buffer, transmute([]u8)text[:byte_length])
	buffer[byte_length] = 0
	old := array.items.strings[index]
	clm_bpa_pool_free(rawptr(old), len(string(old))+1)
	array.items.strings[index] = cstring(raw_data(buffer))
	array.deck_size = array.count
	return CLM_BPA_SUCCESS
}

clm_string_array_item :: proc(id: idkey, index: i32) -> (array: ^clm_bpaa_array_node, text: string, ok: bool) {
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS || index < 0 || u32(index) >= array.count { return }
	text = string(array.items.strings[index])
	ok = true
	return
}

/*
Creates a string array; strings use the canonical UTF-8 internal encoding.
*/
clm_string_array_new :: proc(id: idkey, reserved: i32) -> bool {
	return clm_bpaa_create(id, CLM_BPA_TYPE_STRING, reserved, CLM_STRING_ENCODING_UTF8) == CLM_BPA_SUCCESS
}

/*
Returns the indexed string's character count, or CLM_BPA_FAILURE.
*/
clm_string_array_length_at :: proc(id: idkey, index: i32) -> i32 {
	_, text, ok := clm_string_array_item(id, index)
	n: int
	if !ok || clm_string_measure(text, len(text), CLM_STRING_ENCODING_UTF8, &n) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return i32(n)
}

/*
Returns whether the indexed string contains a valid search string.
*/
clm_string_array_contains_at :: proc(id: idkey, index: i32, search: string) -> bool {
	_, text, ok := clm_string_array_item(id, index)
	return ok && clm_string_valid(search) && strings.contains(text, search)
}

/*
Inserts text at a character index in one array string.
*/
clm_string_array_insert_at :: proc(id: idkey, index, character_index: i32, inserted: string) -> i32 {
	array, text, ok := clm_string_array_item(id, index)
	if !ok { return CLM_BPA_FAILURE }
	buffer, count := clm_string_edit(text, character_index, 0, inserted)
	if count < 0 { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(buffer)
	if clm_string_array_replace_bytes(array, index, string(buffer[:len(buffer)-1]), len(buffer)-1) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return count
}

/*
Appends text to one array string; returns appended character count.
*/
clm_string_array_append_at :: proc(id: idkey, index: i32, suffix: string) -> i32 {
	n := clm_string_array_length_at(id, index)
	if n < 0 { return CLM_BPA_FAILURE }
	return clm_string_array_insert_at(id, index, n, suffix)
}

/*
Appends text and a newline to one array string.
*/
clm_string_array_append_line_at :: proc(id: idkey, index: i32, suffix: string) -> i32 {
	if !clm_string_valid(suffix) || len(suffix) >= 0x7fff_ffff { return CLM_BPA_FAILURE }
	buffer := clm_string_temp_alloc(len(suffix)+2)
	if buffer == nil { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(buffer)
	copy(buffer, transmute([]u8)suffix)
	buffer[len(suffix)] = '\n'
	return clm_string_array_append_at(id, index, string(buffer[:len(suffix)+1]))
}

/*
Removes a character range from one array string.
*/
clm_string_array_remove_at :: proc(id: idkey, index, character_index, length: i32) -> i32 {
	array, text, ok := clm_string_array_item(id, index)
	if !ok { return CLM_BPA_FAILURE }
	if length == 0 {
		n: int
		if character_index < 0 ||
		   clm_string_measure(text, len(text), CLM_STRING_ENCODING_UTF8, &n) != CLM_BPA_SUCCESS ||
		   int(character_index) > n { return CLM_BPA_FAILURE }
		return 0
	}
	buffer, count := clm_string_edit(text, character_index, length, "")
	if count < 0 { return CLM_BPA_FAILURE }
	defer clm_string_temp_free(buffer)
	if clm_string_array_replace_bytes(array, index, string(buffer[:len(buffer)-1]), len(buffer)-1) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return length
}

/*
Replaces every non-overlapping match in one array string; returns match count.
*/
clm_string_array_replace_at :: proc(id: idkey, index: i32, search, replacement: string) -> i32 {
	array, text, ok := clm_string_array_item(id, index)
	if !ok { return CLM_BPA_FAILURE }
	buffer, count := clm_string_replace_buffer(text, search, replacement)
	if count <= 0 { return count }
	defer clm_string_temp_free(buffer)
	if clm_string_array_replace_bytes(array, index, string(buffer[:len(buffer)-1]), len(buffer)-1) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return count
}

/*
Deletes a string array without affecting arrays of other item types.
*/
clm_string_array_delete :: proc(id: idkey) -> bool {
	array: ^clm_bpaa_array_node
	return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS && clm_bpaa_delete(id) == CLM_BPA_SUCCESS
}

/*
Returns array item count, or CLM_BPA_FAILURE if missing/too large.
*/
clm_string_array_count :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS || array.count > 0x7fff_ffff { return CLM_BPA_FAILURE }
	return i32(array.count)
}

/*
Copies an array item into an existing string BPA block.
*/
clm_string_array_get :: proc(id: idkey, index: i32, destination_string_id: idkey) -> i32 {
	array, _, ok := clm_string_array_item(id, index)
	destination := clm_bpa_find_block(destination_string_id)
	if !ok || destination == nil || destination.type != CLM_BPA_TYPE_STRING { return CLM_BPA_FAILURE }
	if clm_bpaa_get_string(id, index, destination_string_id) != CLM_BPA_GET_SUCCESS { return CLM_BPA_FAILURE }
	destination.encoding = array.encoding
	return CLM_BPA_GET_SUCCESS
}

/*
Replaces an array item with an existing string BPA block's value.
*/
clm_string_array_set :: proc(id: idkey, index: i32, source_string_id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS || clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_set_string(id, index, source_string_id)
}

/*
Inserts a string block at an array index.
*/
clm_string_array_insert :: proc(id: idkey, index: i32, source_string_id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS || clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_insert_string(id, index, source_string_id)
}

/*
Appends a string block to an array.
*/
clm_string_array_append :: proc(id: idkey, source_string_id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS || clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_append_string(id, source_string_id)
}

/*
Removes an array item by zero-based index.
*/
clm_string_array_remove :: proc(id: idkey, index: i32) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_remove(id, index)
}

/*
Clears array items while preserving its reserved capacity.
*/
clm_string_array_clear :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_clear(id)
}

/*
Finds the first exact string match; returns its index or CLM_BPA_FAILURE.
*/
clm_string_array_find :: proc(id: idkey, source_string_id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	text, _, ok := clm_string_view(source_string_id)
	if !ok || clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	for i in 0..<int(array.count) {
		if string(array.items.strings[i]) == text {
			if i > 0x7fff_ffff { return CLM_BPA_FAILURE }
			return i32(i)
		}
	}
	return CLM_BPA_FAILURE
}

clm_string_array_compare :: proc(left, right: cstring) -> i32 {
	a, b := string(left), string(right)
	if a < b { return -1 }
	if a > b { return 1 }
	return 0
}

/*
Sorts strings lexicographically by their encoded bytes.
*/
clm_string_array_sort :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	// In-place heapsort avoids scratch allocation and quadratic insertion costs.
	items := array.items.strings[:int(array.count)]
	for root := len(items)/2-1; root >= 0; root -= 1 {
		clm_string_array_sift(items, root, len(items))
	}
	for end := len(items)-1; end > 0; end -= 1 {
		items[0], items[end] = items[end], items[0]
		clm_string_array_sift(items, 0, end)
	}
	array.index = 0
	array.deck_size = array.count
	return CLM_BPA_SUCCESS
}

clm_string_array_sift :: proc(items: []cstring, start, end: int) {
	root := start
	for root < end/2 {
		child := root*2+1
		if child >= end { break }
		if child+1 < end && string(items[child]) < string(items[child+1]) { child += 1 }
		if string(items[root]) >= string(items[child]) { break }
		items[root], items[child] = items[child], items[root]
		root = child
	}
}

/*
Randomly shuffles string items and resets the draw deck.
*/
clm_string_array_shuffle :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	for i := int(array.count); i > 1; i -= 1 {
		other := int(rand.uint64()%u64(i))
		array.items.strings[i-1], array.items.strings[other] = array.items.strings[other], array.items.strings[i-1]
	}
	array.index = 0
	array.deck_size = array.count
	return CLM_BPA_SUCCESS
}

/*
Moves the array cursor to the first item.
*/
clm_string_array_move_first :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_move_first(id)
}
/*
Moves the array cursor to the previous item.
*/
clm_string_array_move_previous :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_move_previous(id)
}
/*
Moves the array cursor to the next item.
*/
clm_string_array_move_next :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_move_next(id)
}
/*
Moves the array cursor to the last item.
*/
clm_string_array_move_last :: proc(id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_move_last(id)
}
/*
Moves the array cursor to an item.
*/
clm_string_array_move_to :: proc(id: idkey, index: i32) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_bpaa_move_to(id, index)
}
/*
Copies the current array item into a string BPA block.
*/
clm_string_array_get_current :: proc(id: idkey, destination_string_id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS || array.index > 0x7fff_ffff { return CLM_BPA_FAILURE }
	return clm_string_array_get(id, i32(array.index), destination_string_id)
}
/*
Draws an indexed item from the active deck into a string BPA block.
*/
clm_string_array_draw_at :: proc(id: idkey, deck_index: i32, destination_string_id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS || deck_index < 0 || u32(deck_index) >= array.deck_size { return CLM_BPA_FAILURE }
	if clm_string_array_get(id, deck_index, destination_string_id) != CLM_BPA_GET_SUCCESS { return CLM_BPA_FAILURE }
	last := array.deck_size-1
	array.items.strings[deck_index], array.items.strings[last] = array.items.strings[last], array.items.strings[deck_index]
	array.deck_size -= 1
	return CLM_BPA_GET_SUCCESS
}
/*
Draws the next item, shuffling again when the active deck is exhausted.
*/
clm_string_array_draw :: proc(id: idkey, destination_string_id: idkey) -> i32 {
	array: ^clm_bpaa_array_node
	if clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	if array.deck_size == 0 && clm_string_array_shuffle(id) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return clm_string_array_draw_at(id, 0, destination_string_id)
}

// Numeric convenience APIs extend the C module: strict whole-string decimal
// parsing, explicit output pointers, unchanged outputs on failure. Formatting
// replaces an existing string and returns its character count.
clm_string_from_int :: proc(id: idkey, value: i64) -> i32 {
	buffer: [32]u8
	return clm_string_set(id, strconv.write_int(buffer[:], value, 10))
}
clm_string_from_uint :: proc(id: idkey, value: u64) -> i32 {
	buffer: [32]u8
	return clm_string_set(id, strconv.write_uint(buffer[:], value, 10))
}
clm_string_from_float :: proc(id: idkey, value: f64) -> i32 {
	buffer: [128]u8
	return clm_string_set(id, strconv.write_float(buffer[:], value, 'g', -1, 64))
}
clm_string_to_int :: proc(id: idkey, value: ^i64) -> i32 {
	text, _, ok := clm_string_view(id)
	if !ok || value == nil { return CLM_BPA_FAILURE }
	negative := len(text) > 0 && text[0] == '-'
	digits := text
	if len(digits) > 0 && (digits[0] == '+' || digits[0] == '-') { digits = digits[1:] }
	limit := u64(0x7fff_ffff_ffff_ffff)
	if negative { limit += 1 }
	number, valid := clm_string_parse_decimal(digits, limit)
	if !valid { return CLM_BPA_FAILURE }
	if negative {
		if number == 0x8000_0000_0000_0000 { value^ = -0x7fff_ffff_ffff_ffff-1
		} else { value^ = -i64(number) }
	} else { value^ = i64(number) }
	return CLM_BPA_SUCCESS
}
clm_string_to_uint :: proc(id: idkey, value: ^u64) -> i32 {
	text, _, ok := clm_string_view(id)
	if !ok || value == nil { return CLM_BPA_FAILURE }
	digits := text
	if len(digits) > 0 && digits[0] == '+' { digits = digits[1:] }
	number, valid := clm_string_parse_decimal(digits, 0xffff_ffff_ffff_ffff)
	if !valid { return CLM_BPA_FAILURE }
	value^ = number
	return CLM_BPA_SUCCESS
}
clm_string_parse_decimal :: proc(text: string, limit: u64) -> (number: u64, ok: bool) {
	if text == "" { return }
	for b in transmute([]u8)text {
		if b < '0' || b > '9' { return }
		digit := u64(b-'0')
		if number > (limit-digit)/10 { return }
		number = number*10+digit
	}
	ok = true
	return
}
clm_string_to_float :: proc(id: idkey, value: ^f64) -> i32 {
	text, _, ok := clm_string_view(id)
	if !ok || value == nil { return CLM_BPA_FAILURE }
	number, valid := strconv.parse_f64(text)
	if !valid { return CLM_BPA_FAILURE }
	value^ = number
	return CLM_BPA_SUCCESS
}
