CFLAGS=-g -Wall -pipe

#CFLAGS+=-DUNCUT_USE_SQLITE
#LFLAGS+=-lsqlite3

default: demo_prog

libuncut.a: uncut.o
	$(AR) crs $@ $<

%.o: %.c
	cppcheck $<
	$(CC) -c -o $@ $< $(CFLAGS)

format: FORCE
	astyle -q -n -s4 uncut.c uncut.h demo_prog.c

check: FORCE
	cppcheck --std=c99 --enable=style,warning,performance,portability,unusedFunction --quiet demo_prog.c uncut.c uncut.h
	astyle -s4 < uncut.c | colordiff -u uncut.c -
	astyle -s4 < uncut.h | colordiff -u uncut.h -
	astyle -s4 < demo_prog.c | colordiff -u demo_prog.c -

demo_prog: libuncut.a demo_prog.o
	$(CC) -o $@ demo_prog.o libuncut.a $(LFLAGS)

FORCE:

clean:
	rm -f *.o demo_prog
