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
 * @return 0 if all tests run successfully, < 0 otherwise
 */
int uncut_suite_run(const char *suite_name, struct uncut_suite *groups,
        struct uncut_parameter *parameters, int argc, char *argv[]);

/**
 * Can be called from within an 'uncut_function' to set the
 * summary message for the test run.
 * This summary message can be saved in the database/report output
 * @param message String message to add
 * @param global Boolean flag - if not set then this is the message for the
 *          currently executing test, otherwise it is a global
 *          message for the entire test run (multiple global messages
 *          are allowed)
 */
void uncut_set_message(const char *message, int global);

#endif

