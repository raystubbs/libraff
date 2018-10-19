CFLAGS = -Wall -Werror -std=c99 -g

build: raff.c raff.h
	$(CC) $(CFLAGS) -fpic -c raff.c
	$(CC) -shared raff.o -o libraff.so
	ar rcs libraff.a raff.o

test: build test-gen.c test-parse.c
	$(CC) test-gen.c libraff.a -o test-gen
	$(CC) test-parse.c libraff.a -o test-parse
	rm -f sample.wav
	./test-gen
	./test-parse

clean:
	rm -f *.o
	rm -f *.so
	rm -f *.a
