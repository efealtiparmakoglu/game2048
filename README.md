# game2048

Terminal 2048 in C11 — ANSI-colored tiles, **slide animation**, undo
stack (16 states), persistent best score, deterministic seed mode. Zero
dependencies.

```text
$ game2048 -s 42
2048 — score 1256  best 3480  moves 87  (arrows: move, u: undo, n: new, q: quit)
+----+----+----+----+
|  16|   8|   4|   2|
...
```

## The core

- **Slide/merge** — compress-then-merge per row: `2 2 2` becomes `4 2`
  (one merge per pair per move, guideline rule). The changed-detection
  counts *any* movement, not just merges — the first implementation
  counted merges only, so pure slides were dropped as no-ops and the
  undo test caught it immediately.
- **Undo** — a 16-deep snapshot ring; no-op slides discard their
  snapshot so undo never wastes a state on nothing.
- **Spawns** — xorshift64* RNG: 90% a 2, 10% a 4; same seed, same game.
- **Speed curve** — the board renders 2 animation frames per slide at
  60fps-ish, then the final state; `best` persists to
  `game2048-scores.txt`.

## Keys

arrows / `hjkl` move · `u` undo · `n` new game · `q` quit

## CLI

```
game2048 [-s seed]
game2048 bench        # 40 seeds x 300 random slides: crash + sanity check
```

## Development

```console
$ make test    # 30 checks: slide/merge math, rotations, undo ring,
               # game-over detection, win flag, spawn values, determinism
```

## License

MIT — see [LICENSE](LICENSE).
