CC := /usr/local/src/liballocs/tools/lang/c/bin/allocscc \
-I/usr/local/src/liballocs/include

LDFLAGS += -L/usr/local/src/liballocs/src
LDLIBS += -lallocs

all: client unit_test

debug: CFLAGS += -g -DDEBUG
debug: client unit_test
	gdb -ex "set env LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so" \
	-ex "handle SIGILL nostop noprint pass" client

client: client.o gc.o
	$(CC) $(CFLAGS) -o "$@" client.o gc.o
client: LDLIBS += gc.o

LIBALLOCS_ALLOC_FNS := gc_malloc(Z)p gc_realloc(pZ)p
export LIBALLOCS_ALLOC_FNS

unit_test: unit_tester.o gc.o
	$(CC) $(CFLAGS) -o "$@" unit_tester.o gc.o

unit_tester.o: gc.h

gc.o: gc.h
	$(CC) $(CFLAGS) -c gc.c

.PHONY: clean
clean:
	rm -f *.o *.cil.* client unit_test *.memacc *.fixuplog *.i *.i.allocs *.allocstubs.c

.PHONY: run
run: client
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./client

.PHONY: test
test: unit_test
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./unit_test
	
