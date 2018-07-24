#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "uncut.h"

DECLARE_TEST(demo, pass)
{
    return 0;
}

DECLARE_TEST(demo, fail)
{
    return -1;
}

DECLARE_TEST(demo, slow)
{
    sleep(10);
    return 0;
}

DECLARE_TEST(demo, slow_fail)
{
    sleep(10);
    return -1;
}

DECLARE_TEST(demo2, param)
{
    UNCUT_EQ(uncut_param_int("myinteger"), 10);
    UNCUT_ASSERT(strcmp(uncut_param("myvalue"), "somevalue") == 0);
    return 0;
}

DECLARE_PARAM(myvalue, "somevalue", "Some value that the tests use")
DECLARE_PARAM(myinteger, "10", "Some integer value that the tests use")
