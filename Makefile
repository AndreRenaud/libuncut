CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic -Wsuggest-attribute=const -Wsuggest-attribute=format -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wold-style-declaration -Wmissing-parameter-type -Woverride-init -Wtype-limits -Wuninitialized -Wunused-but-set-parameter
LFLAGS=-lpthread

CHECKARGS=--std=c11 --error-exitcode=1 --enable=style,warning,performance,portability,unusedFunction --quiet
CLANG_FORMAT?=clang-format
CLANG_TIDY?=clang-tidy

default: demo_prog

libuncut.a: uncut.o
	$(AR) crs $@ $<

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

format: FORCE
	$(CLANG_FORMAT) -i uncut.c uncut.h demo_prog.c

check: FORCE
	cppcheck $(CHECKARGS) demo_prog.c uncut.c uncut.h
	$(CLANG_FORMAT) uncut.c | colordiff -u uncut.c -
	$(CLANG_FORMAT) uncut.h | colordiff -u uncut.h -
	$(CLANG_FORMAT) demo_prog.c | colordiff -u demo_prog.c -
	$(CLANG_TIDY) uncut.c
	$(CLANG_TIDY) demo_prog.c

demo_prog: libuncut.a demo_prog.o
	$(CC) -o $@ demo_prog.o libuncut.a $(LFLAGS)

FORCE:

clean:
	rm -f *.o demo_prog
