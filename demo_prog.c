#include <stdio.h>

#include "uncut.h"

static int pass_test(void)
{
    printf("Pass test\n");
    uncut_set_message("Pass test was all good", 0);
    return 0;
}

static int fail_test(void)
{
    printf("Fail test: %s\n", uncut_param("param_name"));
    uncut_set_message("Set some global notes", 1);
    return -1;
}

struct uncut_parameter params[] = {
    {"param_name",  "10",   "Parameter"},
    {NULL, NULL, NULL},
};

struct uncut_test tests[] = {
    {"pass", pass_test},
    {"fail", fail_test},
    {NULL, NULL},
};

struct uncut_suite suite[] = {
    {"tests", tests},
    {NULL, NULL},
};

int main(int argc, char *argv[])
{
    return uncut_suite_run("Demo tests",suite, params, argc, argv);
}
