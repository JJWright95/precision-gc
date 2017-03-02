LIBALLOCS ?= /usr/local/src/liballocs
LIBDLBIND ?= /usr/local/src/liballocs/contrib/libdlbind

CC := $(LIBALLOCS)/tools/lang/c/bin/allocscc \
-I$(LIBALLOCS)/include

LDFLAGS += -L/$(LIBALLOCS)/src
LDLIBS += -lallocs \
	-L$(LIBALLOCS)/src -Wl,-rpath,$(LIBALLOCS)/src \
	-L$(LIBDLBIND)/lib -Wl,-rpath,$(LIBDLBIND)/lib \
	-lallocs  \
	-ldlbind -ldl \

#        -Wl,--defsym,__wrap_gc_malloc=__real_gc_malloc

all: client unit_test

debug: CFLAGS += -g -DDEBUG
debug: client unit_test
	gdb -ex "set env LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so" \
	-ex "handle SIGILL nostop noprint pass" client

client: client.o gc.o malloc.o
	$(CC) $(CFLAGS) -o "$@" $+ $(LDFLAGS) $(LDLIBS)
client: LDFLAGS += \
	-Wl,--wrap,__real_malloc \
	-Wl,--wrap,__real_calloc \
	-Wl,--wrap,__real_free \
	-Wl,--wrap,__real_realloc \
	-Wl,--wrap,__real_memalign

LIBALLOCS_ALLOC_FNS := gc_malloc(Z)p gc_realloc(pZ)p
export LIBALLOCS_ALLOC_FNS

unit_test: unit_tester.o gc.o malloc.o
	$(CC) $(CFLAGS) -o "$@" unit_tester.o gc.o malloc.o
unit_test: LDLIBS += gc.o malloc.o
unit_test: LDFLAGS += \
	-Wl,--wrap,__real_malloc \
	-Wl,--wrap,__real_calloc \
	-Wl,--wrap,__real_free \
	-Wl,--wrap,__real_realloc \
	-Wl,--wrap,__real_memalign

unit_tester.o: gc.h

gc.o: gc.h
	$(CC) $(CFLAGS) -c gc.c

malloc.o: malloc.h
	$(CC) $(CFLAGS) -c malloc.c -DDEFAULT_TRIM_THRESHOLD=MAX_SIZE_T -DHAVE_MMAP=0 -DHAVE_REMAP=0

.PHONY: clean
clean:
	rm -f *.o *.cil.* *.memacc *.fixuplog *.i *.i.allocs *.allocstubs.c
	rm -f client unit_test

.PHONY: run
run: client
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./client

.PHONY: test
test: unit_test
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./unit_test
	
