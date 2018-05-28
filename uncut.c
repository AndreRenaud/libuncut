/**
 * \file    uncut.c
 * \date    6-August-2012
 * \author  Andre Renaud
 * \copyright Aiotec/Bluewater Systems
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "uncut.h"

struct uncut_execution {
    struct uncut_suite *group;
    int item_index;
    int result;
};

static struct uncut_parameter *global_parameters = NULL;

static void print_uncut_details(struct uncut_suite *groups,
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
}

static void uncut_usage(const char *program, struct uncut_suite *groups,
                        struct uncut_parameter *parameters)
{
    fprintf(stderr, "Usage: %s [-t group_name[:n]] [-p parameter=value] [-c]\n",
            program);
    fprintf(stderr, "\t-c - Continue running tests on failure\n");

    print_uncut_details(groups, parameters);
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
        int len = strlen(groups->group_name);
        if (strncmp(groups->group_name, name, len) == 0 &&
                (name[len] == '\0' || name[len] == ':'))
            return groups;
    }
    fprintf(stderr, "Invalid group name: '%s'\n", name);
    return NULL;
}

static int group_count(struct uncut_suite *group)
{
    int i;
    for (i = 0; group->tests[i].item_name; i++)
        ;
    return i;
}

int uncut_suite_run(const char *suite_name, struct uncut_suite *groups,
                    struct uncut_parameter *parameters, int argc, char *argv[],
                    uncut_callback callback)
{
    struct uncut_execution *tests = NULL;
    int ntests = 0;
    int i;
    int failed = 0;
    int continue_on_failure = 0;

    while (1) {
        int c = getopt(argc, argv, "t:p:lch");

        if (c == -1)
            break;

        switch (c) {
        case 't': {
            char *group_name, *items;
            struct uncut_suite *group;
            struct uncut_execution *new_tests;

            group_name = optarg;
            items = strchr(optarg, ':');
            group = find_group(groups, group_name);
            if (!group) {
                free(tests);
                return -1;
            }

            if (items) {
                for (; items; items = strchr(items, ',')) {
                    items++; // skip the ':' or ','
                    if (!*items) {
                        fprintf(stderr, "Invalid test id specification in '%s'\n", optarg);
                        free(tests);
                        return -1;
                    }
                    int index = atoi(items);
                    /* We display them indexed from 1, not 0 */
                    index--;

                    if (index < 0 || index >= group_count(group)) {
                        fprintf(stderr, "Invalid index for test %s: %d (valid range: %d-%d)\n",
                                group->group_name, index + 1,
                                1, group_count(group));
                        free(tests);
                        return -1;
                    }
                    /* FIXME: Parse item numbers properly */
                    ntests++;
                    new_tests = realloc(tests, ntests * sizeof(struct uncut_execution));
                    if (!new_tests) {
                        free(tests);
                        return -ENOMEM;
                    }
                    tests = new_tests;
                    tests[ntests - 1].group = group;
                    tests[ntests - 1].item_index = index;
                    tests[ntests - 1].result = 0;
                }
            } else {
                for (i = 0; group->tests[i].item_name; i++) {
                    ntests++;
                    new_tests = realloc(tests,
                                        ntests * sizeof(struct uncut_execution));
                    if (!new_tests) {
                        free(tests);
                        return -ENOMEM;
                    }
                    tests = new_tests;
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

        case 'l':
            free(tests);
            print_uncut_details(groups, parameters);
            return 0;

        case 'c':
            continue_on_failure = 1;
            break;

        case 'h':
        default:
            free(tests);
            uncut_usage(argv[0], groups, parameters);
            return -1;
        }

    }
    if (optind < argc) {
        if (tests)
            free(tests);
        uncut_usage(argv[0], groups, parameters);
        return -1;
    }

    /* If we haven't selected any tests, then run them all */
    if (ntests == 0) {
        struct uncut_suite *group;
        for (group = groups; group && group->group_name && group->tests; group++) {
            for (i = 0; group->tests[i].item_name && group->tests[i].function; i++) {
                struct uncut_execution *new_tests;
                ntests++;
                new_tests = realloc(tests, ntests * sizeof(struct uncut_execution));
                if (!new_tests) {
                    free(tests);
                    return -ENOMEM;
                }
                tests = new_tests;
                tests[ntests - 1].group = group;
                tests[ntests - 1].item_index = i;
                tests[ntests - 1].result = 0;
            }
        }
    }
    global_parameters = parameters;

    /* Execute the tests */
    for (i = 0; i < ntests && (continue_on_failure || !failed); i++) {
        struct uncut_test *item = &tests[i].group->tests[tests[i].item_index];
        struct timespec ts1, ts2;
        if (callback) {
            callback(tests[i].group, item, 0, -1);
            if (clock_gettime(CLOCK_MONOTONIC, &ts1) < 0)
                ts1.tv_sec = ts1.tv_nsec = 0;
        }
        tests[i].result = item->function();
        if (callback) {
            if (clock_gettime(CLOCK_MONOTONIC, &ts2) < 0)
                ts2.tv_sec = ts2.tv_nsec = 0;
            callback(tests[i].group, item, tests[i].result,
                     (ts2.tv_sec - ts1.tv_sec) * 1000 + (ts2.tv_nsec - ts1.tv_nsec) / 1000000);
        }
        if (tests[i].result < 0)
            failed++;
    }
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
