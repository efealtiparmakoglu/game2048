/* game2048.h — 2048 core: board, slide/merge logic, undo stack, score,
 * spawn, win/lose detection. Pure logic; terminal I/O lives in main.c. */
#ifndef GAME2048_H
#define GAME2048_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define GRID 4
#define UNDO_DEPTH 16

typedef struct {
    int cells[GRID][GRID]; /* 0 empty, else the tile value (2, 4, ...) */
    uint64_t score;
    uint64_t best;
    int moves;
    bool won;              /* reached 2048 at least once */
    bool over;             /* no moves left */
} Game;

typedef struct {
    Game game;
    bool valid;
} UndoSlot;

typedef struct {
    Game game;
    UndoSlot undo[UNDO_DEPTH];
    int undoCount;
    uint64_t rngState;
} Session;

void session_init(Session *s, uint64_t seed, uint64_t best);
bool session_undo(Session *s);            /* true when a state was restored */
void session_slide(Session *s, int dir);  /* 0=left 1=right 2=up 3=down */
bool session_can_move(const Game *g);

/* exposed for tests */
int slide_row_left(int *row, int *score_delta);      /* returns merged count */
int slide_row_right(int *row, int *score_delta);
void rotate_left(int grid[GRID][GRID]);
void rotate_right(int grid[GRID][GRID]);
int spawn_tile(Game *g, uint64_t *rng);                              /* returns the value spawned */
uint64_t next_rng(uint64_t *state);

#endif /* GAME2048_H */
