/*
 * Hello from Cross Language Modules!
 *
 * An interactive C11 example showcasing CLM strings, IDs, a heterogeneous
 * dictionary, CSV-to-table and YAML-to-tree loading, and the powerful console.
 *
 * This example targets CLM's compiled .h/.c distribution. Link the required
 * CLM libraries using your repository's build system; do not define
 * CLM_*_IMPLEMENTATION switches in this file.
 *
 * Run from this example's directory so config/town.yaml and
 * config/citizens.csv can be found.
 */

#include "clm/base.h"
#include "clm/console.h"
#include "clm/dictionary.h"
#include "clm/id.h"
#include "string.h"
#include "clm/string_dsv.h"
#include "clm/string_yaml.h"
#include "clm/table.h"
#include "clm/tree.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Resource IDs are examples. Keep them unique in your application's ID space. */
#define HELLO_MENU_ID       UINT64_C(0x71010001)
#define HELLO_YAML_PATH_ID  UINT64_C(0x71010002)
#define HELLO_CSV_PATH_ID   UINT64_C(0x71010003)
#define HELLO_DICTIONARY_ID UINT64_C(0x71010011)
#define HELLO_TABLE_ID      UINT64_C(0x71010012)
#define HELLO_TREE_ID       UINT64_C(0x71010013)

/* Keys used by the YAML parser and the sample heterogeneous dictionary. */
static const clm_id HELLO_KEY_TOWN       = UINT64_C(0x71010101);
static const clm_id HELLO_KEY_RESOURCES  = UINT64_C(0x71010102);
static const clm_id HELLO_KEY_NAME       = UINT64_C(0x71010103);
static const clm_id HELLO_KEY_MOTTO      = UINT64_C(0x71010104);
static const clm_id HELLO_KEY_POPULATION = UINT64_C(0x71010105);
static const clm_id HELLO_KEY_GRAIN      = UINT64_C(0x71010106);
static const clm_id HELLO_KEY_WOOD       = UINT64_C(0x71010107);
static const clm_id HELLO_KEY_WATER      = UINT64_C(0x71010108);
static const clm_id HELLO_KEY_MASCOT     = UINT64_C(0x71010109);
static const clm_id HELLO_KEY_LANGUAGE   = UINT64_C(0x7101010A);
static const clm_id HELLO_KEY_VERSION    = UINT64_C(0x7101010B);
static const clm_id HELLO_KEY_CITIZENS   = UINT64_C(0x7101010C);

typedef struct hello_resources {
    bool menu;
    bool yaml_path;
    bool csv_path;
    bool dictionary;
    bool table;
    bool tree;
} hello_resources;

/**
 * Registers names before YAML parsing.
 *
 * CLM's YAML subset resolves mapping keys through clm_id. The DSV parser
 * can register previously unknown CSV headers automatically.
 *
 * Returns true only when every required name was registered successfully.
 * The clm_id registry remains process-owned; these mappings are not removed
 * when individual data objects are deleted.
 */
static bool hello_register_keys(void) // NOLINT(*-redundant-void-arg)
{
    struct hello_key_definition {
        const char *name;
        clm_id id;
    };

    const struct hello_key_definition keys[] = {
        {"town",       HELLO_KEY_TOWN},
        {"resources",  HELLO_KEY_RESOURCES},
        {"name",       HELLO_KEY_NAME},
        {"motto",      HELLO_KEY_MOTTO},
        {"population", HELLO_KEY_POPULATION},
        {"grain",      HELLO_KEY_GRAIN},
        {"wood",       HELLO_KEY_WOOD},
        {"water",      HELLO_KEY_WATER},
        {"mascot",     HELLO_KEY_MASCOT},
        {"language",   HELLO_KEY_LANGUAGE},
        {"version",    HELLO_KEY_VERSION},
        {"citizens",   HELLO_KEY_CITIZENS}
    };

    const size_t count = sizeof(keys) / sizeof(keys[0]);

    for (size_t index = 0; index < count; ++index) {
        if (clm_id_register(keys[index].name, keys[index].id)
                != CLM_BPA_SET_SUCCESS) {
            fprintf(stderr, "Failed to register CLM key: %s\n", keys[index].name);
            return false;
        }
    }

    return true;
}

/**
 * Builds a heterogeneous dictionary with separately typed values.
 *
 * The dictionary copies each value into a dictionary-owned BPA block.
 * On success, the caller owns the dictionary and must call
 * clm_dictionary_delete().
 */
static bool hello_create_dictionary(void) // NOLINT(*-redundant-void-arg)
{
    const clm_int version = 1;
    const clm_int citizens = 4;

    if (clm_dictionary_new(HELLO_DICTIONARY_ID) != CLM_BPA_SUCCESS) {
        return false;
    }

    if (clm_dictionary_set_from(HELLO_DICTIONARY_ID, HELLO_KEY_MASCOT,
            CLM_BPA_TYPE_STRING, "Hermit Crab 🦀", sizeof("Hermit Crab 🦀"))
            != CLM_BPA_SET_SUCCESS ||
        clm_dictionary_set_from(HELLO_DICTIONARY_ID, HELLO_KEY_LANGUAGE,
            CLM_BPA_TYPE_STRING, "C11", sizeof("C11"))
            != CLM_BPA_SET_SUCCESS ||
        clm_dictionary_set_from(HELLO_DICTIONARY_ID, HELLO_KEY_VERSION,
            CLM_BPA_TYPE_INT, &version, sizeof(version))
            != CLM_BPA_SET_SUCCESS ||
        clm_dictionary_set_from(HELLO_DICTIONARY_ID, HELLO_KEY_CITIZENS,
            CLM_BPA_TYPE_INT, &citizens, sizeof(citizens))
            != CLM_BPA_SET_SUCCESS) {
        return false;
    }

    return true;
}

/**
 * Displays a single object and appends one blank line afterward.
 *
 * Object writers return clm_console_status. The helper reports failures
 * without interpreting or changing the underlying CLM object.
 */
static bool hello_report_write(const char *description,
                               clm_console_status status)
{
    if (status != CLM_CONSOLE_SUCCESS) {
        fprintf(stderr, "Unable to display %s.\n", description);
        return false;
    }

    return clm_console_write("\n\n") == CLM_CONSOLE_SUCCESS;
}

/**
 * Runs the interactive demonstration.
 *
 * The menu lives in a UTF-8 CLM string block; write_block prints its contents
 * directly. The YAML tree and CSV table were parsed once at startup and are
 * reused across all menu selections.
 */
static void hello_run_menu(void) // NOLINT(*-redundant-void-arg)
{
    bool colors_enabled = true;

    for (;;) {
        const char *selection = NULL;
        clm_console_status status;

        if (clm_console_write_block(HELLO_MENU_ID) != CLM_CONSOLE_SUCCESS ||
            clm_console_read("clm> ", &selection) != CLM_CONSOLE_SUCCESS) {
            clm_console_write_to(stderr, "Console input/output failed.\n");
            break;
        }

        if (strcmp(selection, "0") == 0) {
            clm_console_write_line("Don't start from scratch, bro! 🦀");
            break;
        }

        if (strcmp(selection, "1") == 0) {
            clm_console_write_line("\nHeterogeneous dictionary:");
            status = clm_console_write_dictionary(HELLO_DICTIONARY_ID);
            (void)hello_report_write("dictionary", status);
        } else if (strcmp(selection, "2") == 0) {
            clm_console_write_line("\nCSV loaded into a CLM table:");
            status = clm_console_write_table(HELLO_TABLE_ID);
            (void)hello_report_write("table", status);
        } else if (strcmp(selection, "3") == 0) {
            clm_console_write_line("\nYAML loaded into a CLM tree:");
            status = clm_console_write_tree(HELLO_TREE_ID);
            (void)hello_report_write("tree", status);
        } else if (strcmp(selection, "4") == 0) {
            clm_console_write_line("\nThe same tree serialized as YAML:");
            status = clm_console_write_tree_as_yaml(HELLO_TREE_ID);
            (void)hello_report_write("YAML", status);
        } else if (strcmp(selection, "5") == 0) {
            clm_console_write_line("\nThe same tree serialized as JSON:");
            status = clm_console_write_tree_as_json(HELLO_TREE_ID);
            (void)hello_report_write("JSON", status);
        } else if (strcmp(selection, "6") == 0) {
            colors_enabled = !colors_enabled;
            clm_console_set_color_on_object_writes(colors_enabled ? 1 : 0);
            clm_console_write_line(colors_enabled
                ? "ANSI colors enabled for object writes."
                : "ANSI colors disabled for object writes.");
        } else if (strcmp(selection, "7") == 0) {
            clm_console_write_line("\nInspect the menu as a BPA string block:");
            status = clm_console_inspect_block(HELLO_MENU_ID);
            (void)hello_report_write("menu block", status);
        } else {
            clm_console_write_line("Unknown option. Choose 0 to 7.");
        }
    }
}

/**
 * Frees only resources successfully created by this example.
 *
 * Dictionary, table, and tree own their parsed or copied children. File-path
 * strings and menu strings remain independent resources. No parsed child ID
 * is individually deleted.
 */
static void hello_cleanup(const hello_resources *resources)
{
    if (resources->tree) {
        (void)clm_tree_delete(HELLO_TREE_ID);
    }

    if (resources->table) {
        (void)clm_table_delete(HELLO_TABLE_ID);
    }

    if (resources->dictionary) {
        (void)clm_dictionary_delete(HELLO_DICTIONARY_ID);
    }

    if (resources->csv_path) {
        (void)clm_string_delete(HELLO_CSV_PATH_ID);
    }

    if (resources->yaml_path) {
        (void)clm_string_delete(HELLO_YAML_PATH_ID);
    }

    if (resources->menu) {
        (void)clm_string_delete(HELLO_MENU_ID);
    }
}

/**
 * Creates sample objects, runs the demonstration, then frees them.
 *
 * Run from the example directory. The sample input files are required and
 * must be valid UTF-8 without an encoding header.
 */
int main(void) // NOLINT(*-redundant-void-arg)
{
    hello_resources resources = {0};
    int exit_code = EXIT_FAILURE;

    if (!hello_register_keys()) {
        goto cleanup;
    }

    if (!clm_string_new(HELLO_MENU_ID,
            "\n"
            "  1. Dictionary\n"
            "  2. Table (CSV)\n"
            "  3. Tree (YAML)\n"
            "  4. Tree as YAML\n"
            "  5. Tree as JSON\n"
            "  6. Toggle object colors\n"
            "  7. Inspect menu block\n"
            "  0. Exit\n\n")) {
        fprintf(stderr, "Unable to create menu string.\n");
        goto cleanup;
    }
    resources.menu = true;

    if (!clm_string_new(HELLO_YAML_PATH_ID, "config/town.yaml")) {
        fprintf(stderr, "Unable to create YAML file-path string.\n");
        goto cleanup;
    }
    resources.yaml_path = true;

    if (!clm_string_new(HELLO_CSV_PATH_ID, "config/citizens.csv")) {
        fprintf(stderr, "Unable to create CSV file-path string.\n");
        goto cleanup;
    }
    resources.csv_path = true;

    if (clm_string_yaml_load(HELLO_YAML_PATH_ID, HELLO_TREE_ID)
            != CLM_FORMAT_DESERIALIZE_SUCCESS) {
        fprintf(stderr, "Could not parse config/town.yaml.\n");
        goto cleanup;
    }
    resources.tree = true;

    /* DSV defaults to comma-separated records with a header row. */
    if (clm_string_dsv_load(HELLO_CSV_PATH_ID, HELLO_TABLE_ID)
            != CLM_BPA_SUCCESS) {
        fprintf(stderr, "Could not parse config/citizens.csv.\n");
        goto cleanup;
    }
    resources.table = true;

    if (!hello_create_dictionary()) {
        /* A partially constructed dictionary should still be cleaned up. */
        resources.dictionary = true;
        fprintf(stderr, "Could not populate the example dictionary.\n");
        goto cleanup;
    }
    resources.dictionary = true;

    clm_console_set_color_on_object_writes(1);
    clm_console_write_color("Hello from Cross Language Modules! 🦀\n",
                            CONSOLE_COLOR_CYAN);
    clm_console_write_line("YAML + CSV + Dictionary + UTF-8 + Console");

    hello_run_menu();
    exit_code = EXIT_SUCCESS;

cleanup:
    hello_cleanup(&resources);
    return exit_code;
}
