# Libuncut

Libuncut is a minimal C testing library for writing unit tests in.

It is designed to make writing unit tests for small C code bases simple, as well as being easy to import into an existing project. It consists of a single C files and a corresponding header.

It supports execution of individual tests either sequentially or in parallel, and provides a simple command line to the user.

## Usage
Libuncut is designed to be used in two ways. Either via macros which automatically construct the test functions at compile/runtime and start the tests immediately, or if more control is required the structures can be created by hand and the test handler called manually.

### Macro invocation
#### test_suite.c
```c
#include <string.h>
#include "uncut.h"

DECLARE_PARAM(foo, "wibble", "Some parameter")

DECLARE_TEST(suite_name, test_name) {
	UNCUT_ASSERT(strcmp(uncut_param("foo"), "wibble") == 0);
	return 0;
}

```

#### Compilation & invocation
```bash
user@host:~/code$ gcc -o test_suite test_suite.c uncut.c -lpthread
user@host:~/code$ ./test_suite -l
Group: suite_name
	1: test_name
Parameters:
	foo             : Some parameter (Default: wibble)
user@host:~/code$ ./test_suite
Running test 'suite_name/test_name': succeeded [0 ms]

SUMMARY: SUCCESS
user@host:~/code$ ./test_suite -p foo=fnord
Running test 'suite_name/test_name': Error: test_suite_nametest_name:7: !strcmp(uncut_param("foo"), "wibble") == 0
failed [0 ms]

SUMMARY: FAILURE
```

### Customised invocation

#### test_suite.c
```c
#include <stdio.h>
#include "uncut.h"

static struct uncut_parameter params[] = {
	{"foo", "wibble", "Some parameter"},
	{NULL, NULL, NULL},
};

static int my_library_test1(void) {
	printf("Test my library 1\n");
	return 0;
}

static int my_library_test2(void) {
	printf("Test my library 2\n");
	return 0;
}

static int my_other_library_test(void) {
	printf("Test a different library\n");
	return 0;
}

static struct uncut_suite suites[] = {
	{
		.group_name = "my_library",
		.tests = (struct uncut_test[]) {
			{"my_library1", my_library_test1},
			{"my_library2", my_library_test2},
			{NULL, NULL},
		},
	}, {
		.group_name = "other_library",
		.tests = (struct uncut_test[]) {
			{"other_library", my_other_library_test},
			{NULL, NULL},
		},
	}, {
		NULL, NULL
	},
};

static void test_callback(const struct uncut_suite *suite,
	const struct uncut_test *test, int retval, int test_time_ms) {
	if (test_time_ms == -1) {
		printf("Running test '%s/%s'\n", suite->group_name, test->item_name);
	} else {
		printf("Finished test '%s/%s': %s [%dms]\n",
			suite->group_name, test->item_name, retval < 0 ? "FAILURE" : "SUCCSES",
			test_time_ms);
	}
}

int main(int argc, char *argv[]) {
	return uncut_suite_run(suites, params, argc, argv, test_callback);
}
```

#### Compilation & invocation
```bash
user@host:~/code$ gcc -o test_suite test_suite.c uncut.c -lpthread
user@host:~/code$ ./test_suite -l
Group: my_library
	1: my_library1
	2: my_library2
Group: other_library
	1: other_library
Parameters:
	foo             : Some parameter (Default: wibble)
user@host:~/code$ ./test_suite -t my_library:2
Running test 'my_library/my_library2'
Test my library 2
Finished test 'my_library/my_library2': SUCCSES [0ms]

SUMMARY: SUCCESS
user@host:~/code$ ./test_suite
Running test 'my_library/my_library1'
Test my library 1
Finished test 'my_library/my_library1': SUCCSES [0ms]
Running test 'my_library/my_library2'
Test my library 2
Finished test 'my_library/my_library2': SUCCSES [0ms]
Running test 'other_library/other_library'
Test a different library
Finished test 'other_library/other_library': SUCCSES [0ms]

SUMMARY: SUCCESS
```

## License

Libuncut is licensed under the public domain. See LICENSE for full details.
