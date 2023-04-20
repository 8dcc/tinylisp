
CC=gcc
CFLAGS=-Wall -Wextra
LDFLAGS=

BIN=tinylisp.out

.PHONY: clean all run

# ------------------------------------------------------------------------------

all: $(BIN)

run: $(BIN)
	./$<

clean:
	rm -f $(BIN)

# ------------------------------------------------------------------------------

%.out : src/%.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

