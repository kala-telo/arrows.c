.PHONY: all clean
LDFLAGS=-lraylib -lm -lomp
CC=cc
CFLAGS=-fopenmp -O3 -Wall -g
TARGET=./build/arrows
SRC=$(wildcard src/*.c)

OBJ=$(subst src/,build/,$(SRC:.c=.o))

build/%.o: src/%.c
	@mkdir -p ./build/
	$(CC) -c $(CFLAGS) -o $@ $^

all: $(OBJ)
	$(CC) $(OBJ) $(CFLAGS) $(LDFLAGS) -o $(TARGET)

native:
	$(MAKE) CFLAGS="$(CFLAGS) -march=native" $(MAKEFLAGS)
clean:
	rm build/*
