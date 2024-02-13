.PHONY: all debug utils clean windows obj
LDFLAGS=-lraylib -lm -lomp
CC=cc
CFLAGS=-fopenmp -O3 -Wall
TARGET=./build/arrows
MAIN_SRC=$(wildcard src/*.c)

OBJ=$(subst src/,build/,$(MAIN_SRC:.c=.o))

build/%.o: src/%.c
	@mkdir -p ./build/
	$(CC) -c $(CFLAGS) -o $@ $^

all: $(OBJ)
	$(CC) $(OBJ) $(CFLAGS) $(LDFLAGS) -o $(TARGET)
