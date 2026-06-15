CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c23 -g -D_POSIX_C_SOURCE=200809L
INCLUDES = -Iinclude
SRC = $(shell find src -name '*.c')
OBJ = $(SRC:.c=.o)
TARGET = chillssh

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(TARGET)

compile_commands:
	make clean
	bear -- make

.PHONY: all clean compile_commands
