CFLAGS=-g -Wall -pipe

#CFLAGS+=-DUNCUT_USE_SQLITE
#LFLAGS+=-lsqlite3

default: demo_prog

libuncut.a: uncut.o
	$(AR) crs $@ $<

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

demo_prog: libuncut.a demo_prog.o
	$(CC) -o $@ demo_prog.o libuncut.a $(LFLAGS)

clean:
	rm -f *.o demo_prog
