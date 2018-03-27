/**
 * \file    uncut.h
 * \date    6 August 2012
 * \author  Andre Renaud
 * \copyright Aiotec/Bluewater Systems
 * \brief   Test framework for running simple unit tests
 */
#ifndef UNCUT_H
#define UNCUT_H

typedef int (*uncut_function)(void);

/**
 * Defines an individual test
 */
struct uncut_test {
    const char *item_name;
    uncut_function function;
};

/**
 * Defines a list of tests that are in some way related
 * (ie: all for the same subsystem)
 * A uncut_test with a NULL name and/or function indicates the end
 * of the list
 */
struct uncut_suite {
    const char *group_name;
    struct uncut_test *tests;
};

/**
 * Defines all of the command-line parameters that can be configured
 */
struct uncut_parameter {
    const char *name;
    const char *value;
    const char *description;
};

/**
 * Callback called before and after each test execution.
 * When called before a test, test_time_ms will be -1
 * When callde after a test, test_time_ms will be >= 0
 * @param suite Test suite which the test is a part of
 * @param test Test being run
 * @param retval Return value of the test function (== 0 if being called before the test execution)
 * @param test_time_ms Time (in milli-seconds) that the test took to run. == -1 if being called before test execution
 */
typedef void (*uncut_callback)(struct uncut_suite *suite, struct uncut_test *test, int retval, int test_time_ms);

/**
 * Retrieve a parameter, as passed on the command line.
 * This function can only be used from within a test function
 * @param name Name of parameter to get value of
 * @return NULL if parameter is not found, parameter value otherwise
 */
const char *uncut_param(const char *name);

/**
 * Retrieve a parameter, as passed on the command line, and convert it to
 * an integer
 * This function can only be used from within a test function
 * @param name Name of parameter to get value of
 * @return -1 if parameter is not found, parameter value otherwise
 */
int uncut_param_int(const char *name);

/**
 * Run a list of test groups with the given default parameters.
 * @param suite_name Name of the suite of tests that are being run
 * @param groups NULL terminated list of test suite groups
 * @param parameters NULL terminated list of test suite parameters
 * @param argc Command line argument count
 * @param argv Command line argument list
 * @param cb Callback to call on completion of each test
 * @return 0 if all tests run successfully, < 0 otherwise
 */
int uncut_suite_run(const char *suite_name, struct uncut_suite *groups,
                    struct uncut_parameter *parameters, int argc, char *argv[],
                    uncut_callback cb);

#endif

