module tests.dictionary_test;

import clm.bpa;
import clm.dictionary;
import tests.test;

int clmDictionaryTests()
{
    ClmTestResult result;
    clmTestBegin("CLM - Dictionary");

    enum ulong dictionaryId = 71000;
    enum ulong sourceId = 71001;
    enum ulong destinationId = 71002;
    enum ulong stringSourceId = 71004;
    int value;
    int sourceValue = 9876;
    ulong iteratedKey;
    int iteratedCount;
    ubyte[] fetched;

    clmAssert(result, clm_dictionary_new(dictionaryId, CLM_BPA_TYPE_INT, 0) == CLM_BPA_SUCCESS,
        "create integer dictionary");
    clmAssert(result, clm_dictionary_count(dictionaryId) == 0, "new dictionary starts empty");
    clmAssert(result, clm_dictionary_new(dictionaryId, CLM_BPA_TYPE_INT, 0) == CLM_BPA_FAILURE,
        "reject duplicate dictionary ID");
    clmAssert(result, clm_dictionary_new(71003, CLM_BPA_TYPE_STRING, 1) == CLM_BPA_FAILURE,
        "reject invalid string dictionary size");
    clmAssert(result, clm_bpa_create_sized(71040, CLM_BPA_TYPE_STRUCT, ulong.sizeof) == CLM_BPA_SUCCESS &&
        clm_dictionary_count(71040) == CLM_BPA_FAILURE &&
        clm_dictionary_delete(71040) == CLM_BPA_FAILURE &&
        clm_bpa_delete(71040) == CLM_BPA_SUCCESS,
        "do not mistake an unrelated struct block for a dictionary");

    bool insertedAll = true;
    foreach (index; 0 .. 40)
    {
        int item = index * 10;
        if (clm_dictionary_set_from(dictionaryId, 1000 + index, item) != CLM_BPA_SET_SUCCESS)
            insertedAll = false;
    }
    clmAssert(result, insertedAll, "insert entries and grow the hash table");
    clmAssert(result, clm_dictionary_count(dictionaryId) == 40, "count entries after insertion");
    clmAssert(result, !clm_dictionary_contains(dictionaryId, 0), "key zero is absent until inserted");
    clmAssert(result, clm_dictionary_set_from(dictionaryId, 1005, sourceValue) == CLM_BPA_SET_SUCCESS &&
        clm_dictionary_get_to(dictionaryId, 1005, value) == CLM_BPA_GET_SUCCESS &&
        value == sourceValue && clm_dictionary_count(dictionaryId) == 40,
        "replace a value without adding a key");

    clmAssert(result, clm_bpa_create(sourceId, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS &&
        clm_bpa_set(sourceId, (cast(ubyte*)&sourceValue)[0 .. int.sizeof]) == CLM_BPA_SET_SUCCESS &&
        clm_dictionary_set(dictionaryId, 0, sourceId) == CLM_BPA_SET_SUCCESS,
        "set a dictionary value from a BPA block");
    clmAssert(result, clm_dictionary_get_to(dictionaryId, 0, value) == CLM_BPA_GET_SUCCESS &&
        value == sourceValue && clm_dictionary_count(dictionaryId) == 41,
        "retrieve a value under key zero and count it as a distinct entry");
    clmAssert(result, clm_bpa_create(destinationId, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS &&
        clm_dictionary_get(dictionaryId, 0, destinationId) == CLM_BPA_GET_SUCCESS &&
        clm_bpa_get(destinationId, fetched, int.sizeof) == CLM_BPA_GET_SUCCESS &&
        *cast(int*)fetched.ptr == sourceValue,
        "retrieve into a compatible BPA block");

    clmAssert(result, clm_dictionary_remove(dictionaryId, 1005) == CLM_BPA_SUCCESS &&
        !clm_dictionary_contains(dictionaryId, 1005) &&
        clm_dictionary_count(dictionaryId) == 40,
        "remove a key and compact value storage");
    clmAssert(result, clm_dictionary_get_to(dictionaryId, 1005, value) == CLM_BPA_FAILURE,
        "fail retrieval for a missing key");
    enum int newValue = 12345;
    clmAssert(result, clm_dictionary_set_from(dictionaryId, 99999, newValue) == CLM_BPA_SET_SUCCESS &&
        clm_dictionary_count(dictionaryId) == 41,
        "insert after removal into a table with tombstones");

    bool iterationValid = clm_dictionary_iterate_reset(dictionaryId) == CLM_BPA_SUCCESS;
    bool[42] seenKeys;
    while (iterationValid && clm_dictionary_iterate_next(dictionaryId, iteratedKey) == CLM_BPA_GET_SUCCESS)
    {
        int expectedValue;
        size_t seenIndex;
        if (iteratedKey == 0)
        {
            expectedValue = sourceValue;
            seenIndex = 0;
        }
        else if (iteratedKey == 99999)
        {
            expectedValue = newValue;
            seenIndex = 1;
        }
        else if (iteratedKey >= 1000 && iteratedKey < 1040 && iteratedKey != 1005)
        {
            expectedValue = cast(int)(iteratedKey - 1000) * 10;
            seenIndex = cast(size_t)(iteratedKey - 1000) + 2;
        }
        else
        {
            iterationValid = false;
            break;
        }
        if (seenKeys[seenIndex] || !clm_dictionary_contains(dictionaryId, iteratedKey) ||
            clm_dictionary_get_to(dictionaryId, iteratedKey, value) != CLM_BPA_GET_SUCCESS ||
            value != expectedValue)
            iterationValid = false;
        seenKeys[seenIndex] = true;
        ++iteratedCount;
    }
    clmAssert(result, iterationValid && iteratedCount == 41,
        "iterate all present keys and retrieve their values");
    clmAssert(result, clm_dictionary_iterate_reset(dictionaryId) == CLM_BPA_SUCCESS &&
        clm_dictionary_iterate_next(dictionaryId, iteratedKey) == CLM_BPA_GET_SUCCESS,
        "reset dictionary iteration");

    clmAssert(result, clm_bpa_create(stringSourceId, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS &&
        clm_bpa_set_string(stringSourceId, cast(const(ubyte)[])"dictionary value") == 16 &&
        clm_dictionary_new(71010, CLM_BPA_TYPE_STRING, 0) == CLM_BPA_SUCCESS &&
        clm_dictionary_set(71010, 5, stringSourceId) == CLM_BPA_SET_SUCCESS,
        "create a string dictionary and copy a BPA string");
    string text;
    clmAssert(result, clm_dictionary_get_to(71010, 5, text) == CLM_BPA_GET_SUCCESS &&
        text == "dictionary value", "retrieve a string value");
    clmAssert(result, clm_dictionary_set_from(71010, 6, "") == CLM_BPA_SET_SUCCESS &&
        clm_dictionary_get_to(71010, 6, text) == CLM_BPA_GET_SUCCESS &&
        text.length == 0, "store and retrieve an empty string");

    struct TestValue
    {
        int first;
        int second;
    }
    TestValue structValue = TestValue(11, 22);
    TestValue structResult;
    clmAssert(result, clm_dictionary_new(71020, CLM_BPA_TYPE_STRUCT, TestValue.sizeof) == CLM_BPA_SUCCESS &&
        clm_dictionary_set_from(71020, 7, structValue) == CLM_BPA_SET_SUCCESS &&
        clm_dictionary_get_to(71020, 7, structResult) == CLM_BPA_GET_SUCCESS &&
        structValue == structResult, "store and retrieve fixed-size struct values");

    ulong[3] collisionKeys;
    uint collisionBucket;
    size_t collisionCount;
    foreach (ulong candidate; 0 .. 100_000)
    {
        auto bucket = cast(uint)(clm_bpa_hash_id(candidate) & 15);
        if (collisionCount == 0)
            collisionBucket = bucket;
        if (bucket == collisionBucket)
            collisionKeys[collisionCount++] = candidate;
        if (collisionCount == collisionKeys.length)
            break;
    }
    bool collisionsWork = collisionCount == 3 &&
        clm_dictionary_new(71030, CLM_BPA_TYPE_INT, 0) == CLM_BPA_SUCCESS;
    foreach (index; 0 .. 3)
    {
        int item = cast(int)index + 1;
        if (collisionsWork && clm_dictionary_set_from(71030, collisionKeys[index], item) != CLM_BPA_SET_SUCCESS)
            collisionsWork = false;
    }
    clmAssert(result, collisionsWork &&
        clm_dictionary_remove(71030, collisionKeys[0]) == CLM_BPA_SUCCESS &&
        clm_dictionary_get_to(71030, collisionKeys[2], value) == CLM_BPA_GET_SUCCESS &&
        value == 3, "continue probing through a removed colliding key");

    clmAssert(result, clm_dictionary_new(71050, CLM_BPA_TYPE_INT, 0) == CLM_BPA_SUCCESS,
        "create an insertion-ordered dictionary");
    immutable ulong[] orderedKeys = [900, 2, 77, 0];
    bool orderedValid = true;
    foreach (index, key; orderedKeys)
    {
        int item = cast(int)index + 100;
        if (clm_dictionary_add_from(71050, key, item) != CLM_BPA_SET_SUCCESS)
            orderedValid = false;
    }
    clmAssert(result, orderedValid && clm_dictionary_count(71050) == 4,
        "add distinct keys in insertion order");
    clmAssert(result, clm_dictionary_add_from(71050, 2, sourceValue) == CLM_BPA_FAILURE,
        "insert-only rejects an existing key");
    clmAssert(result, clm_dictionary_update_from(71050, 333, sourceValue) == CLM_BPA_FAILURE,
        "update-only rejects an absent key");
    clmAssert(result, clm_dictionary_update_from(71050, 2, sourceValue) == CLM_BPA_SET_SUCCESS,
        "updating an existing key preserves its position");
    ulong[4] copiedKeys;
    clmAssert(result, clm_dictionary_keys_to(71050, copiedKeys) == CLM_BPA_GET_SUCCESS &&
        copiedKeys[] == orderedKeys[], "copy keys in insertion order");
    bool orderMatches = clm_dictionary_iterate_reset(71050) == CLM_BPA_SUCCESS;
    foreach (key; orderedKeys)
        if (orderMatches && (clm_dictionary_iterate_next(71050, iteratedKey) != CLM_BPA_GET_SUCCESS ||
            iteratedKey != key))
            orderMatches = false;
    clmAssert(result, orderMatches &&
        clm_dictionary_iterate_next(71050, iteratedKey) == CLM_BPA_FAILURE,
        "iterator follows insertion order");
    clmAssert(result, clm_dictionary_remove(71050, 2) == CLM_BPA_SUCCESS &&
        clm_dictionary_add_from(71050, 2, sourceValue) == CLM_BPA_SET_SUCCESS,
        "deleting and readding a key moves it to the end");
    immutable ulong[] expectedAfterReadd = [900, 77, 0, 2];
    clmAssert(result, clm_dictionary_keys_to(71050, copiedKeys) == CLM_BPA_GET_SUCCESS &&
        copiedKeys[] == expectedAfterReadd[], "reinserted key comes last");
    clmAssert(result, clm_dictionary_clear(71050) == CLM_BPA_SUCCESS &&
        clm_dictionary_count(71050) == 0 && !clm_dictionary_contains(71050, 900),
        "clear preserves an empty dictionary resource");
    clmAssert(result, clm_dictionary_delete(71050) == CLM_BPA_SUCCESS,
        "delete ordered dictionary");

    clmAssert(result, clm_dictionary_delete(71030) == CLM_BPA_SUCCESS &&
        clm_dictionary_delete(71020) == CLM_BPA_SUCCESS &&
        clm_dictionary_delete(71010) == CLM_BPA_SUCCESS &&
        clm_dictionary_delete(dictionaryId) == CLM_BPA_SUCCESS &&
        clm_dictionary_count(dictionaryId) == CLM_BPA_FAILURE,
        "delete dictionaries and reject access to deleted dictionaries");
    clmAssert(result, clm_bpa_delete(sourceId) == CLM_BPA_SUCCESS &&
        clm_bpa_delete(destinationId) == CLM_BPA_SUCCESS &&
        clm_bpa_delete(stringSourceId) == CLM_BPA_SUCCESS,
        "delete BPA source and destination blocks");

    clmTestSummary(result.passed, result.failed);
    return result.failed;
}
