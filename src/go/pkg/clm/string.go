package clm

import (
	"encoding/binary"
	"slices"
	"strings"
	"unicode/utf16"
	"unicode/utf8"
)

const (
	StringEncodingUTF8    byte = 1
	StringEncodingLatin1  byte = 2
	StringEncodingUTF16LE byte = 3
)

var stringDefaultEncoding = StringEncodingUTF8

func StringEncodingSupported(encoding byte) bool {
	return encoding >= StringEncodingUTF8 && encoding <= StringEncodingUTF16LE
}
func StringSetDefaultEncoding(encoding byte) int {
	if !StringEncodingSupported(encoding) {
		return BpaFailure
	}
	stringDefaultEncoding = encoding
	return BpaSuccess
}
func StringGetDefaultEncoding() byte { return stringDefaultEncoding }
func stringValid(text string) bool   { return utf8.ValidString(text) && !strings.ContainsRune(text, 0) }
func stringBlock(id IDKey) *BpaBlockNode {
	block := bpaBlocks[id]
	if block == nil || block.Type != BpaTypeString || block.Length < 0 || block.Length >= len(block.Data) || !stringValid(string(block.Data[:block.Length])) {
		return nil
	}
	return block
}
func StringValue(id IDKey) (string, int) {
	block := stringBlock(id)
	if block == nil {
		return "", BpaFailure
	}
	return string(block.Data[:block.Length]), BpaGetSuccess
}
func StringNew(id IDKey, text string) bool {
	if !stringValid(text) || BpaCreate(id, BpaTypeString) < 0 {
		return false
	}
	block := bpaBlocks[id]
	block.Encoding = stringDefaultEncoding
	BpaSetString(id, append([]byte(text), 0))
	return true
}
func StringDelete(id IDKey) bool { return stringBlock(id) != nil && BpaDelete(id) == BpaSuccess }
func StringSet(id IDKey, text string) int {
	if stringBlock(id) == nil || !stringValid(text) {
		return BpaFailure
	}
	if BpaSetString(id, append([]byte(text), 0)) < 0 {
		return BpaFailure
	}
	return utf8.RuneCountInString(text)
}
func stringInsert(text string, index int, suffix string) (string, int) {
	if !stringValid(suffix) || index < 0 {
		return "", BpaFailure
	}
	characters := []rune(text)
	if index > len(characters) {
		return "", BpaFailure
	}
	return string(characters[:index]) + suffix + string(characters[index:]), utf8.RuneCountInString(suffix)
}
func stringRemove(text string, index, count int) (string, int) {
	characters := []rune(text)
	if index < 0 || count < 0 || index > len(characters) || count > len(characters)-index {
		return "", BpaFailure
	}
	return string(characters[:index]) + string(characters[index+count:]), count
}
func stringReplace(text, search, replacement string) (string, int) {
	if search == "" || !stringValid(search) || !stringValid(replacement) {
		return "", BpaFailure
	}
	return strings.ReplaceAll(text, search, replacement), strings.Count(text, search)
}
func StringInsert(id IDKey, index int, suffix string) int {
	text, status := StringValue(id)
	if status < 0 {
		return BpaFailure
	}
	value, count := stringInsert(text, index, suffix)
	if count < 0 {
		return count
	}
	StringSet(id, value)
	return count
}
func StringAppend(id IDKey, suffix string) int {
	length := StringLength(id)
	if length < 0 {
		return BpaFailure
	}
	return StringInsert(id, length, suffix)
}
func StringAppendLine(id IDKey, suffix string) int { return StringAppend(id, suffix+"\n") }
func StringRemove(id IDKey, index, count int) int {
	text, status := StringValue(id)
	if status < 0 {
		return BpaFailure
	}
	value, removed := stringRemove(text, index, count)
	if removed < 0 {
		return removed
	}
	StringSet(id, value)
	return removed
}
func StringReplace(id IDKey, search, replacement string) int {
	text, status := StringValue(id)
	if status < 0 {
		return BpaFailure
	}
	value, count := stringReplace(text, search, replacement)
	if count < 0 {
		return count
	}
	StringSet(id, value)
	return count
}
func StringContains(id IDKey, search string) bool {
	text, status := StringValue(id)
	return status >= 0 && stringValid(search) && strings.Contains(text, search)
}
func StringLength(id IDKey) int {
	text, status := StringValue(id)
	if status < 0 {
		return BpaFailure
	}
	return utf8.RuneCountInString(text)
}
func stringEncode(text string, encoding byte) ([]byte, bool) {
	switch encoding {
	case StringEncodingUTF8:
		return []byte(text), true
	case StringEncodingLatin1:
		data := make([]byte, 0, len(text))
		for _, character := range text {
			if character > 255 {
				return nil, false
			}
			data = append(data, byte(character))
		}
		return data, true
	case StringEncodingUTF16LE:
		data := make([]byte, 0, len(text)*2)
		for _, unit := range utf16.Encode([]rune(text)) {
			data = binary.LittleEndian.AppendUint16(data, unit)
		}
		return data, true
	}
	return nil, false
}
func StringGetBytes(id IDKey, output []byte) int {
	text, status := StringValue(id)
	if status < 0 {
		return BpaFailure
	}
	data, valid := stringEncode(text, bpaBlocks[id].Encoding)
	if !valid || (output != nil && len(output) < len(data)) {
		return BpaFailure
	}
	if output != nil {
		copy(output, data)
	}
	return len(data)
}
func StringFromBytes(id IDKey, encoding byte, data []byte) int {
	var text string
	switch encoding {
	case StringEncodingUTF8:
		text = string(data)
	case StringEncodingLatin1:
		characters := make([]rune, len(data))
		for index, value := range data {
			characters[index] = rune(value)
		}
		text = string(characters)
	case StringEncodingUTF16LE:
		if len(data)%2 != 0 {
			return BpaFailure
		}
		characters := make([]rune, 0, len(data)/2)
		for index := 0; index < len(data); index += 2 {
			unit := binary.LittleEndian.Uint16(data[index:])
			if unit >= 0xd800 && unit <= 0xdbff {
				if index+4 > len(data) {
					return BpaFailure
				}
				low := binary.LittleEndian.Uint16(data[index+2:])
				if low < 0xdc00 || low > 0xdfff {
					return BpaFailure
				}
				characters = append(characters, utf16.DecodeRune(rune(unit), rune(low)))
				index += 2
			} else if unit >= 0xdc00 && unit <= 0xdfff {
				return BpaFailure
			} else {
				characters = append(characters, rune(unit))
			}
		}
		text = string(characters)
	default:
		return BpaFailure
	}
	if !stringValid(text) {
		return BpaFailure
	}
	if BpaIsExisting(id) {
		if StringSet(id, text) < 0 {
			return BpaFailure
		}
	} else if !StringNew(id, text) {
		return BpaFailure
	}
	bpaBlocks[id].Encoding = encoding
	return BpaSuccess
}
func StringChangeEncoding(id IDKey, encoding byte) int {
	text, status := StringValue(id)
	if status < 0 {
		return BpaFailure
	}
	if _, valid := stringEncode(text, encoding); !valid {
		return BpaFailure
	}
	bpaBlocks[id].Encoding = encoding
	return BpaSuccess
}
func StringSliceBorrow(id IDKey, start, count int) ([]byte, int) {
	block := stringBlock(id)
	if block == nil || start < 0 || count < 0 {
		return nil, BpaFailure
	}
	text := block.Data[:block.Length]
	offset := 0
	for index := 0; index < start; index++ {
		if offset >= len(text) {
			return nil, BpaFailure
		}
		_, width := utf8.DecodeRune(text[offset:])
		offset += width
	}
	end := offset
	for index := 0; index < count; index++ {
		if end >= len(text) {
			return nil, BpaFailure
		}
		_, width := utf8.DecodeRune(text[end:])
		end += width
	}
	return text[offset:end], BpaSuccess
}
func StringSlice(id IDKey, start, count int, destination IDKey) int {
	data, status := StringSliceBorrow(id, start, count)
	if status < 0 || !StringNew(destination, string(data)) {
		return BpaFailure
	}
	bpaBlocks[destination].Encoding = bpaBlocks[id].Encoding
	return BpaSuccess
}

func stringArray(id IDKey) *BpaaArrayNode {
	array := bpaaArrays[id]
	if array == nil || array.Type != BpaTypeString {
		return nil
	}
	return array
}
func stringArrayValue(id IDKey, index int) (string, bool) {
	array := stringArray(id)
	if !bpaaValid(array, index) || !stringValid(array.Strings[index]) {
		return "", false
	}
	return array.Strings[index], true
}
func StringArrayNew(id IDKey, reserved int) bool {
	return BpaaCreate(id, BpaTypeString, reserved, stringDefaultEncoding) == BpaSuccess
}
func StringArrayDelete(id IDKey) bool { return stringArray(id) != nil && BpaaDelete(id) == BpaSuccess }
func StringArrayCount(id IDKey) int {
	array := stringArray(id)
	if array == nil {
		return BpaFailure
	}
	return array.Count
}
func StringArrayLengthAt(id IDKey, index int) int {
	text, valid := stringArrayValue(id, index)
	if !valid {
		return BpaFailure
	}
	return utf8.RuneCountInString(text)
}
func StringArrayContainsAt(id IDKey, index int, search string) bool {
	text, valid := stringArrayValue(id, index)
	return valid && stringValid(search) && strings.Contains(text, search)
}
func stringArrayEdit(id IDKey, index int, edit func(string) (string, int)) int {
	text, valid := stringArrayValue(id, index)
	if !valid {
		return BpaFailure
	}
	value, count := edit(text)
	if count < 0 {
		return count
	}
	stringArray(id).Strings[index] = value
	stringArray(id).DeckSize = stringArray(id).Count
	return count
}
func StringArrayAppendAt(id IDKey, index int, suffix string) int {
	return stringArrayEdit(id, index, func(text string) (string, int) { return stringInsert(text, utf8.RuneCountInString(text), suffix) })
}
func StringArrayAppendLineAt(id IDKey, index int, suffix string) int {
	return StringArrayAppendAt(id, index, suffix+"\n")
}
func StringArrayInsertAt(id IDKey, index, position int, suffix string) int {
	return stringArrayEdit(id, index, func(text string) (string, int) { return stringInsert(text, position, suffix) })
}
func StringArrayRemoveAt(id IDKey, index, position, count int) int {
	return stringArrayEdit(id, index, func(text string) (string, int) { return stringRemove(text, position, count) })
}
func StringArrayReplaceAt(id IDKey, index int, search, replacement string) int {
	return stringArrayEdit(id, index, func(text string) (string, int) { return stringReplace(text, search, replacement) })
}
func StringArrayGet(id IDKey, index int, destination IDKey) int {
	if _, valid := stringArrayValue(id, index); !valid || stringBlock(destination) == nil {
		return BpaFailure
	}
	return BpaaGetString(id, index, destination)
}
func StringArraySet(id IDKey, index int, source IDKey) int {
	if stringArray(id) == nil || stringBlock(source) == nil {
		return BpaFailure
	}
	return BpaaSetString(id, index, source)
}
func StringArrayInsert(id IDKey, index int, source IDKey) int {
	if stringArray(id) == nil || stringBlock(source) == nil {
		return BpaFailure
	}
	return BpaaInsert(id, index, source, 0)
}
func StringArrayAppend(id, source IDKey) int {
	array := stringArray(id)
	if array == nil {
		return BpaFailure
	}
	return StringArrayInsert(id, array.Count, source)
}
func StringArrayRemove(id IDKey, index int) int {
	if stringArray(id) == nil {
		return BpaFailure
	}
	return BpaaRemove(id, index)
}
func StringArrayClear(id IDKey) int {
	if stringArray(id) == nil {
		return BpaFailure
	}
	return BpaaClear(id)
}
func StringArrayFind(id, source IDKey) int {
	array := stringArray(id)
	text, status := StringValue(source)
	if array == nil || status < 0 {
		return BpaFailure
	}
	return slices.Index(array.Strings[:array.Count], text)
}
func StringArraySort(id IDKey) int {
	array := stringArray(id)
	if array == nil {
		return BpaFailure
	}
	slices.Sort(array.Strings[:array.Count])
	array.Index = 0
	array.DeckSize = array.Count
	return BpaSuccess
}
func StringArrayShuffle(id IDKey) int {
	if stringArray(id) == nil {
		return BpaFailure
	}
	return BpaaShuffle(id)
}
func StringArrayMoveTo(id IDKey, index int) int {
	if stringArray(id) == nil {
		return BpaFailure
	}
	return BpaaMoveTo(id, index)
}
func StringArrayMoveFirst(id IDKey) int { return StringArrayMoveTo(id, 0) }
func StringArrayMoveLast(id IDKey) int {
	array := stringArray(id)
	if array == nil {
		return BpaFailure
	}
	return StringArrayMoveTo(id, array.Count-1)
}
func StringArrayMoveNext(id IDKey) int {
	array := stringArray(id)
	if array == nil {
		return BpaFailure
	}
	return StringArrayMoveTo(id, array.Index+1)
}
func StringArrayMovePrevious(id IDKey) int {
	array := stringArray(id)
	if array == nil {
		return BpaFailure
	}
	return StringArrayMoveTo(id, array.Index-1)
}
func StringArrayGetCurrent(id, destination IDKey) int {
	array := stringArray(id)
	if array == nil {
		return BpaFailure
	}
	return StringArrayGet(id, array.Index, destination)
}
func StringArrayDrawAt(id IDKey, index int, destination IDKey) int {
	if stringArray(id) == nil || stringBlock(destination) == nil {
		return BpaFailure
	}
	return BpaaDrawAt(id, index, destination, 0)
}
func StringArrayDraw(id, destination IDKey) int { return StringArrayDrawAt(id, 0, destination) }
