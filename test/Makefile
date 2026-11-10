CC     ?= gcc
CFLAGS := -std=gnu11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all

all: test lgcheck

test: test_engine
	./test_engine

test_engine: test_engine.c ../common/engine.c ../common/engine.h ../common/config.c ../common/config.h
	$(CC) $(CFLAGS) -o $@ test_engine.c ../common/engine.c ../common/config.c ../common/progress.c

lgcheck: lgcheck.c ../common/engine.c ../common/engine.h
	$(CC) -std=c11 -O2 -Wall -Wextra -o $@ lgcheck.c ../common/engine.c

clean:
	rm -f test_engine lgcheck *.tmp

.PHONY: all test clean
