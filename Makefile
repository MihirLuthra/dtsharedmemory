CC=clang
CFLAGS=-Wall -D HAVE_STDATOMIC_H
#Uncomment next line to use libkern/OSAtomic.h and comment the line above
#CFLAGS=-Wall -D HAVE_LIBKERN_OSATOMIC_H

Test_WithMultipleThreads.out: test_dtsharedmemory.c libdtsharedmemory.so
	$(CC) $(CFLAGS) -o $@ test_dtsharedmemory.c -L. -ldtsharedmemory

libdtsharedmemory.so: dtsharedmemory.c dtsharedmemory.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ dtsharedmemory.c -lc

clean:
	rm *.so .shared_Memory_Status .shared_Memory *.out errors.log
