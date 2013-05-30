/**
 * \file    uncut.c
 * \date    6-August-2012
 * \author  Andre Renaud
 * \copyright Aiotec/Bluewater Systems
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#include "uncut.h"

struct uncut_execution {
    struct uncut_suite *group;
    int item_index;
    int result;
};

#ifndef UNCUT_USE_SQLITE
/* Empty implementation of the database stuff, if we don't want to
 * use SQLITE
 */
typedef void *uncut_db;
static uncut_db *uncut_db_open(const char *filename)
{
    return NULL;
}
static void uncut_db_close(uncut_db *db)
{
}

static void uncut_db_add_test_run(uncut_db *db, int64_t id,
        struct uncut_execution *test, const char *message)
{
}
static int64_t uncut_db_new_id(uncut_db *db, const char *suite_name)
{
    return 0;
}
static void uncut_db_finish_id(uncut_db *db, int64_t db_id)
{
}
static void db_add_global_note(uncut_db *db, int64_t db_id, const char *message)
{
}
#else
#include <sqlite3.h>
typedef struct sqlite3 uncut_db;

static int uncut_db_sql(uncut_db *db, const char *command, ...)
{
    char *sql, *err_msg;
    va_list args;
    int ret;

    if (!db)
        return -EINVAL;

    va_start(args, command);
    /* FIXME: Should be using sqlite3_mprintf & %q in our commands */
    ret = vasprintf(&sql, command, args);
    va_end(args);
    if (ret < 0)
        return -ENOMEM;

    ret = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "SQL query '%s' failed: %s\n", sql, err_msg);
        free(sql);
        return -EFAULT;
    }
    free(sql);
    return 0;
}

static uncut_db *uncut_db_open(const char *filename)
{
    uncut_db *db;
    if (sqlite3_open(filename, &db) < 0)
        return NULL;
    if (uncut_db_sql(db,
                "CREATE TABLE IF NOT EXISTS test_run ("
                " id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
                " start_time DATETIME NOT NULL,"
                " end_time DATETIME,"
                " title TEXT"
                ")") != 0) {
        sqlite3_close(db);
        return NULL;
    }
    if (uncut_db_sql(db,
                "CREATE TABLE IF NOT EXISTS test_result ("
                " id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
                " run_id INTEGER NOT NULL REFERENCES test_run(id),"
                " suite_name TEXT NOT NULL,"
                " test_name TEXT NOT NULL,"
                " message TEXT,"
                " result BOOL NOT NULL,"
                " UNIQUE (run_id, suite_name, test_name) ON CONFLICT FAIL"
                ")") != 0) {
        sqlite3_close(db);
        return NULL;
    }
    if (uncut_db_sql(db,
                "CREATE TABLE IF NOT EXISTS test_notes ("
                " id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
                " run_id INTEGER NOT NULL REFERENCES test_run(id),"
                " note TEXT NOT NULL,"
                " UNIQUE (run_id, note) ON CONFLICT FAIL"
                ")") != 0) {
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

static void uncut_db_close(uncut_db *db)
{
    if (db)
        sqlite3_close(db);
}

static void uncut_db_add_test_run(uncut_db *db, int64_t id,
        struct uncut_execution *test, const char *message)
{
    if (!db)
        return;
    uncut_db_sql(db, "INSERT INTO test_result "
            "(run_id, suite_name, test_name, result, message) "
            "VALUES (%lld, \"%s\", \"%s\", %d, \"%s\")",
            id, test->group->group_name,
            test->group->tests[test->item_index].item_name,
            test->result, message);
}

static char *seconds_to_db_time(time_t t, char *buf, size_t max)
{
    struct tm ptm;

    localtime_r(&t, &ptm);
    strftime(buf, max, "%Y-%m-%d %H:%M:%S", &ptm);
    return buf;
}

static int64_t uncut_db_new_id(uncut_db *db, const char *suite_name)
{
    char start_str[64];
    int ret;
    if (!db)
        return -1;
    seconds_to_db_time(time(NULL), start_str, sizeof(start_str));
    ret = uncut_db_sql(db, "INSERT INTO test_run "
                "(title, start_time) VALUES (\"%s\", \"%s\")",
                suite_name, start_str);
    if (ret < 0)
        return ret;

    ret = sqlite3_last_insert_rowid(db);

    return ret;
}

static void uncut_db_finish_id(uncut_db *db, int64_t db_id)
{
    char end_str[64];
    if (!db)
        return;
    seconds_to_db_time(time(NULL), end_str, sizeof(end_str));
    uncut_db_sql(db, "UPDATE test_run SET "
                "end_time=\"%s\" WHERE id=%lld",
                end_str, db_id);
}

static void db_add_global_note(uncut_db *db, int64_t db_id, const char *message)
{
    uncut_db_sql(db, "INSERT INTO test_notes "
            "(run_id, note) VALUES (%lld, \"%s\")", db_id, message);
}
#endif

static struct uncut_parameter *global_parameters = NULL;
static uncut_db *global_db = NULL;
static int64_t global_db_id = -1;
static char global_last_message[256];

static int run_test(uncut_db *db, int64_t id, struct uncut_execution *test)
{
    struct uncut_test *item = &test->group->tests[test->item_index];

    printf("Running test %s:%d:%s\n", test->group->group_name,
            test->item_index + 1, item->item_name);

    global_last_message[0] = '\0';
    /* Actually run the test */
    test->result = item->function();

    printf("RESULT: %s\n", test->result >= 0 ? "SUCCESS" : "FAILURE");

    if (db)
        uncut_db_add_test_run(db, id, test, global_last_message);

    return test->result;
}

static int uncut_usage(const char *program, struct uncut_suite *groups,
        struct uncut_parameter *parameters)
{
    fprintf(stderr, "Usage: %s [-t uncut_name[:n]] [-p parameter=value] [-d database]\n",
        program);
    /* FIXME: Print out test names & parameters */
    return -1;
}

static struct uncut_parameter *find_parameter(struct uncut_parameter *parameters,
        const char *name)
{
    for (; parameters && parameters->name; parameters++)
        if (strcmp(parameters->name, name) == 0)
            return parameters;
    fprintf(stderr, "Invalid parameter: '%s'\n", name);
    return NULL;
}

static int set_parameter(struct uncut_parameter *parameters,
        const char *name, const char *value)
{
    struct uncut_parameter *match = find_parameter(parameters, name);
    if (!match)
        return -EINVAL;

    match->value = value;
    return 0;
}

static struct uncut_suite *find_group(struct uncut_suite *groups, const char *name)
{
    for (; groups->group_name; groups++) {
        if (strcmp(groups->group_name, name) == 0)
            return groups;
    }
    fprintf(stderr, "Invalid group name: '%s'\n", name);
    return NULL;
}

static int print_uncut_details(struct uncut_suite *groups,
        struct uncut_parameter *parameters)
{
    struct uncut_suite *group;
    struct uncut_parameter *param;
    int i;

    for (group = groups; group->group_name; group++) {
        fprintf(stderr, "Group: %s\n", group->group_name);
        for (i = 0; group->tests[i].item_name; i++) {
            /* We display them indexed from 1, not 0 */
            fprintf(stderr, "\t%d: %s\n",
                    i + 1, group->tests[i].item_name);
        }
    }

    fprintf(stderr, "Parameters:\n");
    for (param = parameters; param->name; param++)
        fprintf(stderr, "\t%-16s: %s (Default: %s)\n",
                param->name, param->description, param->value);

    return 0;
}

static int group_count(struct uncut_suite *group)
{
    int i;
    for (i = 0; group->tests[i].item_name; i++)
        ;
    return i;
}

int uncut_suite_run(const char *suite_name, struct uncut_suite *groups,
        struct uncut_parameter *parameters, int argc, char *argv[])
{
    struct uncut_execution *tests = NULL;
    int ntests = 0;
    int i;
    int failed = 0;
    uncut_db *db = NULL;
    int64_t db_id;

    while (1) {
        int c = getopt(argc, argv, "t:p:ld:h");

        if (c == -1)
            break;

        switch (c) {
            case 't': {
                char *group_name, *items;
                struct uncut_suite *group;

                group_name = optarg;
                items = strchr(optarg, ':');
                if (items) {
                    *items = '\0';
                    items++;
                }
                group = find_group(groups, group_name);
                if (!group) {
                    free(tests);
                    return -1;
                }

                if (items) {
                    int index = atoi(items);
                    /* We display them indexed from 1, not 0 */
                    index--;

                    if (index < 0 || index >= group_count(group)) {
                        fprintf(stderr, "Invalid index for test %s: %d\n",
                            group_name, index + 1);
                        free(tests);
                        return -1;
                    }
                    /* FIXME: Parse item numbers properly */
                    ntests++;
                    tests = realloc(tests, ntests * sizeof(struct uncut_execution));
                    if (!tests)
                        return -ENOMEM;
                    tests[ntests - 1].group = group;
                    tests[ntests - 1].item_index = index;
                    tests[ntests - 1].result = 0;
                } else {
                    for (i = 0; group->tests[i].item_name; i++) {
                        ntests++;
                        tests = realloc(tests,
                                ntests * sizeof(struct uncut_execution));
                        if (!tests)
                            return -ENOMEM;
                        tests[ntests - 1].group = group;
                        tests[ntests - 1].item_index = i;
                        tests[ntests - 1].result = 0;
                    }
                }
                break;
            }

            case 'p': {
                char *name, *value;
                name = optarg;
                value = strchr(optarg, '=');
                if (value) {
                    *value = '\0';
                    value++;
                }
                if (set_parameter(parameters, name, value) < 0) {
                    free(tests);
                    return -1;
                }
                break;
            }

            case 'd':
#ifndef UNCUT_USE_SQLITE
                fprintf(stderr, "Database support not enabled\n");
                return -1;
#endif
                db = uncut_db_open(optarg);
                break;

            case 'l':
                free(tests);
                print_uncut_details(groups, parameters);
                return 0;

            case 'h':
            default:
                free(tests);
                return uncut_usage(argv[0], groups, parameters);
        }

    }
    if (optind < argc) {
        if (tests)
            free(tests);
        return uncut_usage(argv[0], groups, parameters);
    }

    /* If we haven't selected any tests, then run them all */
    if (ntests == 0) {
        struct uncut_suite *group;
        for (group = groups; group && group->group_name && group->tests; group++) {
            for (i = 0; group->tests[i].item_name && group->tests[i].function; i++) {
                ntests++;
                tests = realloc(tests, ntests * sizeof(struct uncut_execution));
                if (!tests)
                    return -ENOMEM;
                tests[ntests - 1].group = group;
                tests[ntests - 1].item_index = i;
                tests[ntests - 1].result = 0;
            }
        }
    }

    db_id = uncut_db_new_id(db, suite_name);
    global_db_id = db_id;
    global_parameters = parameters;
    global_db = db;

    /* Execute the tests */
    for (i = 0; i < ntests; i++) {
        if (run_test(db, db_id, &tests[i]) < 0) {
            failed++;
            break;
        }
    }
    uncut_db_finish_id(db, db_id);

    if (db)
        uncut_db_close(db);

    free(tests);

    global_parameters = NULL;

    fprintf(stderr, "\nSUMMARY '%s': %s\n", suite_name, failed ? "FAILURE" : "SUCCESS");

    return failed ? -1 : 0;
}

const char *uncut_param(const char *name)
{
    struct uncut_parameter *match = find_parameter(global_parameters, name);
    if (match)
        return match->value;
    return NULL;
}

int uncut_param_int(const char *name)
{
    struct uncut_parameter *match = find_parameter(global_parameters, name);
    if (match && match->value)
        return strtol(match->value, NULL, 0);
    return -1;
}

void uncut_set_message(const char *message, int global)
{
    /* FIXME: Put them in the database */
    if (!global_db)
        return;
    if (global)
        db_add_global_note(global_db, global_db_id, message);
    else {
        strncpy(global_last_message, message, sizeof(global_last_message) - 1);
        global_last_message[sizeof(global_last_message) - 1] = '\0';
    }
}


