all: test

test: test.c
	gcc  -o $@ $^ -I$(HOME)/local/include $(HOME)/local/lib/libpapi.a -lpthread

clean:
	rm -f test
