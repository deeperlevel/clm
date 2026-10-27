package clm

import (
	"strings"
	"unicode/utf8"
)

const (
	IDStorageID       IDKey = 0x0f000001
	IDIndexStorageID  IDKey = 0x0f000002
	IDNameBufferID    IDKey = 0x0f000003
	IDNamesStorageID  IDKey = 0x0f000004
	IDValuesStorageID IDKey = 0x0f000005
	IDNotFound        IDKey = IdKeyMax
)

var idCaseSensitive = true
var idGeneratorValue IDKey
var idGeneratorExhausted bool
var idNames = map[string]IDKey{}
var idSpellings = map[string]string{}

func idEnsureStorage() bool {
	if DictionaryCount(IDStorageID) >= 0 && DictionaryCount(IDIndexStorageID) >= 0 && DictionaryCount(IDNamesStorageID) >= 0 && DictionaryCount(IDValuesStorageID) >= 0 && BpaIsExisting(IDNameBufferID) {
		return true
	}
	resources := []IDKey{IDStorageID, IDIndexStorageID, IDNamesStorageID, IDValuesStorageID, IDNameBufferID}
	for _, id := range resources {
		if BpaIsExisting(id) || BpaaIsExisting(id) {
			return false
		}
	}
	for _, id := range resources[:4] {
		kind, size := BpaTypeStruct, 8
		if id == IDNamesStorageID {
			kind, size = BpaTypeString, 0
		}
		if DictionaryNew(id, kind, size) < 0 {
			for _, previous := range resources[:4] {
				DictionaryDelete(previous)
			}
			return false
		}
	}
	if BpaCreate(IDNameBufferID, BpaTypeString) < 0 {
		for _, id := range resources[:4] {
			DictionaryDelete(id)
		}
		return false
	}
	clear(idNames)
	clear(idSpellings)
	return true
}
func idValidName(name string) bool {
	return name != "" && utf8.ValidString(name) && !strings.ContainsRune(name, 0)
}
func idFold(name string, sensitive bool) string {
	if sensitive {
		return name
	}
	data := []byte(name)
	for index, character := range data {
		if character >= 'A' && character <= 'Z' {
			data[index] = character + ('a' - 'A')
		}
	}
	return string(data)
}
func IDRegister(name string, id IDKey) int {
	if !idValidName(name) || !idEnsureStorage() {
		return BpaFailure
	}
	key := idFold(name, idCaseSensitive)
	if _, exists := idNames[key]; exists {
		return BpaFailure
	}
	idNames[key] = id
	idSpellings[key] = name
	return BpaSetSuccess
}
func IDMap(name string, id IDKey) int { return IDRegister(name, id) }
func IDTryGet(name string) (IDKey, int) {
	if !idValidName(name) || !idEnsureStorage() {
		return IDNotFound, BpaFailure
	}
	value, found := idNames[idFold(name, idCaseSensitive)]
	if !found {
		return IDNotFound, BpaFailure
	}
	return value, BpaGetSuccess
}
func IDGet(name string) IDKey { value, _ := IDTryGet(name); return value }
func IDSet(name string, id IDKey) int {
	if !idValidName(name) || !idEnsureStorage() {
		return BpaFailure
	}
	key := idFold(name, idCaseSensitive)
	if _, found := idNames[key]; !found {
		return IDRegister(name, id)
	}
	idNames[key] = id
	return BpaSetSuccess
}
func IDRemove(name string) int {
	if _, status := IDTryGet(name); status < 0 {
		return BpaFailure
	}
	key := idFold(name, idCaseSensitive)
	delete(idNames, key)
	delete(idSpellings, key)
	return BpaSuccess
}
func IDUnmapName(name string) int { return IDRemove(name) }
func IDUnmap(id IDKey) int {
	if !idEnsureStorage() {
		return BpaFailure
	}
	status := BpaFailure
	for key, value := range idNames {
		if value == id {
			delete(idNames, key)
			delete(idSpellings, key)
			status = BpaSuccess
		}
	}
	return status
}
func IDIsCaseSensitive() bool { return idCaseSensitive }
func IDSetCaseSensitive(enabled bool) int {
	if !idEnsureStorage() {
		return BpaFailure
	}
	names := map[string]IDKey{}
	spellings := map[string]string{}
	for key, name := range idSpellings {
		folded := idFold(name, enabled)
		if _, found := names[folded]; found {
			return BpaFailure
		}
		names[folded] = idNames[key]
		spellings[folded] = name
	}
	idNames, idSpellings = names, spellings
	idCaseSensitive = enabled
	return BpaSuccess
}
func IDClear() int {
	if !idEnsureStorage() {
		return BpaFailure
	}
	clear(idNames)
	clear(idSpellings)
	return BpaSuccess
}
func IDShutdown() int {
	if !idEnsureStorage() {
		return BpaFailure
	}
	for _, id := range []IDKey{IDStorageID, IDIndexStorageID, IDNamesStorageID, IDValuesStorageID} {
		DictionaryDelete(id)
	}
	BpaDelete(IDNameBufferID)
	clear(idNames)
	clear(idSpellings)
	return BpaSuccess
}
func IDGenReset() { idGeneratorValue = 0; idGeneratorExhausted = false }
func IDGenNext() IDKey {
	if idGeneratorExhausted {
		return IDNotFound
	}
	value := idGeneratorValue
	if value == IDNotFound-1 {
		idGeneratorExhausted = true
	} else {
		idGeneratorValue++
	}
	return value
}
