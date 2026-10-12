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

// QuantumChess: see wrap.h

#include "wrap.h"

namespace Stockfish::Wrap {

Line     Lines[SQUARE_NB][LINE_NB];
Bitboard LineAttacks[SQUARE_NB][LINE_NB][256];
Paths    PathsBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];

namespace {

// The two opposite steps sliding along a line
constexpr int LineSteps[LINE_NB][2][2] = {
  {{1, 0}, {-1, 0}}, {{0, 1}, {0, -1}}, {{1, 1}, {-1, -1}}, {{-1, 1}, {1, -1}}};

Bitboard line_mask(Square sq, LineKind k) {
    Bitboard mask = square_bb(sq);
    for (const auto& d : LineSteps[k])
        for (Square s = offset(sq, d[0], d[1]); s != SQ_NONE && s != sq; s = offset(s, d[0], d[1]))
            mask |= square_bb(s);
    return mask;
}

Bitboard line_attack(Square sq, LineKind k, Bitboard occupied) {
    Bitboard attacks = 0;
    for (const auto& d : LineSteps[k])
        for (Square s = offset(sq, d[0], d[1]); s != SQ_NONE && s != sq; s = offset(s, d[0], d[1]))
        {
            attacks |= square_bb(s);
            if (occupied & square_bb(s))
                break;
        }
    return attacks;
}

}  // namespace

void init() {

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        const int f = int(file_of(s)), r = int(rank_of(s));

        // Ranks: shift the rank down to the low byte. Files: one bit per rank,
        // gathered by 0x8040201008040201. Wrapped diagonals: one bit per file,
        // gathered into the top byte by FileABB (bit n = the square on file n).
        Lines[s][RANK_LINE] = {line_mask(s, RANK_LINE), 1ULL << 56, 8 * r};
        Lines[s][FILE_LINE] = {line_mask(s, FILE_LINE), 0x8040201008040201ULL, f};
        Lines[s][DIAG_LINE] = {line_mask(s, DIAG_LINE), FileABB, 0};
        Lines[s][ANTI_LINE] = {line_mask(s, ANTI_LINE), FileABB, 0};

        for (int k = 0; k < LINE_NB; ++k)
        {
            const Line& line = Lines[s][k];
            Bitboard    b    = 0;
            do
            {
                LineAttacks[s][k][line.index(b)] = line_attack(s, LineKind(k), b);
                b                                = (b - line.mask) & line.mask;
            } while (b);
        }
    }

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
        for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
        {
            PathsBB[s1][s2]   = {{0, 0}, 0};
            BetweenBB[s1][s2] = square_bb(s2);
        }

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
        for (const auto& steps : LineSteps)
            for (const auto& d : steps)
            {
                Bitboard path = 0;
                for (Square s = offset(s1, d[0], d[1]); s != SQ_NONE && s != s1;
                     s = offset(s, d[0], d[1]))
                {
                    Paths& p = PathsBB[s1][s];
                    assert(p.count < 2);
                    p.path[p.count++] = path;
                    BetweenBB[s1][s] |= path;
                    path |= square_bb(s);
                }
            }
}

}  // namespace Stockfish::Wrap
