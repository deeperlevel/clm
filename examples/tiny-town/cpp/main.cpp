#ifndef _WIN32
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

/*
 * CLM Tiny Town: Crabville (C11)
 * ==============================================
 *
 * A deliberately small, deterministic CONSOLE simulation.  It does not contain
 * graphics, network code, a game engine, timers, or random numbers.  Typing
 * "next" advances exactly one simulated day, making identical input files and
 * commands reproducible in every language that implements CLM.
 *
 * WHAT THIS EXAMPLE TEACHES
 * ------------------------
 *   INI  - display preferences and whether to auto-save after each day.
 *   ENV  - a locally loaded .env file with a town-name override and debug flag.
 *   TOML - simulation rules, production rates, and the length of a year.
 *   JSON - persistent state: calendar, resources, and citizen records.
 *   YAML - editable scheduled events that affect the simulated town.
 *
 * Every file is read using clm_file and copied into a clm_string.  The matching
 * clm_string_* parser creates a tree stored in CLM/BPA; clm_tree_get_* and
 * clm_id_try_get retrieve its typed fields.  We then validate and copy a small
 * working snapshot into ordinary C structs.  The native structs are the
 * application's simulation state; the CLM trees are the input/output format.
 *
 * WHY COPY VALUES INTO STRUCTS?
 *   This is a teaching example: simulation rules become easy to inspect and
 *   test without repeatedly looking up keys, and all five formats retain the
 *   same parsing contract.  The original JSON tree is retained separately so
 *   saving can update its known fields without discarding unknown fields.
 *
 * IMPORTANT OWNERSHIP RULE
 *   Parsing a CLM string produces a NEW, owned tree ID; it does not transfer
 *   ownership of the input string.  The application deletes successful trees
 *   with clm_tree_delete().  The input, output, and path strings are also
 *   explicit CLM resources and must be deleted separately.  ID-based storage
 *   does not automatically make concurrent access safe; this example uses
 *   one thread only.
 *
 * This sample targets the CLM parser APIs present in the available C headers.
 * If your local CLM signatures have evolved, adapt the small loading helpers
 * rather than duplicating parsers in the application.
 */

/* Shared registries are defined ONCE in this translation unit.  A larger
 * project should put these switches in one dedicated implementation .c file.
 * All other .c files would include CLM declarations without these switches. */
#define CLM_BPA_IMPLEMENTATION
#define CLM_STRING_IMPLEMENTATION
#define CLM_ID_IMPLEMENTATION
#define CLM_FILE_IMPLEMENTATION

#include "clm/string_ini.hpp"
#include "clm/string_env.hpp"
#include "clm/string_toml.hpp"
#include "clm/string_json.hpp"
#include "clm/string_yaml.hpp"

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOWN_MAX_CITIZENS 32u
#define TOWN_MAX_EVENTS 24u
#define TOWN_NAME_CAP 64u
#define TOWN_JOB_CAP 24u
#define TOWN_PATH_CAP 512u
#define TOWN_COMMAND_CAP 256u

/* Application IDs are deliberately chosen far above CLM's reserved ID range.
 * They are NOT array indexes, pointers, or operating-system handles. */
#define TOWN_INPUT_TEXT_ID UINT64_C(0x71000001)
#define TOWN_OUTPUT_TEXT_ID UINT64_C(0x71000002)
#define TOWN_INI_TREE_ID UINT64_C(0x71000011)
#define TOWN_ENV_TREE_ID UINT64_C(0x71000012)
#define TOWN_TOML_TREE_ID UINT64_C(0x71000013)
#define TOWN_JSON_TREE_ID UINT64_C(0x71000014)
#define TOWN_YAML_TREE_ID UINT64_C(0x71000015)

/** The parser function signature shared by INI, ENV, TOML, JSON, and YAML.
 * It consumes the existing CLM input string by ID and owns a new tree on
 * success.  The format-specific functions remain responsible for syntax. */
typedef clm_int (*town_parser_fn)(clm_id source, clm_id destination);

typedef struct town_citizen {
    char name[TOWN_NAME_CAP];
    char job[TOWN_JOB_CAP];
    clm_id json_object_id; /* Borrowed child of the live JSON tree. */
} town_citizen;

typedef struct town_event {
    char name[TOWN_NAME_CAP];
    int day;
    int food_delta;
    int wood_delta;
    int gold_delta;
} town_event;

typedef struct town_settings {
    char name[TOWN_NAME_CAP];
    bool debug;
    bool verbose_days;
    bool autosave;
    int population_limit;
    int days_per_year;
    int farmer_food;
    int hunter_food;
    int builder_wood;
    int food_per_citizen;
    town_event events[TOWN_MAX_EVENTS];
    size_t event_count;
} town_settings;

typedef struct town_state {
    int year;
    int day;
    int food;
    int wood;
    int gold;
    town_citizen citizens[TOWN_MAX_CITIZENS];
    size_t citizen_count;
} town_state;

typedef struct tiny_town {
    char config_dir[TOWN_PATH_CAP];
    town_settings settings;
    town_state state;
    bool dirty;
} tiny_town;

/** Resolve a parsed property name through CLM's global ID registry.
 *
 * The *_loads_auto_ids() parsers register unknown field names so that we can
 * read human-readable keys without hardcoding numeric IDs into configuration.
 * CLM's key registry is independent of the lifetime of a particular tree.
 *
 * @param name UTF-8 key as it appears in one of the configuration files.
 * @param key Receives the registered CLM ID if found.
 * @return true on success; false when the name is not registered.
 */
static bool town_key(const char* name, clm_id* key)
{
    return name != NULL && key != NULL &&
        clm_id_try_get(name, key) == CLM_BPA_GET_SUCCESS;
}

/** Copy a required string leaf from a parsed object into caller-owned memory.
 *
 * The returned bytes are copied; no pointer into a BPA block escapes this
 * function.  A string result is only accepted if its field type is STRING and
 * its NUL terminator fits inside the supplied destination.
 */
static bool town_get_text(clm_id object, const char* field,
                          char* output, size_t capacity)
{
    clm_id key;
    clm_byte type;
    if (output == NULL || capacity == 0u ||
        !town_key(field, &key) ||
        clm_tree_get_type(object, key, &type) != CLM_BPA_GET_SUCCESS ||
        type != CLM_BPA_TYPE_STRING ||
        clm_tree_get_to(object, key, output, capacity) != CLM_BPA_GET_SUCCESS)
        return false;
    output[capacity - 1u] = '\0';
    return true;
}

/** Read an integer leaf, rejecting floating-point values and unsupported types.
 *
 * JSON/TOML/YAML parsers represent ordinary small integer literals as INT.
 * A LONG is accepted only if it fits in C int; this keeps the simulator's
 * arithmetic bounded and makes overflow checks explicit in town_next().
 */
static bool town_get_int(clm_id object, const char* field, int* result)
{
    clm_id key;
    clm_byte type;
    if (result == NULL || !town_key(field, &key) ||
        clm_tree_get_type(object, key, &type) != CLM_BPA_GET_SUCCESS)
        return false;
    if (type == CLM_BPA_TYPE_INT) {
        clm_int value = 0;
        if (clm_tree_get_to(object, key, &value, sizeof(value)) !=
            CLM_BPA_GET_SUCCESS)
            return false;
        *result = (int)value;
        return true;
    }
    if (type == CLM_BPA_TYPE_LONG) {
        clm_long value = 0;
        if (clm_tree_get_to(object, key, &value, sizeof(value)) !=
            CLM_BPA_GET_SUCCESS || value < INT_MIN || value > INT_MAX)
            return false;
        *result = (int)value;
        return true;
    }
    return false;
}

/** Read a Boolean leaf (TOML/JSON/YAML) or INI/ENV Boolean text.
 * INI and ENV intentionally store strings, even when they look like booleans.
 * We accept exactly "true", "false", "1", or "0" in those formats.
 */
static bool town_get_bool(clm_id object, const char* field, bool* result)
{
    clm_id key;
    clm_byte type;
    char text[16];
    if (result == NULL || !town_key(field, &key) ||
        clm_tree_get_type(object, key, &type) != CLM_BPA_GET_SUCCESS)
        return false;
    if (type == CLM_BPA_TYPE_STRUCT) {
        clm_id leaf_id;
        if (clm_tree_get_id(object, key, &leaf_id) != CLM_BPA_GET_SUCCESS)
            return false;
        return clm_tree_is_boolean(leaf_id, result);
    }
    if (type != CLM_BPA_TYPE_STRING ||
        !town_get_text(object, field, text, sizeof(text)))
        return false;
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0) {
        *result = true;
        return true;
    }
    if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0) {
        *result = false;
        return true;
    }
    return false;
}

/** Get a required child object by its field name. The ID remains owned by the
 * root tree and must NOT be deleted individually while its root is alive. */
static bool town_get_object(clm_id parent, const char* field, clm_id* child)
{
    clm_id key;
    return town_key(field, &key) &&
        clm_tree_get_object(parent, key, child) == CLM_BPA_GET_SUCCESS;
}

/** Get a required child array by field name without transferring ownership. */
static bool town_get_array(clm_id parent, const char* field, clm_id* array)
{
    clm_id key;
    clm_id candidate;
    return town_key(field, &key) &&
        clm_tree_get_id(parent, key, &candidate) == CLM_BPA_GET_SUCCESS &&
        clm_tree_is_array(candidate) && ((*array = candidate), true);
}

/** Resolve a path under the application's configuration directory.
 *
 * All five sample files deliberately live together.  Passing a different
 * directory on the command line allows a second test fixture to be used
 * without modifying global environment variables or recompiling.
 */
static bool town_path(const tiny_town* town, const char* filename,
                      char* output, size_t capacity)
{
    const int used = snprintf(output, capacity, "%s/%s", town->config_dir,
                              filename);
    return used > 0 && (size_t)used < capacity;
}

/** Load one UTF-8 file with CLM file/string APIs, then parse it into a tree.
 *
 * A failed parse leaves no successful output tree to own.  If an intermediate
 * parser version retains a partial tree on failure, clean it up defensively.
 * The input CLM string is reused between formats and is therefore never
 * confused with a JSON/TOML/etc. tree ID.
 *
 * @param town Application containing the directory path.
 * @param filename Basename under that directory.
 * @param tree_id New root ID reserved for the parsed tree.
 * @param parser Matching format's *_loads_auto_ids() implementation.
 * @return true if the file was read and parsed successfully.
 */
static bool town_load_tree(const tiny_town* town, const char* filename,
                           clm_id tree_id, town_parser_fn parser)
{
    char path[TOWN_PATH_CAP + 48u];
    size_t required_capacity = 0u;
    if (!town_path(town, filename, path, sizeof(path)))
        return false;

    /* Use CLM's binary read-to-caller-buffer path, then validate the result as
     * UTF-8 before copying it into an owned CLM string.  The example files are
     * UTF-8 without a byte-order mark, and embedded NUL is forbidden.
     *
     * This two-step method is intentional.  In the available file.h snapshot,
     * the text decoding path has a memory-allocator mismatch: it calls the
     * BPA pool free helper on memory allocated by malloc.  Until that CLM
     * bug is fixed, the binary _to path has clearer ownership and still uses
     * real CLM file I/O, CLM strings, and the five format-specific parsers.
     *
     * The first call queries the file's exact byte size; the second fetches
     * it.  This is NOT an atomic snapshot if another process changes the file
     * between calls, so we reject mismatched sizes. */
    if (clm_file_read_all_to(path, NULL, 0u,
            &required_capacity) != CLM_FILE_OPERATION_SUCCESS ||
        required_capacity > 1024u * 1024u) {
        fprintf(stderr, "[CLM] Cannot read or size %s\n", path);
        return false;
    }
    char* content = (char*)malloc(required_capacity + 1u);
    size_t actual_size = 0u;
    size_t unicode_characters = 0u;
    if (content == NULL || clm_file_read_all_to(path, content,
            required_capacity, &actual_size) != CLM_FILE_OPERATION_SUCCESS ||
        actual_size != required_capacity ||
        memchr(content, '\0', actual_size) != NULL ||
        (actual_size >= 3u &&
            memcmp(content, "\xEF\xBB\xBF", 3u) == 0) ||
        (content[actual_size] = '\0',
         clm_string_measure(content, actual_size, CLM_STRING_ENCODING_UTF8,
             &unicode_characters) != CLM_BPA_SUCCESS) ||
        clm_string_set(TOWN_INPUT_TEXT_ID, content) < 0) {
        fprintf(stderr, "[CLM] Cannot load valid UTF-8 from %s\n", path);
        free(content);
        return false;
    }
    free(content);
    const size_t byte_count = actual_size;
    if (parser(TOWN_INPUT_TEXT_ID, tree_id) != CLM_FORMAT_DESERIALIZE_SUCCESS) {
        fprintf(stderr, "[CLM] Invalid %s (parsing failed)\n", path);
        if (clm_tree_is_object(tree_id) || clm_tree_is_array(tree_id))
            (void)clm_tree_delete(tree_id);
        return false;
    }
    printf("[CLM] Loaded %-14s (%zu bytes)\n", filename, byte_count);
    return true;
}

/** Validate and import the citizen list and resource counters from JSON.
 *
 * The JSON tree remains alive for the whole program.  Each citizen keeps the
 * ID of its JSON object node so "hire" can update that node during save.
 * No other module can silently free those children while they are borrowed.
 */
static bool town_read_state(town_state* state)
{
    clm_id array;
    int year, day, food, wood, gold;
    if (!town_get_int(TOWN_JSON_TREE_ID, "year", &year) ||
        !town_get_int(TOWN_JSON_TREE_ID, "day", &day) ||
        !town_get_int(TOWN_JSON_TREE_ID, "food", &food) ||
        !town_get_int(TOWN_JSON_TREE_ID, "wood", &wood) ||
        !town_get_int(TOWN_JSON_TREE_ID, "gold", &gold) ||
        !town_get_array(TOWN_JSON_TREE_ID, "citizens", &array) ||
        year < 1 || day < 1 || food < 0 || wood < 0 || gold < 0)
        return false;
    const clm_int count = clm_tree_array_count(array);
    if (count < 0 || (size_t)count > TOWN_MAX_CITIZENS)
        return false;
    town_state candidate = {0};
    candidate.year = year;
    candidate.day = day;
    candidate.food = food;
    candidate.wood = wood;
    candidate.gold = gold;
    candidate.citizen_count = (size_t)count;
    for (clm_int i = 0; i < count; ++i) {
        clm_id object_id;
        town_citizen* citizen = &candidate.citizens[i];
        if (clm_tree_array_get_object(array, i, &object_id) !=
                CLM_BPA_GET_SUCCESS ||
            !town_get_text(object_id, "name", citizen->name,
                sizeof(citizen->name)) ||
            !town_get_text(object_id, "job", citizen->job,
                sizeof(citizen->job)) ||
            citizen->name[0] == '\0' || citizen->job[0] == '\0')
            return false;
        citizen->json_object_id = object_id;
        for (clm_int j = 0; j < i; ++j) {
            if (strcmp(candidate.citizens[j].name, citizen->name) == 0)
                return false; /* Names are unique in this tiny simulation. */
        }
    }
    *state = candidate;
    return true;
}

/** Import one event from a YAML array object's typed fields. */
static bool town_read_event(clm_id object, town_event* event)
{
    return town_get_text(object, "name", event->name,
               sizeof(event->name)) &&
        town_get_int(object, "day", &event->day) &&
        town_get_int(object, "food", &event->food_delta) &&
        town_get_int(object, "wood", &event->wood_delta) &&
        town_get_int(object, "gold", &event->gold_delta) &&
        event->name[0] != '\0' && event->day > 0;
}

/** Load all configurable values from their parsed trees, then validate them.
 *
 * Reload passes a temporary settings struct here.  We commit it to the live
 * simulation only after ALL fields and events have passed validation.  That
 * prevents a broken TOML or YAML edit from half-changing production rules.
 */
static bool town_read_settings(town_settings* settings)
{
    clm_id display, simulation, town_section, economy, events;
    town_settings candidate = {0};
    if (!town_get_object(TOWN_INI_TREE_ID, "display", &display) ||
        !town_get_object(TOWN_INI_TREE_ID, "simulation", &simulation) ||
        !town_get_bool(display, "verbose_days", &candidate.verbose_days) ||
        !town_get_bool(simulation, "autosave", &candidate.autosave) ||
        !town_get_text(TOWN_ENV_TREE_ID, "CLM_TOWN_NAME", candidate.name,
            sizeof(candidate.name)) ||
        !town_get_bool(TOWN_ENV_TREE_ID, "CLM_DEBUG", &candidate.debug) ||
        !town_get_object(TOWN_TOML_TREE_ID, "town", &town_section) ||
        !town_get_object(TOWN_TOML_TREE_ID, "economy", &economy) ||
        !town_get_int(town_section, "population_limit",
            &candidate.population_limit) ||
        !town_get_int(town_section, "days_per_year", &candidate.days_per_year) ||
        !town_get_int(economy, "farmer_food", &candidate.farmer_food) ||
        !town_get_int(economy, "hunter_food", &candidate.hunter_food) ||
        !town_get_int(economy, "builder_wood", &candidate.builder_wood) ||
        !town_get_int(economy, "food_per_citizen",
            &candidate.food_per_citizen) ||
        !town_get_array(TOWN_YAML_TREE_ID, "events", &events))
        return false;
    if (candidate.name[0] == '\0' || candidate.population_limit < 1 ||
        candidate.population_limit > (int)TOWN_MAX_CITIZENS ||
        candidate.days_per_year < 2 || candidate.days_per_year > 365 ||
        candidate.farmer_food < 0 || candidate.farmer_food > 1000 ||
        candidate.hunter_food < 0 || candidate.hunter_food > 1000 ||
        candidate.builder_wood < 0 || candidate.builder_wood > 1000 ||
        candidate.food_per_citizen < 0 || candidate.food_per_citizen > 1000)
        return false;

    const clm_int count = clm_tree_array_count(events);
    if (count < 0 || (size_t)count > TOWN_MAX_EVENTS)
        return false;
    candidate.event_count = (size_t)count;
    for (clm_int index = 0; index < count; ++index) {
        clm_id object;
        if (clm_tree_array_get_object(events, index, &object) !=
                CLM_BPA_GET_SUCCESS ||
            !town_read_event(object, &candidate.events[index]) ||
            candidate.events[index].day > candidate.days_per_year)
            return false;
        const town_event* event = &candidate.events[index];
        if (event->food_delta < -100000 || event->food_delta > 100000 ||
            event->wood_delta < -100000 || event->wood_delta > 100000 ||
            event->gold_delta < -100000 || event->gold_delta > 100000)
            return false;
    }
    *settings = candidate;
    return true;
}

/** Remove a successfully loaded tree.  This includes child objects, arrays,
 * and leaf blocks that are owned by that root's CLM tree. */
static void town_dispose_tree(clm_id id)
{
    if (clm_tree_is_object(id) || clm_tree_is_array(id))
        (void)clm_tree_delete(id);
}

/** Reload INI, ENV, TOML, and YAML, preserving running JSON state.
 *
 * This deliberately DOES NOT reload citizens.json: changing production rates
 * should not reset the calendar or undo unsaved citizen job changes.  After
 * successful validation, old configuration trees are no longer needed and
 * can be deleted.  On failure, the active settings remain unchanged.
 */
static bool town_reload(tiny_town* town)
{
    bool ini = false, env = false, toml = false, yaml = false;
    town_settings candidate;
    ini = town_load_tree(town, "settings.ini", TOWN_INI_TREE_ID,
        clm_string_ini_loads_auto_ids);
    if (ini)
        env = town_load_tree(town, ".env", TOWN_ENV_TREE_ID,
            clm_string_env_loads_auto_ids);
    if (env)
        toml = town_load_tree(town, "town.toml", TOWN_TOML_TREE_ID,
            clm_string_toml_loads_auto_ids);
    if (toml)
        yaml = town_load_tree(town, "events.yaml", TOWN_YAML_TREE_ID,
            clm_string_yaml_loads_auto_ids);

    bool valid = ini && env && toml && yaml && town_read_settings(&candidate);
    if (valid && town->state.citizen_count != 0u) {
        valid = town->state.citizen_count <= (size_t)candidate.population_limit &&
            town->state.day <= candidate.days_per_year;
    }
    if (valid)
        town->settings = candidate;
    else
        fprintf(stderr, "[CLM] Reload rejected; current rules remain active.\n");
    town_dispose_tree(TOWN_INI_TREE_ID);
    town_dispose_tree(TOWN_ENV_TREE_ID);
    town_dispose_tree(TOWN_TOML_TREE_ID);
    town_dispose_tree(TOWN_YAML_TREE_ID);
    return valid;
}

/** Update a JSON integer leaf in its existing owned tree.
 *
 * JSON serialization uses the numeric BPA type, not a formatted string.  The
 * field is replaced with an INT value, allowing the real CLM JSON serializer
 * to emit a JSON number rather than quotation marks around the value.
 */
static bool town_set_json_int(clm_id object, const char* field, int value)
{
    clm_id key;
    const clm_int typed_value = (clm_int)value;
    return town_key(field, &key) &&
        clm_tree_set_from(object, key, CLM_BPA_TYPE_INT,
            &typed_value, sizeof(typed_value)) == CLM_BPA_SET_SUCCESS;
}

/** Synchronize the working snapshot into the owned JSON tree, then serialize.
 *
 * The tree retains unknown JSON properties because we mutate the existing
 * fields instead of recreating a reduced JSON object.  The output CLM string
 * receives JSON generated by CLM; no handmade JSON escaping is involved.
 *
 * Note: direct writes are not atomic across crashes.  A production application
 * should write a temporary file, flush it, and replace the destination using
 * an appropriately durable and platform-safe replacement operation.
 */
static bool town_save(tiny_town* town)
{
    char path[TOWN_PATH_CAP + 48u];
    clm_id job_key;
    if (!town_path(town, "citizens.json", path, sizeof(path)) ||
        !town_key("job", &job_key))
        return false;
    if (!town_set_json_int(TOWN_JSON_TREE_ID, "year", town->state.year) ||
        !town_set_json_int(TOWN_JSON_TREE_ID, "day", town->state.day) ||
        !town_set_json_int(TOWN_JSON_TREE_ID, "food", town->state.food) ||
        !town_set_json_int(TOWN_JSON_TREE_ID, "wood", town->state.wood) ||
        !town_set_json_int(TOWN_JSON_TREE_ID, "gold", town->state.gold))
        return false;
    for (size_t i = 0; i < town->state.citizen_count; ++i) {
        const town_citizen* citizen = &town->state.citizens[i];
        if (clm_tree_set_from(citizen->json_object_id, job_key,
                CLM_BPA_TYPE_STRING, citizen->job,
                strlen(citizen->job) + 1u) != CLM_BPA_SET_SUCCESS)
            return false;
    }
    if (clm_string_json_dumps(TOWN_JSON_TREE_ID, TOWN_OUTPUT_TEXT_ID) !=
        CLM_FORMAT_SERIALIZE_SUCCESS)
        return false;
    const clm_bpa_block_node* output = clm_bpa_find_block(TOWN_OUTPUT_TEXT_ID);
    if (output == NULL || output->type != CLM_BPA_TYPE_STRING ||
        clm_file_write_all_text_as(path, CLM_FILE_TEXT_ENCODING_UTF8,
            TOWN_OUTPUT_TEXT_ID, output->length) != CLM_FILE_OPERATION_SUCCESS)
        return false;
    town->dirty = false;
    printf("[CLM] Saved simulation state to %s\n", path);
    return true;
}

/** Saturating resource adjustment for a tiny teaching simulator.
 *
 * Resource balances never become negative.  An oversized positive delta is
 * capped rather than causing signed integer overflow (undefined behavior in
 * C).  This is an application policy, NOT behavior imposed by CLM parsers.
 */
static int town_adjust(int current, int delta)
{
    int64_t total = (int64_t)current + (int64_t)delta;
    if (total < 0)
        return 0;
    if (total > INT_MAX)
        return INT_MAX;
    return (int)total;
}

/** Advance exactly one day, applying production, consumption, then events.
 *
 * The order is intentionally fixed so running the same scenario in C, Python,
 * Go, or Assembly can produce the same observable state.  Events recur once
 * each simulated year on their configured day; no RNG is involved.
 */
static void town_next(tiny_town* town)
{
    town_state* state = &town->state;
    const town_settings* settings = &town->settings;
    ++state->day;
    if (state->day > settings->days_per_year) {
        state->day = 1;
        if (state->year < INT_MAX)
            ++state->year;
    }
    int produced_food = 0;
    int produced_wood = 0;
    for (size_t index = 0; index < state->citizen_count; ++index) {
        const char* job = state->citizens[index].job;
        if (strcmp(job, "Farmer") == 0)
            produced_food += settings->farmer_food;
        else if (strcmp(job, "Hunter") == 0)
            produced_food += settings->hunter_food;
        else if (strcmp(job, "Builder") == 0)
            produced_wood += settings->builder_wood;
        /* Other jobs are valid citizens but don't produce these resources. */
    }
    const int consumed_food =
        (int)state->citizen_count * settings->food_per_citizen;
    state->food = town_adjust(state->food, produced_food - consumed_food);
    state->wood = town_adjust(state->wood, produced_wood);
    printf("\n[Year %d | Day %d]\n", state->year, state->day);
    if (settings->verbose_days)
        printf("Produced %d food, consumed %d food, produced %d wood.\n",
            produced_food, consumed_food, produced_wood);
    for (size_t index = 0; index < settings->event_count; ++index) {
        const town_event* event = &settings->events[index];
        if (event->day != state->day)
            continue;
        printf("Event: %s (food %+d, wood %+d, gold %+d)\n",
            event->name, event->food_delta, event->wood_delta,
            event->gold_delta);
        state->food = town_adjust(state->food, event->food_delta);
        state->wood = town_adjust(state->wood, event->wood_delta);
        state->gold = town_adjust(state->gold, event->gold_delta);
    }
    town->dirty = true;
    printf("Population: %zu | Food: %d | Wood: %d | Gold: %d\n",
        state->citizen_count, state->food, state->wood, state->gold);
    if (settings->debug)
        printf("[DEBUG] %zu scheduled events loaded.\n", settings->event_count);
    if (settings->autosave && !town_save(town))
        fprintf(stderr, "[CLM] Autosave failed; state remains unsaved.\n");
}

/** Display the working simulation snapshot without modifying CLM resources. */
static void town_status(const tiny_town* town)
{
    printf("\n%s | Year %d, Day %d\n", town->settings.name,
        town->state.year, town->state.day);
    printf("Population: %zu / %d\n", town->state.citizen_count,
        town->settings.population_limit);
    printf("Food: %d | Wood: %d | Gold: %d%s\n", town->state.food,
        town->state.wood, town->state.gold,
        town->dirty ? " (unsaved changes)" : "");
}

/** Show all citizens from the copied JSON snapshot. */
static void town_citizens(const tiny_town* town)
{
    puts("\nNAME                 JOB");
    puts("-------------------- ----------------");
    for (size_t i = 0; i < town->state.citizen_count; ++i)
        printf("%-20s %s\n", town->state.citizens[i].name,
            town->state.citizens[i].job);
}

/** List scheduled YAML events, including their resource changes. */
static void town_events(const tiny_town* town)
{
    puts("\nSCHEDULED EVENTS (each year)");
    for (size_t i = 0; i < town->settings.event_count; ++i) {
        const town_event* event = &town->settings.events[i];
        printf("  Day %-3d %-24s food %+d  wood %+d  gold %+d\n",
            event->day, event->name, event->food_delta, event->wood_delta,
            event->gold_delta);
    }
}

/** Change a named citizen's job without modifying the underlying JSON yet.
 *
 * The next save synchronizes the modified job into that citizen's JSON tree
 * object, demonstrating how parsed CLM trees can be updated and serialized.
 */
static void town_hire(tiny_town* town, const char* name, const char* job)
{
    if (strcmp(job, "Farmer") != 0 && strcmp(job, "Builder") != 0 &&
        strcmp(job, "Hunter") != 0) {
        puts("Jobs: Farmer, Builder, Hunter");
        return;
    }
    for (size_t i = 0; i < town->state.citizen_count; ++i) {
        town_citizen* citizen = &town->state.citizens[i];
        if (strcmp(citizen->name, name) == 0) {
            snprintf(citizen->job, sizeof(citizen->job), "%s", job);
            town->dirty = true;
            printf("%s is now a %s.\n", name, job);
            return;
        }
    }
    printf("No citizen named '%s'.\n", name);
}

/** Dispatch one whole terminal command; stdin is NOT executed as code.
 *
 * We parse only a tiny fixed grammar.  Unsupported commands are rejected,
 * and command input is bounded so an oversized line cannot be executed in
 * fragments as multiple commands.
 *
 * @return false only when the application should exit.
 */
static bool town_command(tiny_town* town, char* line)
{
    if (strcmp(line, "next") == 0) {
        town_next(town);
    } else if (strcmp(line, "status") == 0) {
        town_status(town);
    } else if (strcmp(line, "citizens") == 0) {
        town_citizens(town);
    } else if (strcmp(line, "events") == 0) {
        town_events(town);
    } else if (strcmp(line, "save") == 0) {
        if (!town_save(town))
            puts("Save failed; original file may need restoration.");
    } else if (strcmp(line, "reload") == 0) {
        if (town_reload(town)) {
            puts("Configuration and events reloaded; saved citizens unchanged.");
            town_status(town);
        }
    } else if (strncmp(line, "hire ", 5u) == 0) {
        char name[TOWN_NAME_CAP];
        char job[TOWN_JOB_CAP];
        char extra;
        if (sscanf(line + 5u, "%63s %23s %c", name, job, &extra) == 2)
            town_hire(town, name, job);
        else
            puts("Usage: hire <name> <Farmer|Builder|Hunter>");
    } else if (strcmp(line, "help") == 0) {
        puts("Commands: status | next | citizens | events | hire NAME JOB");
        puts("          save | reload | help | quit");
    } else if (strcmp(line, "quit") == 0) {
        if (town->dirty)
            puts("Warning: unsaved changes. Use 'save' before 'quit' next time.");
        puts("Goodbye from Crabville! ðŸ¦€");
        return false;
    } else if (line[0] != '\0') {
        puts("Unknown command. Type 'help'.");
    }
    return true;
}

/** Boot the sample: initialize CLM string IDs, load all five formats, and
 * validate the starting state BEFORE accepting simulation commands.
 *
 * @param argc Number of shell arguments.
 * @param argv argv[1] optionally selects the directory containing the files.
 * @return EXIT_SUCCESS for normal exit; EXIT_FAILURE on bootstrap failure.
 */
int main(int argc, char** argv)
{
    tiny_town town = {0};
    char line[TOWN_COMMAND_CAP];
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [config-directory]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char* directory = argc == 2 ? argv[1] : "config";
    if (strlen(directory) >= sizeof(town.config_dir)) {
        fputs("Configuration directory path is too long.\n", stderr);
        return EXIT_FAILURE;
    }
    snprintf(town.config_dir, sizeof(town.config_dir), "%s", directory);

    if (!clm_string_new(TOWN_INPUT_TEXT_ID, "") ||
        !clm_string_new(TOWN_OUTPUT_TEXT_ID, "")) {
        fputs("Unable to create CLM input/output strings.\n", stderr);
        return EXIT_FAILURE;
    }
    puts("[CLM] Booting Tiny Town...");

    /* The initial JSON file is loaded only ONCE, before the optional reload
     * command is available.  The resulting tree remains our writable save
     * document, and its child object IDs remain stable for the program's life. */
    if (!town_load_tree(&town, "citizens.json", TOWN_JSON_TREE_ID,
            clm_string_json_loads_auto_ids) ||
        !town_read_state(&town.state) || !town_reload(&town)) {
        fputs("[CLM] Town initialization failed. Check the sample files.\n", stderr);
        town_dispose_tree(TOWN_JSON_TREE_ID);
        (void)clm_string_delete(TOWN_OUTPUT_TEXT_ID);
        (void)clm_string_delete(TOWN_INPUT_TEXT_ID);
        return EXIT_FAILURE;
    }
    printf("\n========================================\n"
           "         CLM TINY TOWN â€” %s\n"
           "========================================\n", town.settings.name);
    town_status(&town);
    puts("Type 'help' to see available commands.");

    for (;;) {
        fputs("\ntown> ", stdout);
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;
        const size_t length = strlen(line);
        if (length != 0 && line[length - 1] != '\n' && !feof(stdin)) {
            int character;
            while ((character = getchar()) != '\n' && character != EOF) { }
            puts("Command too long; ignored.");
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (!town_command(&town, line))
            break;
    }
    /* CLM releases entire trees recursively.  No borrowed citizen child ID
     * should be accessed after this call.  Next release the two string IDs;
     * CLM's process-scoped key registry can live until process termination. */
    town_dispose_tree(TOWN_JSON_TREE_ID);
    (void)clm_string_delete(TOWN_OUTPUT_TEXT_ID);
    (void)clm_string_delete(TOWN_INPUT_TEXT_ID);
    return EXIT_SUCCESS;
}

