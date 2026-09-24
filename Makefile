CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

all: game2048

game2048: src/main.c src/game2048.c src/game2048.h
	$(CC) $(CFLAGS) -Isrc -o $@ src/main.c src/game2048.c

game_tests: tests/game2048_tests.c src/game2048.c src/game2048.h
	$(CC) $(CFLAGS) -Isrc -o game_tests tests/game2048_tests.c src/game2048.c

test: game_tests game2048
	./game_tests
	./game2048 bench >/dev/null && echo "bench OK"

clean:
	rm -f game2048 game_tests
