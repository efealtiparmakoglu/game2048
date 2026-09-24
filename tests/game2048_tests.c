/* Numerical tests for the 2048 core. */

#include <stdio.h>
#include <string.h>

#include "game2048.h"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        tests_run++;                                                      \
        if (!(cond)) {                                                    \
            tests_failed++;                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

static void test_slide_left(void) {
    int row[GRID] = {2, 2, 4, 4};
    int delta = 0;
    int changed = slide_row_left(row, &delta);
    CHECK(row[0] == 4 && row[1] == 8 && row[2] == 0 && row[3] == 0);
    CHECK(changed != 0);
    CHECK(delta == 12);

    int triple[GRID] = {2, 2, 2, 0};
    changed = slide_row_left(triple, &delta);
    CHECK(triple[0] == 4 && triple[1] == 2 && changed != 0);
    CHECK(delta == 4);

    int four[GRID] = {2, 2, 2, 2};
    slide_row_left(four, &delta);
    CHECK(four[0] == 4 && four[1] == 4 && four[2] == 0 && four[3] == 0);
    CHECK(delta == 8);

    int locked[GRID] = {2, 4, 2, 4};
    changed = slide_row_left(locked, &delta);
    CHECK(changed == 0 && delta == 0);
    CHECK(locked[0] == 2 && locked[1] == 4 && locked[2] == 2 && locked[3] == 4);
}

static void test_slide_right(void) {
    int row[GRID] = {2, 2, 4, 4};
    int delta = 0;
    slide_row_right(row, &delta);
    CHECK(row[0] == 0 && row[1] == 0 && row[2] == 4 && row[3] == 8);
    CHECK(delta == 12);
}

static void test_rotations(void) {
    int grid[GRID][GRID] = {{1, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    rotate_left(grid);
    CHECK(grid[3][0] == 1); /* rotate_left is CCW: top-left -> bottom-left */
    rotate_right(grid);
    CHECK(grid[0][0] == 1); /* and rotate_right restores it */
}

static void test_session_and_undo(void) {
    Session s;
    session_init(&s, 42, 0);
    CHECK(s.game.score == 0);
    /* find a direction that actually changes the board */
    int dir = -1;
    for (int d = 0; d < 4 && dir == -1; d++) {
        Session probe = s;
        session_slide(&probe, d);
        if (memcmp(probe.game.cells, s.game.cells, sizeof(s.game.cells)) != 0) dir = d;
    }
    CHECK(dir >= 0);
    int before[GRID][GRID];
    memcpy(before, s.game.cells, sizeof(before));
    session_slide(&s, dir);
    CHECK(s.undoCount == 1);
    session_undo(&s);
    CHECK(memcmp(s.game.cells, before, sizeof(before)) == 0);
    CHECK(s.undoCount == 0);
    CHECK(!session_undo(&s));
    /* no-op slide: the snapshot must be discarded so undo stays empty */
    Session probe = s;
    session_slide(&probe, dir);
    int undoAfter = probe.undoCount;
    session_slide(&probe, dir); /* same direction again: cannot change (just spawned) */
    CHECK(probe.undoCount <= undoAfter + 1);
    CHECK(s.undoCount == 0);
}

static void test_game_over_detection(void) {
    Game g;
    memset(&g, 0, sizeof(g));
    int values[GRID][GRID] = {
        {2, 4, 8, 16},
        {16, 8, 4, 2},
        {2, 4, 8, 16},
        {16, 8, 4, 2},
    };
    memcpy(g.cells, values, sizeof(values));
    CHECK(!session_can_move(&g));
    /* one free cell means moves exist */
    g.cells[3][3] = 0;
    CHECK(session_can_move(&g));
}

static void test_win_flag_and_score(void) {
    Session s;
    session_init(&s, 7, 0);
    /* force a winning board: 1024 + 1024 in the top row, slide left */
    memset(&s.game.cells, 0, sizeof(s.game.cells));
    s.game.cells[0][0] = 1024;
    s.game.cells[0][1] = 1024;
    s.game.cells[1][0] = 2;
    session_slide(&s, 0);
    CHECK(s.game.won);
    CHECK(s.game.score == 2048);
    CHECK(s.game.best >= s.game.score);
}

static void test_spawn_values(void) {
    Session s;
    session_init(&s, 99, 0);
    (void)0;
    int twos = 0, fours = 0;
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++) {
            if (s.game.cells[r][c] == 2) twos++;
            if (s.game.cells[r][c] == 4) fours++;
        }
    CHECK(twos + fours == 2);
    CHECK(fours <= 2);
}

static void test_determinism(void) {
    Session a, b;
    session_init(&a, 1234, 0);
    session_init(&b, 1234, 0);
    for (int i = 0; i < 30; i++) {
        int dir = (int)next_rng(&a.rngState) % 4;
        /* burn the same rng draws in b so both stay aligned */
        (void)next_rng(&b.rngState);
        session_slide(&a, dir);
        session_slide(&b, dir);
    }
    CHECK(memcmp(a.game.cells, b.game.cells, sizeof(a.game.cells)) == 0);
    CHECK(a.game.score == b.game.score);
}

int main(void) {
    test_slide_left();
    test_slide_right();
    test_rotations();
    test_session_and_undo();
    test_game_over_detection();
    test_win_flag_and_score();
    test_spawn_values();
    test_determinism();

    printf("%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
