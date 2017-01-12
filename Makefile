CC := /usr/local/src/liballocs/tools/lang/c/bin/allocscc \
-I/usr/local/src/liballocs/include \
-I/usr/local/src/liballocs/include

LDFLAGS += -L/usr/local/src/liballocs/src 
LDLIBS += -lallocs

CFLAGS += 

all: client unit_test

client: client.c

unit_test: unit_tester.o gc.o
	cc -o "$@" unit_tester.o gc.o

unit_tester.o: gc.h pointer_macros.h

gc.o: gc.h pointer_macros.h

.PHONY: clean
clean:
	rm -f *.o *.cil.* client unit_test *.memacc *.fixuplog *.i *.i.allocs *.allocstubs.c

.PHONY: test
test: unit_test
	./unit_test

.PHONY: run
run: client
	LD_PRELOAD=/usr/local/src/liballocs/lib/liballocs_preload.so ./client
