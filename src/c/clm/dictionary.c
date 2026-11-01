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

#include "dictionary.h"

clm_bpa_block_node* clm_typed_dictionary_internal_metadata(
    const clm_id id) {
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRUCT ||
        block->block_size != sizeof(clm_typed_dictionary_metadata) ||
        block->data == NULL)
        return NULL;
    clm_typed_dictionary_metadata metadata;
    memcpy(&metadata, block->data, sizeof(metadata));
    if (metadata.magic != CLM_TYPED_DICTIONARY_METADATA_MAGIC)
        return NULL;
    return block;
}

int clm_typed_dictionary_internal_read(
    const clm_id id,
    clm_typed_dictionary_metadata* metadata) {
    clm_bpa_block_node* block;
    if (metadata == NULL ||
        (block = clm_typed_dictionary_internal_metadata(id)) == NULL)
        return CLM_BPA_FAILURE;
    memcpy(metadata, block->data, sizeof(*metadata));
    return CLM_BPA_SUCCESS;
}

clm_bpaa_array_node* clm_typed_dictionary_internal_array(
    const clm_id id,
    const clm_byte type,
    const clm_uint block_size) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type != type ||
        (type == CLM_BPA_TYPE_STRUCT && array->block_size != block_size))
        return NULL;
    return array;
}

clm_typed_dictionary_slot* clm_typed_dictionary_internal_slots(const clm_bpaa_array_node* array) {
    if (array == NULL || array->type != CLM_BPA_TYPE_STRUCT ||
        array->block_size != sizeof(clm_typed_dictionary_slot) ||
        array->count != array->capacity || array->items.blocks == NULL)
        return NULL;
    return (clm_typed_dictionary_slot*)array->items.blocks;
}

int clm_typed_dictionary_internal_allocate_array_id(
    const clm_id forbidden_a,
    const clm_id forbidden_b,
    clm_id* out_id) {
    const size_t existing_count = clm_bpa_global_arrays_count;
    if (out_id == NULL || existing_count > (size_t)-1 - (size_t)3)
        return CLM_BPA_FAILURE;
    for (size_t attempt = 0; attempt <= existing_count + 2; ++attempt) {
        const clm_id candidate = ~(clm_id)0 - (clm_id)attempt;
        if (candidate != forbidden_a && candidate != forbidden_b &&
            !clm_bpaa_is_existing(candidate)) {
            *out_id = candidate;
            return CLM_BPA_SUCCESS;
        }
    }
    return CLM_BPA_FAILURE;
}

int clm_typed_dictionary_internal_create_slots(
    const clm_id id,
    const clm_uint capacity) {
    clm_typed_dictionary_slot empty = {0, 0, CLM_TYPED_DICTIONARY_SLOT_EMPTY};
    if (capacity == 0 || capacity > INT_MAX ||
        clm_bpaa_create_sized(id, CLM_BPA_TYPE_STRUCT, (clm_int)capacity,
                              sizeof(clm_typed_dictionary_slot), 0) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    for (clm_uint index = 0; index < capacity; ++index) {
        if (clm_bpaa_append_raw(id, &empty, sizeof(empty)) != CLM_BPA_SUCCESS) {
            (void)clm_bpaa_delete(id);
            return CLM_BPA_FAILURE;
        }
    }
    return CLM_BPA_SUCCESS;
}

clm_uint clm_typed_dictionary_internal_find_slot(
    const clm_typed_dictionary_slot* slots,
    const clm_uint capacity,
    const clm_id key,
    bool* found) {
    clm_uint first_deleted = capacity;
    clm_uint index = (clm_uint)(clm_bpa_hash_id(key) & (capacity - 1));
    *found = false;
    for (clm_uint probe = 0; probe < capacity; ++probe) {
        if (slots[index].state == CLM_TYPED_DICTIONARY_SLOT_EMPTY)
            return first_deleted != capacity ? first_deleted : index;
        if (slots[index].state == CLM_TYPED_DICTIONARY_SLOT_OCCUPIED &&
            slots[index].key == key) {
            *found = true;
            return index;
        }
        if (slots[index].state == CLM_TYPED_DICTIONARY_SLOT_DELETED &&
            first_deleted == capacity)
            first_deleted = index;
        index = (index + 1) & (capacity - 1);
    }
    return first_deleted;
}

int clm_typed_dictionary_internal_rehash(
    const clm_id dictionary_id,
    clm_typed_dictionary_metadata* metadata,
    const clm_uint new_capacity) {
    clm_id new_slots_id;
    const clm_uint capacity = new_capacity;

    if (metadata == NULL || capacity < CLM_TYPED_DICTIONARY_INITIAL_CAPACITY ||
        capacity > INT_MAX ||
        (capacity & (capacity - 1)) != 0 ||
        clm_typed_dictionary_internal_allocate_array_id(
            dictionary_id, metadata->values_id, &new_slots_id) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_internal_create_slots(new_slots_id, capacity) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    const clm_bpaa_array_node* old_array = clm_typed_dictionary_internal_array(
        metadata->slots_id, CLM_BPA_TYPE_STRUCT, sizeof(clm_typed_dictionary_slot));
    const clm_typed_dictionary_slot* old_slots = clm_typed_dictionary_internal_slots(old_array);
    const clm_bpaa_array_node* new_array = clm_typed_dictionary_internal_array(
        new_slots_id, CLM_BPA_TYPE_STRUCT, sizeof(clm_typed_dictionary_slot));
    clm_typed_dictionary_slot* new_slots = clm_typed_dictionary_internal_slots(new_array);
    if (old_slots == NULL || new_slots == NULL) {
        (void)clm_bpaa_delete(new_slots_id);
        return CLM_BPA_FAILURE;
    }

    for (clm_uint index = 0; index < old_array->capacity; ++index) {
        if (old_slots[index].state == CLM_TYPED_DICTIONARY_SLOT_OCCUPIED) {
            bool found;
            const clm_uint target = clm_typed_dictionary_internal_find_slot(
                new_slots, new_array->capacity, old_slots[index].key, &found);
            if (found || target == new_array->capacity) {
                (void)clm_bpaa_delete(new_slots_id);
                return CLM_BPA_FAILURE;
            }
            new_slots[target] = old_slots[index];
        }
    }

    const clm_id old_slots_id = metadata->slots_id;
    metadata->slots_id = new_slots_id;
    /* Rehash does not change insertion order or iteration position. */
    clm_bpa_block_node* metadata_block =
        clm_typed_dictionary_internal_metadata(dictionary_id);
    if (metadata_block == NULL) {
        metadata->slots_id = old_slots_id;
        (void)clm_bpaa_delete(new_slots_id);
        return CLM_BPA_FAILURE;
    }
    memcpy(metadata_block->data, metadata, sizeof(*metadata));
    if (clm_bpaa_delete(old_slots_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

int clm_typed_dictionary_internal_prepare_insert(
    const clm_id dictionary_id,
    clm_typed_dictionary_metadata* metadata,
    clm_bpaa_array_node** slots_array,
    clm_typed_dictionary_slot** slots) {
    clm_bpaa_array_node* array = clm_typed_dictionary_internal_array(
        metadata->slots_id, CLM_BPA_TYPE_STRUCT, sizeof(clm_typed_dictionary_slot));
    clm_typed_dictionary_slot* entries = clm_typed_dictionary_internal_slots(array);
    if (array == NULL || entries == NULL)
        return CLM_BPA_FAILURE;

    clm_uint deleted = 0;
    for (clm_uint index = 0; index < array->capacity; ++index)
        if (entries[index].state == CLM_TYPED_DICTIONARY_SLOT_DELETED)
            ++deleted;

    if (metadata->count + deleted + 1 >= array->capacity - array->capacity / 4) {
        clm_uint new_capacity = array->capacity;
        if (metadata->count + 1 >= array->capacity - array->capacity / 4) {
            if (array->capacity > (clm_uint)INT_MAX / 2)
                return CLM_BPA_FAILURE;
            new_capacity *= 2;
        }
        if (clm_typed_dictionary_internal_rehash(dictionary_id, metadata,
                                            new_capacity) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        array = clm_typed_dictionary_internal_array(
            metadata->slots_id, CLM_BPA_TYPE_STRUCT,
            sizeof(clm_typed_dictionary_slot));
        entries = clm_typed_dictionary_internal_slots(array);
        if (array == NULL || entries == NULL)
            return CLM_BPA_FAILURE;
    }
    *slots_array = array;
    *slots = entries;
    return CLM_BPA_SUCCESS;
}

int clm_typed_dictionary_internal_lookup(
    const clm_id dictionary_id,
    const clm_id key,
    clm_typed_dictionary_metadata* metadata,
    clm_bpaa_array_node** slots_array,
    clm_typed_dictionary_slot** slots,
    clm_uint* slot_index,
    bool* found) {
    if (clm_typed_dictionary_internal_read(dictionary_id, metadata) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_bpaa_array_node* keys = clm_typed_dictionary_internal_array(
        metadata->keys_id, CLM_BPA_TYPE_STRUCT, sizeof(clm_id));
    const clm_bpaa_array_node* values = clm_bpaa_find_array(metadata->values_id);
    if (keys == NULL || values == NULL || keys->count != values->count ||
        keys->count != metadata->count)
        return CLM_BPA_FAILURE;
    *slots_array = clm_typed_dictionary_internal_array(
        metadata->slots_id, CLM_BPA_TYPE_STRUCT, sizeof(clm_typed_dictionary_slot));
    *slots = clm_typed_dictionary_internal_slots(*slots_array);
    if (*slots_array == NULL || *slots == NULL)
        return CLM_BPA_FAILURE;
    *slot_index = clm_typed_dictionary_internal_find_slot(
        *slots, (*slots_array)->capacity, key, found);
    return CLM_BPA_SUCCESS;
}

int clm_typed_dictionary_new(
    const clm_id id,
    const clm_byte type,
    const size_t size) {
    clm_typed_dictionary_metadata metadata = {0};
    clm_uint value_size = clm_bpa_type_size(type);
    if (type == CLM_BPA_TYPE_ANY || clm_bpa_is_existing(id) || clm_bpaa_is_existing(id))
        return CLM_BPA_FAILURE;
    if (type == CLM_BPA_TYPE_STRUCT) {
        if (size == 0 || size > UINT_MAX)
            return CLM_BPA_FAILURE;
        value_size = (clm_uint)size;
    } else if (type == CLM_BPA_TYPE_STRING) {
        if (size != 0)
            return CLM_BPA_FAILURE;
        value_size = 0;
    } else if (value_size == 0 || (size != 0 && size != value_size)) {
        return CLM_BPA_FAILURE;
    }

    if (clm_typed_dictionary_internal_allocate_array_id(id, ~(clm_id)0,
                                                   &metadata.slots_id) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_internal_create_slots(
            metadata.slots_id, CLM_TYPED_DICTIONARY_INITIAL_CAPACITY) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_typed_dictionary_internal_allocate_array_id(
            id, metadata.slots_id, &metadata.values_id) != CLM_BPA_SUCCESS ||
        clm_bpaa_create_sized(metadata.values_id, type, 0, value_size, 0) !=
            CLM_BPA_SUCCESS) {
        (void)clm_bpaa_delete(metadata.slots_id);
        return CLM_BPA_FAILURE;
    }

    /* Parallel key array preserves insertion order independently of hash slots. */
    if (clm_typed_dictionary_internal_allocate_array_id(
            id, metadata.values_id, &metadata.keys_id) != CLM_BPA_SUCCESS ||
        clm_bpaa_create_sized(metadata.keys_id, CLM_BPA_TYPE_STRUCT, 0,
                              sizeof(clm_id), 0) != CLM_BPA_SUCCESS) {
        (void)clm_bpaa_delete(metadata.values_id);
        (void)clm_bpaa_delete(metadata.slots_id);
        return CLM_BPA_FAILURE;
    }

    metadata.value_type = type;
    metadata.value_size = value_size;
    metadata.magic = CLM_TYPED_DICTIONARY_METADATA_MAGIC;
    if (clm_bpa_create_sized(id, CLM_BPA_TYPE_STRUCT,
                             sizeof(metadata)) != CLM_BPA_SUCCESS) {
        (void)clm_bpaa_delete(metadata.keys_id);
        (void)clm_bpaa_delete(metadata.values_id);
        (void)clm_bpaa_delete(metadata.slots_id);
        return CLM_BPA_FAILURE;
    }
    clm_bpa_block_node* metadata_block = clm_bpa_find_block(id);
    if (metadata_block == NULL || metadata_block->data == NULL) {
        (void)clm_bpa_delete(id);
        (void)clm_bpaa_delete(metadata.keys_id);
        (void)clm_bpaa_delete(metadata.values_id);
        (void)clm_bpaa_delete(metadata.slots_id);
        return CLM_BPA_FAILURE;
    }
    memcpy(metadata_block->data, &metadata, sizeof(metadata));
    return CLM_BPA_SUCCESS;
}

int clm_typed_dictionary_delete(const clm_id id) {
    clm_typed_dictionary_metadata metadata;
    int status = CLM_BPA_SUCCESS;
    if (clm_typed_dictionary_internal_read(id, &metadata) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_bpaa_delete(metadata.keys_id) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    if (clm_bpaa_delete(metadata.values_id) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    if (clm_bpaa_delete(metadata.slots_id) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    if (clm_bpa_delete(id) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    return status;
}

int clm_typed_dictionary_set_from(
    const clm_id id,
    const clm_id key,
    const void* value,
    const size_t value_size) {
    clm_typed_dictionary_metadata metadata;
    clm_bpaa_array_node* slots_array;
    clm_typed_dictionary_slot* slots;
    clm_uint slot_index;
    bool found;
    if (value == NULL ||
        clm_typed_dictionary_internal_lookup(id, key, &metadata, &slots_array,
                                       &slots, &slot_index, &found) !=
            CLM_BPA_SUCCESS ||
        (metadata.value_type != CLM_BPA_TYPE_STRING &&
         value_size != metadata.value_size) ||
        value_size > UINT_MAX)
        return CLM_BPA_FAILURE;

    const clm_bpaa_array_node* values_array = clm_typed_dictionary_internal_array(
        metadata.values_id, metadata.value_type, metadata.value_size);
    if (values_array == NULL ||
        (found && slots[slot_index].value_index >= values_array->count))
        return CLM_BPA_FAILURE;
    if (found) {
        const int result = metadata.value_type == CLM_BPA_TYPE_STRING
            ? clm_bpaa_set_string_raw(metadata.values_id,
                                      (clm_int)slots[slot_index].value_index,
                                      value, (clm_uint)value_size)
            : clm_bpaa_set_raw(metadata.values_id,
                               (clm_int)slots[slot_index].value_index,
                               value, (clm_uint)value_size);
        return result < 0 ? CLM_BPA_FAILURE : CLM_BPA_SET_SUCCESS;
    }
    if (metadata.count >= INT_MAX ||
        clm_typed_dictionary_internal_prepare_insert(id, &metadata, &slots_array,
                                                &slots) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    slot_index = clm_typed_dictionary_internal_find_slot(
        slots, slots_array->capacity, key, &found);
    if (found || slot_index >= slots_array->capacity)
        return CLM_BPA_FAILURE;

    values_array = clm_typed_dictionary_internal_array(
        metadata.values_id, metadata.value_type, metadata.value_size);
    if (values_array == NULL || values_array->count >= INT_MAX)
        return CLM_BPA_FAILURE;
    const clm_uint value_index = values_array->count;
    if (clm_bpaa_append_raw(metadata.keys_id, &key, sizeof(key)) !=
        CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_bpaa_append_raw(metadata.values_id, value, (clm_uint)value_size) !=
        CLM_BPA_SUCCESS) {
        (void)clm_bpaa_remove(metadata.keys_id, (clm_int)value_index);
        return CLM_BPA_FAILURE;
    }
    slots[slot_index].key = key;
    slots[slot_index].value_index = value_index;
    slots[slot_index].state = CLM_TYPED_DICTIONARY_SLOT_OCCUPIED;
    ++metadata.count;
    metadata.iterator = 0;
    clm_bpa_block_node* metadata_block = clm_typed_dictionary_internal_metadata(id);
    if (metadata_block == NULL) {
        (void)clm_bpaa_remove(metadata.values_id,
                              (clm_int)value_index);
        (void)clm_bpaa_remove(metadata.keys_id, (clm_int)value_index);
        slots[slot_index].state = CLM_TYPED_DICTIONARY_SLOT_DELETED;
        return CLM_BPA_FAILURE;
    }
    memcpy(metadata_block->data, &metadata, sizeof(metadata));
    return CLM_BPA_SET_SUCCESS;
}

int clm_typed_dictionary_set(
    const clm_id id,
    const clm_id key,
    const clm_id source_block_id) {
    clm_typed_dictionary_metadata metadata;
    const clm_bpa_block_node* source = clm_bpa_find_block(source_block_id);
    if (source == NULL ||
        (source->type != CLM_BPA_TYPE_STRING && source->data == NULL) ||
        clm_typed_dictionary_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        source->type != metadata.value_type ||
        (source->type != CLM_BPA_TYPE_STRING &&
         source->block_size != metadata.value_size))
        return CLM_BPA_FAILURE;
    return clm_typed_dictionary_set_from(
        id, key, source->data != NULL ? source->data : "",
        source->type == CLM_BPA_TYPE_STRING
            ? (size_t)source->length + 1 : source->block_size);
}

int clm_typed_dictionary_get_to(
    const clm_id id,
    const clm_id key,
    void* buffer,
    const size_t buffer_size) {
    clm_typed_dictionary_metadata metadata;
    clm_bpaa_array_node* slots_array;
    clm_typed_dictionary_slot* slots;
    clm_uint slot_index;
    bool found;
    if (buffer == NULL ||
        clm_typed_dictionary_internal_lookup(id, key, &metadata, &slots_array,
                                       &slots, &slot_index, &found) !=
            CLM_BPA_SUCCESS ||
        !found)
        return CLM_BPA_FAILURE;
    const clm_bpaa_array_node* values_array = clm_typed_dictionary_internal_array(
        metadata.values_id, metadata.value_type, metadata.value_size);
    if (values_array == NULL || slots[slot_index].value_index >= values_array->count ||
        buffer_size > UINT_MAX)
        return CLM_BPA_FAILURE;
    const int result = metadata.value_type == CLM_BPA_TYPE_STRING
        ? clm_bpaa_get_string_raw(metadata.values_id,
                                  (clm_int)slots[slot_index].value_index,
                                  buffer, (clm_uint)buffer_size)
        : clm_bpaa_get_raw(metadata.values_id,
                           (clm_int)slots[slot_index].value_index,
                           buffer, (clm_uint)buffer_size);
    return result < 0 ? CLM_BPA_FAILURE : CLM_BPA_GET_SUCCESS;
}

int clm_typed_dictionary_get(
    const clm_id id,
    const clm_id key,
    const clm_id destination_block_id) {
    clm_typed_dictionary_metadata metadata;
    clm_bpaa_array_node* slots_array;
    clm_typed_dictionary_slot* slots;
    clm_uint slot_index;
    bool found;
    const clm_bpa_block_node* destination =
        clm_bpa_find_block(destination_block_id);
    if (destination == NULL ||
        clm_typed_dictionary_internal_lookup(id, key, &metadata, &slots_array,
                                       &slots, &slot_index, &found) !=
            CLM_BPA_SUCCESS ||
        !found || destination->type != metadata.value_type ||
        (metadata.value_type != CLM_BPA_TYPE_STRING &&
         destination->block_size != metadata.value_size))
        return CLM_BPA_FAILURE;
    const clm_bpaa_array_node* values_array = clm_typed_dictionary_internal_array(
        metadata.values_id, metadata.value_type, metadata.value_size);
    if (values_array == NULL ||
        slots[slot_index].value_index >= values_array->count)
        return CLM_BPA_FAILURE;
    const int result = metadata.value_type == CLM_BPA_TYPE_STRING
        ? clm_bpaa_get_string(metadata.values_id,
                              (clm_int)slots[slot_index].value_index,
                              destination_block_id)
        : clm_bpaa_get(metadata.values_id,
                       (clm_int)slots[slot_index].value_index,
                       destination_block_id, metadata.value_size);
    return result == CLM_BPA_GET_SUCCESS ? result : CLM_BPA_FAILURE;
}

bool clm_typed_dictionary_contains(const clm_id id, const clm_id key) {
    clm_typed_dictionary_metadata metadata;
    clm_bpaa_array_node* slots_array;
    clm_typed_dictionary_slot* slots;
    clm_uint slot_index;
    bool found;
    return clm_typed_dictionary_internal_lookup(id, key, &metadata, &slots_array,
                                          &slots, &slot_index, &found) ==
               CLM_BPA_SUCCESS &&
           found;
}

clm_int clm_typed_dictionary_count(const clm_id id) {
    clm_typed_dictionary_metadata metadata;
    if (clm_typed_dictionary_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        metadata.count > INT_MAX)
        return CLM_BPA_FAILURE;
    return (clm_int)metadata.count;
}

int clm_typed_dictionary_remove(const clm_id id, const clm_id key) {
    clm_typed_dictionary_metadata metadata;
    clm_bpaa_array_node* slots_array;
    clm_typed_dictionary_slot* slots;
    clm_uint slot_index;
    bool found;
    if (clm_typed_dictionary_internal_lookup(id, key, &metadata, &slots_array,
                                       &slots, &slot_index, &found) !=
            CLM_BPA_SUCCESS ||
        !found)
        return CLM_BPA_FAILURE;
    clm_bpaa_array_node* values_array = clm_typed_dictionary_internal_array(
        metadata.values_id, metadata.value_type, metadata.value_size);
    if (values_array == NULL || slots[slot_index].value_index >= values_array->count ||
        slots[slot_index].value_index > INT_MAX ||
        clm_bpaa_remove(metadata.values_id,
                        (clm_int)slots[slot_index].value_index) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    const clm_uint removed_index = slots[slot_index].value_index;
    if (clm_bpaa_remove(metadata.keys_id, (clm_int)removed_index) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    slots[slot_index].state = CLM_TYPED_DICTIONARY_SLOT_DELETED;
    --metadata.count;
    metadata.iterator = 0;
    for (clm_uint index = 0; index < slots_array->capacity; ++index) {
        if (slots[index].state == CLM_TYPED_DICTIONARY_SLOT_OCCUPIED &&
            slots[index].value_index > removed_index)
            --slots[index].value_index;
    }
    clm_bpa_block_node* metadata_block = clm_typed_dictionary_internal_metadata(id);
    if (metadata_block == NULL)
        return CLM_BPA_FAILURE;
    memcpy(metadata_block->data, &metadata, sizeof(metadata));
    return CLM_BPA_SUCCESS;
}

int clm_typed_dictionary_iterate_reset(const clm_id id) {
    clm_typed_dictionary_metadata metadata;
    if (clm_typed_dictionary_internal_read(id, &metadata) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    metadata.iterator = 0;
    clm_bpa_block_node* metadata_block = clm_typed_dictionary_internal_metadata(id);
    if (metadata_block == NULL)
        return CLM_BPA_FAILURE;
    memcpy(metadata_block->data, &metadata, sizeof(metadata));
    return CLM_BPA_SUCCESS;
}

int clm_typed_dictionary_iterate_next(
    const clm_id id,
    clm_id* key) {
    clm_typed_dictionary_metadata metadata;
    if (key == NULL ||
        clm_typed_dictionary_internal_read(id, &metadata) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_bpaa_array_node* keys = clm_typed_dictionary_internal_array(
        metadata.keys_id, CLM_BPA_TYPE_STRUCT, sizeof(clm_id));
    if (keys == NULL || keys->count != metadata.count ||
        metadata.iterator >= metadata.count)
        return CLM_BPA_FAILURE;
    if (clm_bpaa_get_raw(metadata.keys_id, (clm_int)metadata.iterator,
                         key, sizeof(*key)) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    ++metadata.iterator;
    clm_bpa_block_node* block = clm_typed_dictionary_internal_metadata(id);
    if (block == NULL)
        return CLM_BPA_FAILURE;
    memcpy(block->data, &metadata, sizeof(metadata));
    return CLM_BPA_GET_SUCCESS;
}

int clm_typed_dictionary_clear(const clm_id id) {
    clm_typed_dictionary_metadata metadata;
    if (clm_typed_dictionary_internal_read(id, &metadata) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    clm_bpaa_array_node* slots_array = clm_typed_dictionary_internal_array(
        metadata.slots_id, CLM_BPA_TYPE_STRUCT, sizeof(clm_typed_dictionary_slot));
    clm_typed_dictionary_slot* slots = clm_typed_dictionary_internal_slots(slots_array);
    if (slots == NULL || clm_bpaa_clear(metadata.values_id) != CLM_BPA_SUCCESS ||
        clm_bpaa_clear(metadata.keys_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    memset(slots, 0, (size_t)slots_array->capacity * sizeof(*slots));
    metadata.count = 0;
    metadata.iterator = 0;
    clm_bpa_block_node* block = clm_typed_dictionary_internal_metadata(id);
    if (block == NULL)
        return CLM_BPA_FAILURE;
    memcpy(block->data, &metadata, sizeof(metadata));
    return CLM_BPA_SUCCESS;
}

int clm_typed_dictionary_add_from(
    const clm_id id, const clm_id key, const void* value, const size_t size) {
    clm_typed_dictionary_metadata meta;
    if (clm_typed_dictionary_internal_read(id, &meta) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_contains(id, key))
        return CLM_BPA_FAILURE;
    return clm_typed_dictionary_set_from(id, key, value, size);
}

int clm_typed_dictionary_update_from(
    const clm_id id, const clm_id key, const void* value, const size_t size) {
    if (!clm_typed_dictionary_contains(id, key))
        return CLM_BPA_FAILURE;
    return clm_typed_dictionary_set_from(id, key, value, size);
}

int clm_typed_dictionary_keys_to(
    const clm_id id, clm_id* buffer, const size_t capacity) {
    clm_typed_dictionary_metadata meta;
    if (clm_typed_dictionary_internal_read(id, &meta) != CLM_BPA_SUCCESS ||
        capacity < meta.count || (meta.count != 0 && buffer == NULL))
        return CLM_BPA_FAILURE;
    for (clm_uint i = 0; i < meta.count; ++i)
        if (clm_bpaa_get_raw(meta.keys_id, (clm_int)i,
                             &buffer[i], sizeof(clm_id)) != CLM_BPA_GET_SUCCESS)
            return CLM_BPA_FAILURE;
    return CLM_BPA_GET_SUCCESS;
}

int clm_dictionary_internal_read(
    clm_id id, clm_dictionary_any_metadata* out) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (!out || !block || block->type != CLM_BPA_TYPE_STRUCT ||
        block->block_size != sizeof(*out) || !block->data) return CLM_BPA_FAILURE;
    memcpy(out, block->data, sizeof(*out));
    return out->magic == CLM_DICTIONARY_METADATA_MAGIC ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

int clm_dictionary_internal_unused(clm_id avoid, clm_id* out) {
    if (!out) return CLM_BPA_FAILURE;
    /* Use both BPA namespaces and skip the public dictionary ID. */
    for (clm_id candidate = ~(clm_id)0; candidate != 0; --candidate) {
        if (candidate != avoid && !clm_bpa_is_existing(candidate) &&
            !clm_bpaa_is_existing(candidate)) { *out = candidate; return CLM_BPA_SUCCESS; }
    }
    return CLM_BPA_FAILURE;
}

int clm_dictionary_internal_ref(
    clm_id id, clm_id key, clm_dictionary_value_ref* value) {
    clm_dictionary_any_metadata meta;
    return clm_dictionary_internal_read(id, &meta) == CLM_BPA_SUCCESS &&
        clm_typed_dictionary_get_to(meta.index_id, key, value, sizeof(*value)) == CLM_BPA_GET_SUCCESS &&
        clm_bpa_find_block(value->value_id) != NULL ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

int clm_dictionary_new(clm_id id) {
    clm_id index_id;
    if (clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) ||
        clm_dictionary_internal_unused(id, &index_id) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_new(index_id, CLM_BPA_TYPE_STRUCT,
                                 sizeof(clm_dictionary_value_ref)) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_bpa_create_sized(id, CLM_BPA_TYPE_STRUCT,
                             sizeof(clm_dictionary_any_metadata)) != CLM_BPA_SUCCESS) {
        (void)clm_typed_dictionary_delete(index_id); return CLM_BPA_FAILURE;
    }
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (!block || !block->data) {
        (void)clm_bpa_delete(id); (void)clm_typed_dictionary_delete(index_id);
        return CLM_BPA_FAILURE;
    }
    const clm_dictionary_any_metadata meta = {CLM_DICTIONARY_METADATA_MAGIC, index_id};
    memcpy(block->data, &meta, sizeof(meta));
    return CLM_BPA_SUCCESS;
}

int clm_dictionary_internal_assign(clm_id id, clm_id key,
    clm_id value_id, bool owned, int policy) {
    clm_dictionary_any_metadata meta;
    clm_dictionary_value_ref old;
    if (clm_dictionary_internal_read(id, &meta) != CLM_BPA_SUCCESS ||
        !clm_bpa_find_block(value_id) || value_id == id || value_id == meta.index_id)
        return CLM_BPA_FAILURE;
    const bool present = clm_typed_dictionary_contains(meta.index_id, key);
    if ((policy == 1 && present) || (policy == 2 && !present)) return CLM_BPA_FAILURE;
    if (present && clm_dictionary_internal_ref(id, key, &old) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_dictionary_value_ref next = {value_id, owned ? 1 : 0, {0}};
    if (clm_typed_dictionary_set_from(meta.index_id, key, &next,
                                     sizeof(next)) != CLM_BPA_SET_SUCCESS)
        return CLM_BPA_FAILURE;
    /* A previously owned value cannot be dropped while still referenced by
       this entry; replacing with its own ID simply updates ownership. */
    if (present && old.owned && old.value_id != value_id)
        (void)clm_bpa_delete(old.value_id);
    return CLM_BPA_SET_SUCCESS;
}

int clm_dictionary_set(clm_id id, clm_id key, clm_id source_block_id) {
    return clm_dictionary_internal_assign(id, key, source_block_id, false, 0);
}

int clm_dictionary_add(clm_id id, clm_id key, clm_id source_block_id) {
    return clm_dictionary_internal_assign(id, key, source_block_id, false, 1);
}

int clm_dictionary_update(clm_id id, clm_id key, clm_id source_block_id) {
    return clm_dictionary_internal_assign(id, key, source_block_id, false, 2);
}

int clm_dictionary_set_from(clm_id id, clm_id key,
    clm_byte type, const void* value, size_t size) {
    clm_dictionary_any_metadata meta;
    clm_id new_id;
    if (!value || type == CLM_BPA_TYPE_ANY ||
        clm_dictionary_internal_read(id, &meta) != CLM_BPA_SUCCESS ||
        clm_dictionary_internal_unused(id, &new_id) != CLM_BPA_SUCCESS ||
        (type == CLM_BPA_TYPE_STRING && (size == 0 || size > UINT_MAX ||
            ((const char*)value)[size - 1] != '\0')) ||
        (type != CLM_BPA_TYPE_STRING &&
         (size == 0 || size > UINT_MAX ||
          (type != CLM_BPA_TYPE_STRUCT && clm_bpa_type_size(type) != size))))
        return CLM_BPA_FAILURE;
    const int create_status = type == CLM_BPA_TYPE_STRING
        ? clm_bpa_create(new_id, type)
        : clm_bpa_create_sized(new_id, type, (clm_uint)size);
    if (create_status != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    clm_bpa_block_node* block = clm_bpa_find_block(new_id);
    int status = CLM_BPA_FAILURE;
    if (block) {
        if (type == CLM_BPA_TYPE_STRING) {
            if (clm_bpa_set_string(new_id, (const char*)value, (clm_uint)size) == (int)size - 1)
                status = CLM_BPA_SUCCESS;
        } else if (block->data && block->block_size >= size) {
            memcpy(block->data, value, size); status = CLM_BPA_SUCCESS;
        }
    }
    if (status == CLM_BPA_SUCCESS)
        status = clm_dictionary_internal_assign(id, key, new_id, true, 0);
    if (status != CLM_BPA_SET_SUCCESS) {
        (void)clm_bpa_delete(new_id); return CLM_BPA_FAILURE;
    }
    return status;
}

int clm_dictionary_get_id(clm_id id, clm_id key, clm_id* out) {
    clm_dictionary_value_ref value;
    if (!out || clm_dictionary_internal_ref(id, key, &value) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    *out = value.value_id;
    return CLM_BPA_GET_SUCCESS;
}

int clm_dictionary_get_type(clm_id id, clm_id key, clm_byte* out) {
    clm_id value_id;
    if (!out || clm_dictionary_get_id(id, key, &value_id) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_bpa_block_node* block = clm_bpa_find_block(value_id);
    if (!block) return CLM_BPA_FAILURE;
    *out = block->type;
    return CLM_BPA_GET_SUCCESS;
}

int clm_dictionary_get_to(clm_id id, clm_id key, void* buffer, size_t capacity) {
    clm_id value_id;
    if (!buffer || clm_dictionary_get_id(id, key, &value_id) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_bpa_block_node* block = clm_bpa_find_block(value_id);
    if (!block || !block->data) return CLM_BPA_FAILURE;
    const size_t count = block->type == CLM_BPA_TYPE_STRING
        ? (size_t)block->length + 1 : block->block_size;
    if (capacity < count) return CLM_BPA_FAILURE;
    memcpy(buffer, block->data, count);
    return CLM_BPA_GET_SUCCESS;
}

int clm_dictionary_get(clm_id id, clm_id key, clm_id destination_id) {
    clm_id value_id;
    if (clm_dictionary_get_id(id, key, &value_id) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_bpa_block_node* source = clm_bpa_find_block(value_id);
    clm_bpa_block_node* dest = clm_bpa_find_block(destination_id);
    if (!source || !dest || source->type != dest->type) return CLM_BPA_FAILURE;
    if (source->type == CLM_BPA_TYPE_STRING) {
        if (source->length >= UINT_MAX) return CLM_BPA_FAILURE;
        return clm_bpa_set_string(destination_id,
            source->data ? (const char*)source->data : "",
            (clm_uint)source->length + 1) == (int)source->length
            ? CLM_BPA_GET_SUCCESS : CLM_BPA_FAILURE;
    }
    if (!source->data || !dest->data || source->block_size != dest->block_size)
        return CLM_BPA_FAILURE;
    memcpy(dest->data, source->data, source->block_size);
    return CLM_BPA_GET_SUCCESS;
}

bool clm_dictionary_contains(clm_id id, clm_id key) {
    clm_dictionary_any_metadata meta;
    return clm_dictionary_internal_read(id, &meta) == CLM_BPA_SUCCESS &&
        clm_typed_dictionary_contains(meta.index_id, key);
}

clm_int clm_dictionary_count(clm_id id) {
    clm_dictionary_any_metadata meta;
    return clm_dictionary_internal_read(id, &meta) == CLM_BPA_SUCCESS
        ? clm_typed_dictionary_count(meta.index_id) : CLM_BPA_FAILURE;
}

int clm_dictionary_remove(clm_id id, clm_id key) {
    clm_dictionary_any_metadata meta;
    clm_dictionary_value_ref value;
    if (clm_dictionary_internal_read(id, &meta) != CLM_BPA_SUCCESS ||
        clm_dictionary_internal_ref(id, key, &value) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_remove(meta.index_id, key) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return !value.owned || clm_bpa_delete(value.value_id) == CLM_BPA_SUCCESS
        ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

int clm_dictionary_iterate_reset(clm_id id) {
    clm_dictionary_any_metadata meta;
    return clm_dictionary_internal_read(id, &meta) == CLM_BPA_SUCCESS
        ? clm_typed_dictionary_iterate_reset(meta.index_id) : CLM_BPA_FAILURE;
}

int clm_dictionary_iterate_next(clm_id id, clm_id* key) {
    clm_dictionary_any_metadata meta;
    return clm_dictionary_internal_read(id, &meta) == CLM_BPA_SUCCESS
        ? clm_typed_dictionary_iterate_next(meta.index_id, key) : CLM_BPA_FAILURE;
}

int clm_dictionary_keys_to(clm_id id, clm_id* keys, size_t capacity) {
    clm_dictionary_any_metadata meta;
    return clm_dictionary_internal_read(id, &meta) == CLM_BPA_SUCCESS
        ? clm_typed_dictionary_keys_to(meta.index_id, keys, capacity) : CLM_BPA_FAILURE;
}

int clm_dictionary_clear(clm_id id) {
    clm_dictionary_any_metadata meta;
    if (clm_dictionary_internal_read(id, &meta) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    const clm_int count = clm_typed_dictionary_count(meta.index_id);
    if (count < 0) return CLM_BPA_FAILURE;
    clm_id* keys = count ? (clm_id*)malloc((size_t)count * sizeof(clm_id)) : NULL;
    if (count && !keys) return CLM_BPA_FAILURE;
    if (clm_typed_dictionary_keys_to(meta.index_id, keys, (size_t)count) != CLM_BPA_GET_SUCCESS) {
        free(keys); return CLM_BPA_FAILURE;
    }
    int status = CLM_BPA_SUCCESS;
    for (clm_int i = 0; i < count; ++i)
        if (clm_dictionary_remove(id, keys[i]) != CLM_BPA_SUCCESS)
            status = CLM_BPA_FAILURE;
    free(keys);
    return status;
}

int clm_dictionary_delete(clm_id id) {
    clm_dictionary_any_metadata meta;
    if (clm_dictionary_internal_read(id, &meta) != CLM_BPA_SUCCESS ||
        clm_dictionary_clear(id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const int index_status = clm_typed_dictionary_delete(meta.index_id);
    const int root_status = clm_bpa_delete(id);
    return index_status == CLM_BPA_SUCCESS && root_status == CLM_BPA_SUCCESS
        ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}
