/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// QuantumChess: geometry of the QuantumChess board, whose files wrap around
// (a piece leaving through the h-file comes back through the a-file and vice
// versa; ranks do not wrap, a slider stops before returning to its origin).
//
// The game rules (move generation, legality, checks, SEE, repetitions) use
// this geometry. The NNUE input features keep the standard geometry of
// attacks.h: the network was trained on regular chess and its incremental
// threat updates rely on unique lines between two squares.
//
// With wrapping, two squares can be joined by two different paths: two squares
// of a rank (eastwards and westwards), or two squares four ranks and four files
// apart (along both diagonals). Paths keeps both of them.

#ifndef WRAP_H_INCLUDED
#define WRAP_H_INCLUDED

#include <array>
#include <cassert>
#include <utility>

#include "bitboard.h"
#include "types.h"

namespace Stockfish::Wrap {

void init();

// Square reached from s by (df, dr) with wrapped files, or SQ_NONE off the board
constexpr Square offset(Square s, int df, int dr) {
    int r = int(rank_of(s)) + dr;
    if (r < 0 || r > 7)
        return SQ_NONE;
    int f = ((int(file_of(s)) + df) % 8 + 8) % 8;
    return make_square(File(f), Rank(r));
}

constexpr Bitboard rotate_east(Bitboard b) { return ((b & ~FileHBB) << 1) | ((b & FileHBB) >> 7); }
constexpr Bitboard rotate_west(Bitboard b) { return ((b & ~FileABB) >> 1) | ((b & FileABB) << 7); }

// Moves a bitboard one or two steps as specified by the direction D, files wrapping
constexpr Bitboard shift(Bitboard b, Direction dir) {
    return dir == NORTH         ? b << 8
         : dir == SOUTH         ? b >> 8
         : dir == NORTH + NORTH ? b << 16
         : dir == SOUTH + SOUTH ? b >> 16
         : dir == EAST          ? rotate_east(b)
         : dir == WEST          ? rotate_west(b)
         : dir == NORTH_EAST    ? rotate_east(b) << 8
         : dir == NORTH_WEST    ? rotate_west(b) << 8
         : dir == SOUTH_EAST    ? rotate_east(b) >> 8
         : dir == SOUTH_WEST    ? rotate_west(b) >> 8
                                : 0;
}

template<Color C>
constexpr Bitboard pawn_attacks_bb(Bitboard b) {
    return C == WHITE ? Wrap::shift(b, NORTH_WEST) | Wrap::shift(b, NORTH_EAST)
                      : Wrap::shift(b, SOUTH_WEST) | Wrap::shift(b, SOUTH_EAST);
}

constexpr int RookSteps[4][2]   = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
constexpr int BishopSteps[4][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

constexpr Bitboard sliding_attack(PieceType pt, Square sq, Bitboard occupied) {
    Bitboard attacks = 0;
    for (const auto& d : (pt == ROOK ? RookSteps : BishopSteps))
        for (Square s = offset(sq, d[0], d[1]); s != SQ_NONE && s != sq; s = offset(s, d[0], d[1]))
        {
            attacks |= square_bb(s);
            if (occupied & square_bb(s))
                break;
        }
    return attacks;
}

constexpr Bitboard step_attack(Square sq, const int (&steps)[8][2]) {
    Bitboard b = 0;
    for (const auto& d : steps)
        if (Square s = offset(sq, d[0], d[1]); s != SQ_NONE)
            b |= square_bb(s);
    return b;
}

constexpr int KnightSteps[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                   {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
constexpr int KingSteps[8][2]   = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                   {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

// Same layout as Attacks::PseudoAttacks: [WHITE] and [BLACK] hold pawn attacks
inline constexpr auto PseudoAttacks = []() constexpr {
    std::array<std::array<Bitboard, SQUARE_NB>, PIECE_TYPE_NB> attacks{};

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        attacks[WHITE][s] = pawn_attacks_bb<WHITE>(square_bb(s));
        attacks[BLACK][s] = pawn_attacks_bb<BLACK>(square_bb(s));

        attacks[KING][s]   = step_attack(s, KingSteps);
        attacks[KNIGHT][s] = step_attack(s, KnightSteps);
        attacks[QUEEN][s] = attacks[BISHOP][s] = sliding_attack(BISHOP, s, 0);
        attacks[QUEEN][s] |= attacks[ROOK][s]  = sliding_attack(ROOK, s, 0);
    }

    return attacks;
}();

// Sliding attacks are looked up per line (rank, file, and the two wrapped
// diagonals, each holding one square per file and per rank): the occupancy of
// the line is gathered into 8 bits ((occupied & mask) >> preShift) * multiplier >> 56.
enum LineKind {
    RANK_LINE,
    FILE_LINE,
    DIAG_LINE,
    ANTI_LINE,
    LINE_NB
};

struct Line {
    Bitboard mask;
    Bitboard multiplier;
    int      preShift;

    unsigned index(Bitboard occupied) const {
        return unsigned((((occupied & mask) >> preShift) * multiplier) >> 56);
    }
};

extern Line     Lines[SQUARE_NB][LINE_NB];
extern Bitboard LineAttacks[SQUARE_NB][LINE_NB][256];

inline Bitboard line_attacks(Square s, LineKind k, Bitboard occupied) {
    return LineAttacks[s][k][Lines[s][k].index(occupied)];
}

// Returns the pseudo attacks of the given piece type assuming an empty board
template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Color c = COLOR_NB) {
    assert((Pt != PAWN || c < COLOR_NB) && is_ok(s));
    return Pt == PAWN ? PseudoAttacks[c][s] : PseudoAttacks[Pt][s];
}

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied) {
    assert(Pt != PAWN && is_ok(s));

    switch (Pt)
    {
    case BISHOP :
        return line_attacks(s, DIAG_LINE, occupied) | line_attacks(s, ANTI_LINE, occupied);
    case ROOK :
        return line_attacks(s, RANK_LINE, occupied) | line_attacks(s, FILE_LINE, occupied);
    case QUEEN :
        return attacks_bb<BISHOP>(s, occupied) | attacks_bb<ROOK>(s, occupied);
    default :
        return PseudoAttacks[Pt][s];
    }
}

// [bishop, rook]
inline std::pair<Bitboard, Bitboard> both_attacks_bb(Square s, Bitboard occupied) {
    return {attacks_bb<BISHOP>(s, occupied), attacks_bb<ROOK>(s, occupied)};
}

inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) {
    assert(pt != PAWN && is_ok(s));

    switch (pt)
    {
    case BISHOP :
        return attacks_bb<BISHOP>(s, occupied);
    case ROOK :
        return attacks_bb<ROOK>(s, occupied);
    case QUEEN :
        return attacks_bb<QUEEN>(s, occupied);
    default :
        return PseudoAttacks[pt][s];
    }
}

// The (at most two) sliding paths joining two squares, end squares excluded.
// An empty path joins two adjacent squares.
struct Paths {
    Bitboard path[2];
    int      count;
};

extern Paths    PathsBB[SQUARE_NB][SQUARE_NB];
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];

inline const Paths& paths(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return PathsBB[s1][s2];
}

// Union of the paths from s1 to s2, plus s2: a superset of the squares that
// block a check or capture the checker (Position::legal() decides exactly).
inline Bitboard between_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return BetweenBB[s1][s2];
}

// True when s1 and s2 are not joined by a sliding path, or when one of the
// paths holds no piece of occupied
inline bool path_clear(Square s1, Square s2, Bitboard occupied) {
    const Paths& p = paths(s1, s2);
    if (!p.count)
        return true;
    for (int i = 0; i < p.count; ++i)
        if (!(p.path[i] & occupied))
            return true;
    return false;
}

}  // namespace Stockfish::Wrap

#endif  // #ifndef WRAP_H_INCLUDED
