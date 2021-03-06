/**
 * \file    uncut.c
 * \date    6-August-2012
 * \author  Andre Renaud
 * \copyright Aiotec/Bluewater Systems
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "uncut.h"

struct uncut_execution {
    uncut_callback callback;
    struct uncut_suite *group;
    int item_index;
    int result;
    sem_t *sem;
};

static struct uncut_parameter *global_parameters = NULL;

static void print_uncut_details(struct uncut_suite *groups,
                                struct uncut_parameter *parameters)
{
    struct uncut_suite *group;
    int i;

    for (group = groups; group->group_name; group++) {
        fprintf(stderr, "Group: %s\n", group->group_name);
        for (i = 0; group->tests[i].item_name; i++) {
            /* We display them indexed from 1, not 0 */
            fprintf(stderr, "\t%d: %s\n", i + 1, group->tests[i].item_name);
        }
    }

    if (parameters) {
        fprintf(stderr, "Parameters:\n");
        for (struct uncut_parameter *param = parameters; param->name; param++)
            fprintf(stderr, "\t%-16s: %s (Default: %s)\n", param->name,
                    param->description, param->value);
    }
}

static void uncut_usage(const char *program, struct uncut_suite *groups,
                        struct uncut_parameter *parameters)
{
    fprintf(stderr,
            "Usage: %s [-t group_name[:n]] [-p parameter=value] [-c]\n",
            program);
    fprintf(stderr, "\t-c - Continue running tests on failure\n");
    fprintf(stderr,
            "\t-j N - Run tests using N parallel threads. Implies -c\n");

    print_uncut_details(groups, parameters);
}

static struct uncut_parameter *
find_parameter(struct uncut_parameter *parameters, const char *name)
{
    for (; parameters && parameters->name; parameters++)
        if (strcmp(parameters->name, name) == 0)
            return parameters;
    fprintf(stderr, "Invalid parameter: '%s'\n", name);
    return NULL;
}

static int set_parameter(struct uncut_parameter *parameters, const char *name,
                         const char *value)
{
    struct uncut_parameter *match = find_parameter(parameters, name);
    if (!match)
        return -EINVAL;

    match->value = value;
    return 0;
}

static struct uncut_suite *find_group(struct uncut_suite *groups,
                                      const char *name)
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

static int run_test(struct uncut_execution *test)
{
    struct uncut_test *item = &test->group->tests[test->item_index];
    struct timespec ts1 = {0, 0}, ts2 = {0, 0};
    if (test->callback) {
        test->callback(test->group, item, 0, -1);
        clock_gettime(CLOCK_MONOTONIC, &ts1);
    }
    test->result = item->function();
    if (test->callback) {
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        test->callback(test->group, item, test->result,
                       (ts2.tv_sec - ts1.tv_sec) * 1000 +
                           (ts2.tv_nsec - ts1.tv_nsec) / 1000000);
    }
    return test->result;
}

static void *run_test_thread(void *data)
{
    struct uncut_execution *test = data;
    run_test(test);
    if (test->sem)
        sem_post(test->sem);
    return NULL;
}

struct uncut_execution *update_tests(struct uncut_execution *tests,
                                     int ntests, struct uncut_suite *group,
                                     int index, uncut_callback callback)
{
    struct uncut_execution *new_tests =
        realloc(tests, ntests * sizeof(struct uncut_execution));
    if (!new_tests) {
        free(tests);
        return NULL;
    }
    tests = new_tests;
    tests[ntests - 1].group = group;
    tests[ntests - 1].item_index = index;
    tests[ntests - 1].result = 0;
    tests[ntests - 1].callback = callback;
    tests[ntests - 1].sem = NULL;
    return tests;
}

int uncut_suite_run(struct uncut_suite *groups,
                    struct uncut_parameter *parameters, int argc,
                    char *argv[], uncut_callback callback)
{
    struct uncut_execution *tests = NULL;
    int ntests = 0;
    int i;
    int failed = 0;
    int continue_on_failure = 0;
    int parallel = 0;

    while (1) {
        int c = getopt(argc, argv, "t:p:lchj:");

        if (c == -1)
            break;

        switch (c) {
        case 't': {
            char *group_name, *items;
            struct uncut_suite *group;

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
                        fprintf(stderr,
                                "Invalid test id specification in '%s'\n",
                                optarg);
                        free(tests);
                        return -1;
                    }
                    int index = atoi(items);
                    /* We display them indexed from 1, not 0 */
                    index--;

                    if (index < 0 || index >= group_count(group)) {
                        fprintf(stderr,
                                "Invalid index for test %s: %d (valid range: "
                                "%d-%d)\n",
                                group->group_name, index + 1, 1,
                                group_count(group));
                        free(tests);
                        return -1;
                    }
                    /* FIXME: Parse item numbers properly */
                    ntests++;
                    tests =
                        update_tests(tests, ntests, group, index, callback);
                    if (!tests)
                        return -ENOMEM;
                }
            } else {
                for (i = 0; group->tests[i].item_name; i++) {
                    ntests++;
                    tests = update_tests(tests, ntests, group, i, callback);
                    if (!tests)
                        return -ENOMEM;
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

        case 'j':
            parallel = atoi(optarg);
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
        for (group = groups; group && group->group_name && group->tests;
             group++) {
            for (i = 0; group->tests[i].item_name && group->tests[i].function;
                 i++) {
                ntests++;
                tests = update_tests(tests, ntests, group, i, callback);
                if (!tests)
                    return -ENOMEM;
            }
        }
    }
    global_parameters = parameters;

    // There aren't any tests supplied
    if (ntests == 0)
        return -1;

    /* Execute the tests */
    if (parallel <= 1) {
        for (i = 0; i < ntests && (continue_on_failure || !failed); i++) {
            if (run_test(&tests[i]) < 0)
                failed++;
        }
    } else {
        pthread_t threads[ntests];
        sem_t sem;
        sem_init(&sem, 0, parallel);
        for (i = 0; i < ntests; i++) {
            tests[i].sem = &sem;
            sem_wait(&sem);
            pthread_create(&threads[i], NULL, run_test_thread, &tests[i]);
        }
        for (i = 0; i < ntests; i++)
            pthread_join(threads[i], NULL);
        for (i = 0; i < ntests; i++)
            if (tests[i].result < 0)
                failed++;
    }
    free(tests);

    global_parameters = NULL;

    fprintf(stderr, "\nSUMMARY: %s\n", failed ? "FAILURE" : "SUCCESS");

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

static struct uncut_suite *uncut_tests = NULL;
static struct uncut_parameter *uncut_parameters = NULL;

void uncut_register_test(const char *group_name, const char *test_name,
                         uncut_function function)
{
    struct uncut_suite *suite;
    struct uncut_test *tests;
    int ntests = 0;

    for (suite = uncut_tests; suite && suite->group_name; suite++) {
        if (strcmp(suite->group_name, group_name) == 0)
            break;
    }
    if (!suite || !suite->group_name) {
        int nsuites = 0;
        for (struct uncut_suite *s = uncut_tests; s && s->group_name; s++)
            nsuites++;
        nsuites++;
        suite =
            realloc(uncut_tests, (nsuites + 1) * sizeof(struct uncut_suite));
        uncut_tests = suite;
        if (!suite)
            return;
        suite[nsuites - 1].group_name = group_name;
        suite[nsuites - 1].tests = NULL;
        suite[nsuites].group_name = NULL;
        suite[nsuites].tests = NULL;

        suite = &suite[nsuites - 1];
    }

    tests = suite->tests;

    for (struct uncut_test *t = tests; t && t->item_name; t++)
        ntests++;
    ntests++;
    tests = realloc(tests, (ntests + 1) * sizeof(struct uncut_test));
    suite->tests = tests;
    if (!tests)
        return;
    tests[ntests - 1].item_name = test_name;
    tests[ntests - 1].function = function;
    tests[ntests].item_name = NULL;
    tests[ntests].function = NULL;
}

void uncut_register_param(const char *name, const char *value,
                          const char *description)
{
    struct uncut_parameter *param;
    int nparams = 0;

    for (param = uncut_parameters; param && param->name; param++) {
        nparams++;
        if (strcmp(name, param->name) == 0) {
            fprintf(stderr, "Duplicate parameter defined '%s'\n", name);
            exit(1);
        }
    }
    nparams++;

    uncut_parameters = realloc(
        uncut_parameters, (nparams + 1) * sizeof(struct uncut_parameter));
    if (!uncut_parameters)
        return;
    uncut_parameters[nparams - 1].name = name;
    uncut_parameters[nparams - 1].value = value;
    uncut_parameters[nparams - 1].description = description;
    uncut_parameters[nparams].name = NULL;
    uncut_parameters[nparams].value = NULL;
    uncut_parameters[nparams].description = NULL;
}

static void simple_callback(const struct uncut_suite *suite,
                            const struct uncut_test *test, int retval,
                            int test_time)
{
    if (test_time < 0) {
        printf("Running test '%s/%s': ", suite->group_name, test->item_name);
        fflush(stdout);
    } else {
        printf("%s [%d ms]\n", retval ? "failed" : "succeeded", test_time);
    }
}

int main(int argc, char *argv[]) __attribute__((weak));

int main(int argc, char *argv[])
{
    return uncut_suite_run(uncut_tests, uncut_parameters, argc, argv,
                           simple_callback);
}
