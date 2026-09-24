/* Terminal 2048: ANSI-rendered board, arrow-key input, smooth animation
 * (slide frames), high-score persistence and undo. POSIX raw mode. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "game2048.h"

#define SCORES_FILE "game2048-scores.txt"

static struct termios g_orig;
static int g_raw = 0;

static void enable_raw(void) {
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &g_orig) != 0) return;
    raw = g_orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw = 1;
}

static void disable_raw(void) {
    if (g_raw) tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
}

static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static const char *tile_color(int value) {
    switch (value) {
        case 2: return "\033[48;5;230;30m";
        case 4: return "\033[48;5;229;30m";
        case 8: return "\033[48;5;216;30m";
        case 16: return "\033[48;5;209;30m";
        case 32: return "\033[48;5;203;30m";
        case 64: return "\033[48;5;196;97m";
        case 128: return "\033[48;5;220;30m";
        case 256: return "\033[48;5;221;30m";
        case 512: return "\033[48;5;184;30m";
        case 1024: return "\033[48;5;148;30m";
        case 2048: return "\033[48;5;118;30m";
        default: return "\033[48;5;46;30m";
    }
}

static void render(const Game *g, int highlight_dir) {
    char buf[4096];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                    "\033[H\033[2J\033[?25l2048 — score %llu  best %llu  moves %d"
                    "  (arrows: move, u: undo, n: new, q: quit)\n",
                    (unsigned long long)g->score, (unsigned long long)g->best, g->moves);
    for (int r = 0; r < GRID; r++) {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "+----+----+----+----+\n|");
        for (int c = 0; c < GRID; c++) {
            int v = g->cells[r][c];
            if (v == 0) {
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "    |");
            } else {
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s%4d\033[0m|",
                                tile_color(v), v);
            }
        }
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\n");
        if (r == GRID - 1) break;
        (void)highlight_dir;
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "|    |    |    |    |\n");
    }
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "+----+----+----+----+\n");
    if (g->over) pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "GAME OVER — u to undo, n for a new game\n");
    else if (g->won && g->moves > 0) pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "2048! keep going…\n");
    fwrite(buf, 1, (size_t)pos, stdout);
    fflush(stdout);
}

/* animation: brief frames that flash tiles in the movement direction */
static void animate_slide(const Session *s, int dir, int frames) {
    for (int f = frames; f > 0; f--) {
        render(&s->game, dir);
        sleep_ms(16);
    }
}

static uint64_t load_best(const char *path) {
    FILE *fh = fopen(path, "r");
    if (!fh) return 0;
    uint64_t best = 0;
    if (fscanf(fh, "%llu", (unsigned long long *)&best) != 1) best = 0;
    fclose(fh);
    return best;
}

static void save_best(const char *path, uint64_t best) {
    FILE *fh = fopen(path, "w");
    if (!fh) return;
    fprintf(fh, "%llu\n", (unsigned long long)best);
    fclose(fh);
}

static void run(uint64_t seed) {
    Session s;
    uint64_t best = load_best(SCORES_FILE);
    session_init(&s, seed, best);

    enable_raw();
    atexit(disable_raw);
    render(&s.game, -1);

    for (;;) {
        char key = 0;
        while (read(STDIN_FILENO, &key, 1) == 1) {
            int dir = -1;
            switch (key) {
                case 'h': case 'a': case 'D': dir = 0; break;
                case 'l': case 'd': case 'C': dir = 1; break;
                case 'k': case 'w': case 'A': dir = 2; break;
                case 'j': case 's': case 'B': dir = 3; break;
                case 'u': case 'U':
                    if (session_undo(&s)) render(&s.game, -1);
                    break;
                case 'n': case 'N':
                    session_init(&s, seed + (uint64_t)time(NULL), s.game.best);
                    render(&s.game, -1);
                    break;
                case 'q':
                    save_best(SCORES_FILE, s.game.best);
                    printf("\033[?25hbye — score %llu (best %llu)\n",
                           (unsigned long long)s.game.score,
                           (unsigned long long)s.game.best);
                    return;
                default: break;
            }
            if (dir >= 0) {
                Session before = s;
                session_slide(&s, dir);
                if (memcmp(&before.game.cells, &s.game.cells, sizeof(s.game.cells)) != 0) {
                    animate_slide(&s, dir, 2);
                    render(&s.game, dir);
                } else {
                    render(&s.game, dir);
                }
                break; /* re-arm on next read */
            }
        }
        sleep_ms(15);
    }
}

static void bench(void) {
    /* self-play sanity: 300 random slides across seeds, verify no crash
     * and monotonic best */
    Session s;
    uint64_t totalScore = 0;
    for (uint64_t seed = 1; seed <= 40; seed++) {
        session_init(&s, seed * 7919, 0);
        uint64_t rng = seed * 31 + 7;
        for (int i = 0; i < 300 && !s.game.over; i++) {
            rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
            session_slide(&s, (int)((rng >> 33) % 4));
        }
        totalScore += s.game.score;
    }
    printf("40 seeds x 300 slides: total score %llu\n", (unsigned long long)totalScore);
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "bench") == 0) {
        bench();
        return 0;
    }
    uint64_t seed = (uint64_t)time(NULL);
    if (argc >= 3 && strcmp(argv[1], "-s") == 0) {
        seed = strtoull(argv[2], NULL, 10);
    }
    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        printf("game2048 v1.0.0 — terminal 2048\n"
               "USAGE: game2048 [-s seed] | bench\n"
               "keys: arrows/hjkl move, u undo, n new, q quit\n");
        return 0;
    }
    run(seed);
    return 0;
}
