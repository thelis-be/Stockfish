# Stockfish for QuantumChess

This branch (`quantumchess`) is [Stockfish 19](https://github.com/official-stockfish/Stockfish/releases/tag/sf_19)
(`sf_19`) playing the QuantumChess variant, where **files wrap around**: a piece leaving through the
h-file comes back through the a-file and vice versa (ranks do not wrap, a slider stops before
returning to its origin). The UCI name ends with `QuantumChess`.

## Changes from sf_19

- `src/wrap.h`, `src/wrap.cpp`: the wrapped geometry (pseudo attacks, sliding attacks per line, the
  up to two paths joining two squares, `between_bb`, `path_clear`).
- The game rules use it: move generation (`movegen.cpp`, pawn captures through the edge), legality,
  checks and discovered checks, pins with two paths, SEE, en passant and upcoming repetitions
  (`position.cpp`, `position.h`; the cuckoo tables are twice as large).
- The NNUE input features keep the standard geometry of `attacks.h`: the network was trained on
  regular chess and its incremental threat updates rely on unique lines between two squares.
- `main.cpp` (`Wrap::init()`), `misc.cpp` (name suffix), `Makefile` (new files).

## Docker image

The [`Dockerfile`](Dockerfile) builds the image used by the QuantumChess API:

| Path | Content |
|------|---------|
| `/usr/local/bin/stockfish` | official sf_19 release, `linux-x86-64-universal` (standard variant) |
| `/usr/local/bin/stockfish-wrap` | this fork, `x86-64-universal` (QuantumChess variant) |
| `/usr/share/doc/stockfish/` | `Copying.txt` (GPLv3) and `AUTHORS` |

```bash
docker build -t stockfish-quantumchess .
```

The API image copies both executables from it (`COPY --from=...`). The image also runs on its own,
speaking UCI on stdin/stdout:

```bash
docker run --rm -i stockfish-quantumchess
```

Windows builds for local development (`stockfish.exe` from the official release,
`stockfish-wrap.exe` AVX2, plus the licence files), written to `<dir>`:

```bash
docker build --target windows -o <dir> .
```

Both targets can be built straight from GitHub, without a checkout:

```bash
docker build --target windows -o stockfish https://github.com/thelis-be/Stockfish.git#quantumchess
```

The NNUE network is exported from the official binary (`export_net`), so the build only downloads
the base images and the two sf_19 release archives (SHA-256 pinned in the Dockerfile).

## Updating

The fork is pinned to `sf_19`, like the official binary it plays next to. Upstream `master` has
since switched to legal-only move generation (pins along a single line, `threats_by`), which does
not hold with wrapped files: moving to a newer Stockfish means porting `wrap.h` and the
legality code again, then updating the release URL and checksums in the Dockerfile.
