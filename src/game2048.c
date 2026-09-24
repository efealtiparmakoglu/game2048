#include "game2048.h"

#include <string.h>

/* Deterministic xorshift64* RNG: same seed, same spawn sequence. */
uint64_t next_rng(uint64_t *state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* Compress non-zero tiles left, merge equal neighbors once per move.
 * Returns non-zero when the row changed at all (moves AND merges). */
int slide_row_left(int *row, int *score_delta) {
    int before[GRID];
    memcpy(before, row, sizeof(before));
    int temp[GRID];
    int n = 0;
    for (int i = 0; i < GRID; i++) {
        if (row[i] != 0) temp[n++] = row[i];
    }
    int merged = 0;
    int out = 0;
    *score_delta = 0;
    for (int i = 0; i < n; i++) {
        if (i + 1 < n && temp[i] == temp[i + 1]) {
            row[out++] = temp[i] * 2;
            *score_delta += temp[i] * 2;
            i++; /* consume both */
            merged++;
        } else {
            row[out++] = temp[i];
        }
    }
    for (; out < GRID; out++) row[out] = 0;
    return memcmp(before, row, sizeof(before)) != 0 ? (merged + 1) : 0;
}

int slide_row_right(int *row, int *score_delta) {
    int reversed[GRID];
    for (int i = 0; i < GRID; i++) reversed[i] = row[GRID - 1 - i];
    int merged = slide_row_left(reversed, score_delta);
    for (int i = 0; i < GRID; i++) row[i] = reversed[GRID - 1 - i];
    return merged;
}

void rotate_left(int grid[GRID][GRID]) {
    int temp[GRID][GRID];
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            temp[GRID - 1 - c][r] = grid[r][c];
        }
    }
    memcpy(grid, temp, sizeof(temp));
}

void rotate_right(int grid[GRID][GRID]) {
    int temp[GRID][GRID];
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            temp[c][GRID - 1 - r] = grid[r][c];
        }
    }
    memcpy(grid, temp, sizeof(temp));
}

static bool move_grid(Game *g, int rotations) {
    for (int i = 0; i < rotations; i++) rotate_left(g->cells);
    int changed = 0;
    uint64_t gained = 0;
    for (int r = 0; r < GRID; r++) {
        int delta = 0;
        changed += slide_row_left(g->cells[r], &delta);
        gained += (uint64_t)delta;
    }
    for (int i = 0; i < rotations; i++) rotate_right(g->cells);
    g->score += gained;
    return changed != 0;
}

bool session_can_move(const Game *g) {
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (g->cells[r][c] == 0) return true;
            if (c + 1 < GRID && g->cells[r][c] == g->cells[r][c + 1]) return true;
            if (r + 1 < GRID && g->cells[r][c] == g->cells[r + 1][c]) return true;
        }
    }
    return false;
}

int spawn_tile(Game *g, uint64_t *rng) {
    int empties[GRID * GRID];
    int count = 0;
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (g->cells[r][c] == 0) empties[count++] = r * GRID + c;
        }
    }
    if (count == 0) return 0;
    int square = empties[next_rng(rng) % (uint64_t)count];
    /* 90% a 2, 10% a 4 */
    int value = (next_rng(rng) % 10 == 0) ? 4 : 2;
    g->cells[square / GRID][square % GRID] = value;
    return value;
}

static void push_undo(Session *s) {
    if (s->undoCount < UNDO_DEPTH) {
        s->undo[s->undoCount].game = s->game;
        s->undo[s->undoCount].valid = true;
        s->undoCount++;
    } else {
        memmove(&s->undo[0], &s->undo[1], sizeof(UndoSlot) * (UNDO_DEPTH - 1));
        s->undo[UNDO_DEPTH - 1].game = s->game;
        s->undo[UNDO_DEPTH - 1].valid = true;
    }
}

void session_init(Session *s, uint64_t seed, uint64_t best) {
    memset(s, 0, sizeof(*s));
    s->game.best = best;
    s->rngState = seed ? seed : 1;
    spawn_tile(&s->game, &s->rngState);
    spawn_tile(&s->game, &s->rngState);
}

bool session_undo(Session *s) {
    if (s->undoCount == 0) return false;
    s->undoCount--;
    s->game = s->undo[s->undoCount].game;
    return true;
}

void session_slide(Session *s, int dir) {
    if (s->game.over) return;
    push_undo(s);
    int rotations = 0;
    switch (dir) {
        case 1: rotations = 2; break; /* right */
        case 2: rotations = 1; break; /* up = rotate left once */
        case 3: rotations = 3; break; /* down */
        default: rotations = 0; break; /* left */
    }
    bool changed = move_grid(&s->game, rotations);
    if (!changed) {
        s->undoCount--; /* nothing happened: discard the snapshot */
        return;
    }
    s->game.moves++;
    spawn_tile(&s->game, &s->rngState);
    if (s->game.score > s->game.best) s->game.best = s->game.score;
    if (!s->game.won) {
        for (int r = 0; r < GRID; r++) {
            for (int c = 0; c < GRID; c++) {
                if (s->game.cells[r][c] >= 2048) s->game.won = true;
            }
        }
    }
    if (!session_can_move(&s->game)) s->game.over = true;
}
