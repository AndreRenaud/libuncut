#include <stdio.h>
#include <unistd.h>

#include "uncut.h"

static int pass_test(void)
{
    printf("Pass test\n");
    return 0;
}

static int fail_test(void)
{
    printf("Failed test\n");
    return -1;
}

static int slow_test(void)
{
    sleep(10);
    return 0;
}

struct uncut_parameter params[] = {
    {"myvalue",  "10",   "Some value that the tests use"},
    {NULL, NULL, NULL},
};

struct uncut_test tests[] = {
    {"pass", pass_test},
    {"fail", fail_test},
    {"pass2", pass_test},
    {"slow", slow_test},
    {NULL, NULL},
};

struct uncut_suite suite[] = {
    {"tests", tests},
    {NULL, NULL},
};

static void test_callback(struct uncut_suite *suite, struct uncut_test *test,
                          int retval, int test_time)
{
    if (test_time < 0) {
        printf("Running test '%s/%s': ", suite->group_name, test->item_name);
        fflush(stdout);
    } else {
        printf("%s [%d ms]\n", retval ? "failed" : "succeeded", test_time);
    }
}

int main(int argc, char *argv[])
{
    return uncut_suite_run("Demo tests",suite, params, argc, argv, test_callback);
}
