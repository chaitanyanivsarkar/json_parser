CC = gcc
CFLAGS = -Ofast -Wall -Wextra -std=c2x -lm -c
CFLAGS_TEST = -DTEST -DJP_USE_LIB_ALLOC -Ofast -Wall -Wextra -std=c2x -lm
CFLAGS_PERF = -DTEST -DJP_USE_LIB_ALLOC -DPERF_TEST -Ofast -Wall -Wextra -std=c2x -lm

all: libjson.o

libjson.o: parser.c
	$(CC) $(CFLAGS) parser.c -o libjson.o

test: parser.o
	$(CC) $(CFLAGS_TEST) parser.c -o test_program
	./test_program > test_results.txt

perf: parser.o
	$(CC) $(CFLAGS_PERF) parser.c -o perf_program
	./perf_program > perf_results.txt

clean:
	rm -f libjson.o parser.o test_program perf_program test_results.txt perf_results.txt
