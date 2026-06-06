CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -I./include
LDFLAGS = -lcurl -lm

SRC = src/annealer.c \
      src/guardian.c \
      src/mutator.c

BIN = annealer

.PHONY: all clean run valgrind

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(BIN)
	./$(BIN) ../seeds/seed_01.txt

valgrind: $(BIN)
	valgrind --leak-check=full --track-origins=yes \
	         ./$(BIN) ../seeds/seed_01.txt

clean:
	rm -f $(BIN)

