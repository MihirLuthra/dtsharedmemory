CC=clang
CFLAGS=-Wall -D HAVE_STDATOMIC_H -D HAVE_DECL_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT=1
#Uncomment next line to use libkern/OSAtomic.h and comment the line above
#CFLAGS=-Wall -D HAVE_LIBKERN_OSATOMIC_H -D HAVE_OSATOMICCOMPAREANDSWAPPTR -D HAVE_OSATOMICCOMPAREANDSWAP64

Test_WithMultipleThreads.out: test_dtsharedmemory.c libdtsharedmemory.so
	$(CC) $(CFLAGS) -o $@ test_dtsharedmemory.c -L. -ldtsharedmemory

libdtsharedmemory.so: dtsharedmemory.c dtsharedmemory.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ dtsharedmemory.c -lc

clean:
	rm *.so macports-dtsm-* *.out errors.log
