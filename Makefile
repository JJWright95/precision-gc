LIBALLOCS ?= /usr/local/src/liballocs
LIBDLBIND ?= /usr/local/src/liballocs/contrib/libdlbind
LIBMALLOCHOOKS ?= /usr/local/src/liballocs/contrib/libmallochooks

CC := $(LIBALLOCS)/tools/lang/c/bin/allocscc \
-I$(LIBALLOCS)/include

LDFLAGS += -L/$(LIBALLOCS)/src
LDLIBS += -lallocs \
	-L$(LIBALLOCS)/src -Wl,-rpath,$(LIBALLOCS)/src \
	-L$(LIBDLBIND)/lib -Wl,-rpath,$(LIBDLBIND)/lib \
	-lallocs  \
	-ldlbind -ldl \

#        -Wl,--defsym,__wrap_gc_malloc=__real_gc_malloc

all: client unit_test gc_bench

mymalloc_usable_size.o: CC := cc
mymalloc_usable_size.o: CFLAGS += -I$(LIBALLOCS)/include -std=c11 -g

debug: CFLAGS += -g -DDEBUG
debug: client unit_test gc_bench
	gdb -ex "set env LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so" \
	-ex "handle SIGILL nostop noprint pass" gc_bench

client: client.o gc.o mymalloc_hooks.o mymalloc.a mymalloc_usable_size.o
	$(CC) $(CFLAGS) -o "$@" $(filter-out mymalloc.a,$+) -Wl,--whole-archive $(filter mymalloc.a,$+) -Wl,--no-whole-archive $(LDFLAGS) $(LDLIBS)
client: LDFLAGS += \
	-Wl,--wrap,__real_mymalloc \
	-Wl,--wrap,__real_mycalloc \
	-Wl,--wrap,__real_myfree \
	-Wl,--wrap,__real_myrealloc \
	-Wl,--wrap,__real_mymemalign \
	-Wl,--wrap,__real_mymalloc_usable_size \
	-Wl,--wrap,myfree \
	-Wl,--defsym,__wrap___real_mymalloc_usable_size=__real_mymalloc_usable_size
	#-Wl,test.lds \
	
gc_bench: GCBench.o gc.o mymalloc_hooks.o mymalloc.a mymalloc_usable_size.o
	$(CC) $(CFLAGS) -o "$@" $(filter-out mymalloc.a,$+) -Wl,--whole-archive $(filter mymalloc.a,$+) -Wl,--no-whole-archive $(LDFLAGS) $(LDLIBS)
gc_bench: LDFLAGS += \
	-Wl,--wrap,__real_mymalloc \
	-Wl,--wrap,__real_mycalloc \
	-Wl,--wrap,__real_myfree \
	-Wl,--wrap,__real_myrealloc \
	-Wl,--wrap,__real_mymemalign \
	-Wl,--wrap,__real_mymalloc_usable_size \
	-Wl,--wrap,myfree \
	-Wl,--defsym,__wrap___real_mymalloc_usable_size=__real_mymalloc_usable_size
	#-Wl,test.lds \
	
GCBench.o: gc.h
	$(CC) $(CFLAGS) -c GCBench.c -DGC

LIBALLOCS_ALLOC_FNS := gc_malloc(Z)p gc_realloc(pZ)p mymalloc(Z)p mycalloc(zZ)p myrealloc(pZ)p mymemalign(zZ)p
export LIBALLOCS_ALLOC_FNS

mymalloc.a: mymalloc.o
	$(AR) r "$@" $+

unit_test: unit_tester.o gc.o mymalloc_hooks.o mymalloc.a mymalloc_usable_size.o
	$(CC) $(CFLAGS) -o "$@" $(filter-out mymalloc.a,$+) -Wl,--whole-archive $(filter mymalloc.a,$+) -Wl,--no-whole-archive $(LDFLAGS) $(LDLIBS)
#unit_test: LDLIBS += gc.o mymalloc.o
unit_test: LDFLAGS += \
	-Wl,--wrap,__real_mymalloc \
	-Wl,--wrap,__real_mycalloc \
	-Wl,--wrap,__real_myfree \
	-Wl,--wrap,__real_myrealloc \
	-Wl,--wrap,__real_mymemalign \
	-Wl,--wrap,__real_mymalloc_usable_size \
	-Wl,--wrap,myfree \
	-Wl,--defsym,__wrap___real_mymalloc_usable_size=__real_mymalloc_usable_size \
    -Wl,--exclude-libs=ALL
	#-Wl,test.lds \

unit_tester.o: gc.h

gc.o: gc.h
	$(CC) $(CFLAGS) -c gc.c

dlmalloc.o: dlmalloc.h
	$(CC) $(CFLAGS) -c dlmalloc.c -DUSE_DL_PREFIX -DDEFAULT_TRIM_THRESHOLD=MAX_SIZE_T -DDEFAULT_MMAP_THRESHOLD=MAX_SIZE_T -DHAVE_REMAP=0 -DHAVE_MORECORE=0


vpath %.c $(LIBMALLOCHOOKS)

event_hooks_mymalloc.o: CFLAGS += -D'ALLOC_EVENT(s)=__liballocs_malloc_\#\#s' -DALLOC_EVENT_ATTRIBUTES=
event_hooks_mymalloc.o: event_hooks.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o "$@" $+

mymalloc_hooks.o: event_hooks_mymalloc.o mymalloc_hook_wrappers.o
	$(LD) -r -o "$@" $+

mymalloc_hooks.o event_hooks_mymalloc.o: CFLAGS += -D__next_hook_init=__terminal_hook_init \
	-D__next_hook_malloc=__terminal_hook_malloc \
	-D__next_hook_free=__terminal_hook_free \
	-D__next_hook_realloc=__terminal_hook_realloc \
	-D__next_hook_memalign=__terminal_hook_memalign 

mymalloc_hook_wrappers.o: CFLAGS += -I$(LIBALLOCS)/include -D'MALLOC_PREFIX(p,s)=p \#\# __real_my \#\# s'
mymalloc_hook_wrappers.o: malloc_hook_stubs_wrapdl.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o "$@" $+

dlmalloc_api_syms := malloc calloc realloc free memalign posix_memalign malloc_usable_size \
    malloc_footprint malloc_max_footprint malloc_footprint_limit malloc_set_footprint_limit \
    malloc_trim malloc_stats

mymalloc.o: dlmalloc.o
	out="$$(mktemp)"; \
        objcopy $(foreach s,$(dlmalloc_api_syms),\
--redefine-sym dl$(s)=my$(s)) "$<" "$$out" && \
        $(LD) -o "$@" -r $(foreach s,$(dlmalloc_api_syms),\
--defsym __real_my$(s)=my$(s)) "$$out"; \
	rm -f "$$out"	


.PHONY: clean
clean:
	rm -f *.o *.cil.* *.memacc *.fixuplog *.i *.i.allocs *.allocstubs.c
	rm -f client unit_test gc_bench
	rm -f -- -ldl.res

.PHONY: run
run: client
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./client
	
.PHONY: bench
bench: gc_bench
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./gc_bench

.PHONY: test
test: unit_test
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./unit_test
	
