CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

all: tests

tests: allocator.o tests.o
	$(CC) $(CFLAGS) -o $@ $^

allocator.o: allocator.c allocator.h
	$(CC) $(CFLAGS) -c allocator.c

tests.o: tests.c allocator.h
	$(CC) $(CFLAGS) -c tests.c

clean:
	rm -f *.o tests

.PHONY: all clean
