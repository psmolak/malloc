CC = gcc
CFLAGS = -ggdb -Og -Wall -Wextra
CPPFLAGS = -DDEBUG $(shell pkg-config --cflags libbsd-overlay)
LDLIBS = $(shell pkg-config --libs libbsd-overlay) -Wl,-rpath=.

all: malloc.so test

%.lo: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -fPIC -c -o $@ $<

%.so: %.lo
	$(CC) -shared $^ -ldl -o $@

debug.lo: debug.c malloc.h
wrappers.lo: wrappers.c malloc.h
malloc.lo: malloc.c malloc.h
malloc.so: debug.lo malloc.lo wrappers.lo arena.lo block.lo invariants.lo

TESTS = $(wildcard tst-*.c)

test: test.o $(TESTS:.c=.o) malloc.so

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f test *.so *.lo *.o *~

.PRECIOUS: %.o
.PHONY: all clean format run

# vim: ts=8 sw=8 noet
