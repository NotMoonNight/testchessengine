#ifndef VARIATION_H
#define VARIATION_H

#include "ChessBoard.h"

typedef struct Variation {
    Move* line;
    int score;
    int depth;
    int length;
} Variation;

Variation* Variation_init(Variation* var, Move* line, int score, int depth, int length);

#endif