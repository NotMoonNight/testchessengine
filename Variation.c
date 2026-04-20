#include "Variation.h"
#include <stdlib.h>
#include <string.h>

Variation* Variation_init(Variation* var, Move* line, int score, int depth, int length) {
    Variation* newVar = var ? var : malloc(sizeof(Variation));
    if (newVar) {
        if (line && length > 0) {
            newVar->line = malloc(sizeof(Move) * length);
            memcpy(newVar->line, line, sizeof(Move) * length);
        }
        else {
            newVar->line = NULL;
        }
        newVar->score = score;
        newVar->depth = depth;
        newVar->length = length;
    }
    return newVar;
}