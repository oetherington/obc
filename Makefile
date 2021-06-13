SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
TARGET=obc

CC=clang
CFLAGS=-c -std=c99 -W -Wall -Wextra -Wno-unused-parameter -Wswitch-enum

LD=clang
LDFLAGS=

.PHONY: $(TARGET) run clean

all: debug

debug: CFLAGS += -g3 -O1 -fno-omit-frame-pointer
debug: $(TARGET)

ndebug: CFLAGS += -O3 -fomit-frame-pointer
ndebug: LDFLAGS += -s
ndebug: $(TARGET)

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

runv: $(TARGET)
	valgrind ./$(TARGET)
