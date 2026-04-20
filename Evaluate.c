#include "Evaluate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#include "ChessBoard.h"

#define TT_SIZE 1048576
#define TT_MASK (TT_SIZE - 1)

typedef struct {
    unsigned long long hash;
    int score;
    int depth;
    char flag;
    Move bestMove;
} TTEntry;

static TTEntry* transposition_table = NULL;

const int pieceValues[] = { 100, 320, 330, 500, 900, 20000, -100, -320, -330, -500, -900, -20000 };

const int pst[12][64] = {
    { 0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0 }, // P

    {-50,-40,-30,-30,-30,-30,-40,-50,
     -40,-20,  0,  0,  0,  0,-20,-40,
     -30,  0, 10, 15, 15, 10,  0,-30,
     -30,  5, 15, 20, 20, 15,  5,-30,
     -30,  0, 15, 20, 20, 15,  0,-30,
     -30,  5, 10, 15, 15, 10,  5,-30,
     -40,-20,  0,  5,  5,  0,-20,-40,
     -50,-40,-30,-30,-30,-30,-40,-50 }, // N

    {-20,-10,-10,-10,-10,-10,-10,-20,
     -10,  0,  0,  0,  0,  0,  0,-10,
     -10,  0,  5, 10, 10,  5,  0,-10,
     -10,  5,  5, 10, 10,  5,  5,-10,
     -10,  0, 10, 10, 10, 10,  0,-10,
     -10, 10, 10, 10, 10, 10, 10,-10,
     -10,  5,  0,  0,  0,  0,  5,-10,
     -20,-10,-10,-10,-10,-10,-10,-20 }, // B

    {  0,  0,  0,  0,  0,  0,  0,  0,
       5, 10, 10, 10, 10, 10, 10,  5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
       0,  0,  0,  5,  5,  0,  0,  0 }, // R

    {-20,-10,-10, -5, -5,-10,-10,-20,
     -10,  0,  0,  0,  0,  0,  0,-10,
     -10,  0,  5,  5,  5,  5,  0,-10,
      -5,  0,  5,  5,  5,  5,  0, -5,
       0,  0,  5,  5,  5,  5,  0, -5,
     -10,  5,  5,  5,  5,  5,  0,-10,
     -10,  0,  5,  0,  0,  0,  0,-10,
     -20,-10,-10, -5, -5,-10,-10,-20 }, // Q

    {-30,-40,-40,-50,-50,-40,-40,-30,
     -30,-40,-40,-50,-50,-40,-40,-30,
     -30,-40,-40,-50,-50,-40,-40,-30,
     -30,-40,-40,-50,-50,-40,-40,-30,
     -20,-30,-30,-40,-40,-30,-30,-20,
     -10,-20,-20,-20,-20,-20,-20,-10,
      20, 20,  0,  0,  0,  0, 20, 20,
      20, 30, 10,  0,  0, 10, 30, 20 }  // K
};

void tt_init() {
    if (!transposition_table) transposition_table = calloc(TT_SIZE, sizeof(TTEntry));
}

void tt_free() {
    if (transposition_table) free(transposition_table);
    transposition_table = NULL;
}

int scorePosition(ChessBoard* board) {
    int score = 0;
    for (int piece = P; piece <= k; piece++) {
        uint64_t bb = board->pieceBB[piece];
        while (bb) {
            int sq = get_lsb(bb);
            score += pieceValues[piece];
            score += pst[piece][sq ^ 56];
            pop_bit(bb, sq);
        }
    }
    return (board->side == WHITE) ? score : -score;
}

int killer_moves[100][2];

int quiescence(ChessBoard* board, int alpha, int beta) {
    int stand_pat = scorePosition(board);
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    for (int f = 0; f < 64; f++) {
        if (!get_bit(board->occupancy[board->side], f)) continue;
        for (int t = 0; t < 64; t++) {
            if (!get_bit(board->occupancy[!board->side], t)) continue;

            ChessMoveState state;
            if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                int score = -quiescence(board, -beta, -alpha);
                ChessBoard_unmakeMove(board, &state);

                if (score >= beta) return beta;
                if (score > alpha) alpha = score;
            }
        }
    }
    return alpha;
}

int search(ChessBoard* board, int depth, int alpha, int beta, int ply) {
    if (depth == 0) return quiescence(board, alpha, beta);

    int moves_from[256], moves_to[256], moves_score[256];
    int movesCount = 0;
    int kingSq = get_lsb(board->pieceBB[board->side == WHITE ? K : k]);

    for (int f = 0; f < 64; f++) {
        if (!get_bit(board->occupancy[board->side], f)) continue;
        for (int t = 0; t < 64; t++) {
            ChessMoveState state;
            if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                moves_from[movesCount] = f;
                moves_to[movesCount] = t;

                int is_capture = get_bit(board->occupancy[!board->side], t);
                if (is_capture) {
                    moves_score[movesCount] = 10000;
                }
                else if (killer_moves[ply][0] == (f | (t << 6))) {
                    moves_score[movesCount] = 9000;
                }
                else if (killer_moves[ply][1] == (f | (t << 6))) {
                    moves_score[movesCount] = 8000;
                }
                else {
                    moves_score[movesCount] = 0;
                }

                ChessBoard_unmakeMove(board, &state);
                movesCount++;
            }
        }
    }

    if (movesCount == 0) {
        return is_square_attacked(board, kingSq, !board->side) ? -100000 + ply : 0;
    }

    for (int i = 0; i < movesCount - 1; i++) {
        for (int j = i + 1; j < movesCount; j++) {
            if (moves_score[j] > moves_score[i]) {
                int tf = moves_from[i]; moves_from[i] = moves_from[j]; moves_from[j] = tf;
                int tt = moves_to[i]; moves_to[i] = moves_to[j]; moves_to[j] = tt;
                int ts = moves_score[i]; moves_score[i] = moves_score[j]; moves_score[j] = ts;
            }
        }
    }

    for (int i = 0; i < movesCount; i++) {
        ChessMoveState state;
        ChessBoard_makeMove(board, moves_from[i], moves_to[i], ' ', &state);
        int score = -search(board, depth - 1, -beta, -alpha, ply + 1);
        ChessBoard_unmakeMove(board, &state);

        if (score >= beta) {
            if (moves_score[i] < 10000) {
                killer_moves[ply][1] = killer_moves[ply][0];
                killer_moves[ply][0] = moves_from[i] | (moves_to[i] << 6);
            }
            return beta;
        }
        if (score > alpha) alpha = score;
    }
    return alpha;
}

Evaluate Evaluate_findBestMove(ChessBoard* board, int max_depth, int multiPvCount) {
    Evaluate eval;
    eval.multiPvCount = multiPvCount;
    eval.variations = malloc(sizeof(Variation) * multiPvCount);
    for (int i = 0; i < multiPvCount; i++) {
        eval.variations[i].line = NULL;
        eval.variations[i].score = -1000000;
    }

    memset(killer_moves, 0, sizeof(killer_moves));

    int best_from = -1, best_to = -1;
    int current_best_score = -1000000;

    for (int current_depth = 1; current_depth <= max_depth; current_depth++) {
        int moves_from[256], moves_to[256], moves_score[256];
        int movesCount = 0;
        int alpha = -1000000;
        int beta = 1000000;

        for (int f = 0; f < 64; f++) {
            if (!get_bit(board->occupancy[board->side], f)) continue;
            for (int t = 0; t < 64; t++) {
                ChessMoveState state;
                if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                    moves_from[movesCount] = f;
                    moves_to[movesCount] = t;
                    moves_score[movesCount] = (f == best_from && t == best_to) ? 20000 : 0;
                    ChessBoard_unmakeMove(board, &state);
                    movesCount++;
                }
            }
        }

        for (int i = 0; i < movesCount - 1; i++) {
            for (int j = i + 1; j < movesCount; j++) {
                if (moves_score[j] > moves_score[i]) {
                    int tf = moves_from[i]; moves_from[i] = moves_from[j]; moves_from[j] = tf;
                    int tt = moves_to[i]; moves_to[i] = moves_to[j]; moves_to[j] = tt;
                    int ts = moves_score[i]; moves_score[i] = moves_score[j]; moves_score[j] = ts;
                }
            }
        }

        for (int i = 0; i < movesCount; i++) {
            ChessMoveState state;
            ChessBoard_makeMove(board, moves_from[i], moves_to[i], ' ', &state);
            int score = -search(board, current_depth - 1, -beta, -alpha, 1);
            ChessBoard_unmakeMove(board, &state);

            if (score > current_best_score) {
                current_best_score = score;
                best_from = moves_from[i];
                best_to = moves_to[i];
            }
            if (score > alpha) alpha = score;
        }
    }

    Move bestMoveObj;
    bestMoveObj.from = best_from;
    bestMoveObj.to = best_to;
    bestMoveObj.promoteTo = ' ';

    eval.variations[0].line = malloc(sizeof(Move));
    eval.variations[0].line[0] = bestMoveObj;
    eval.variations[0].score = current_best_score;
    eval.variations[0].depth = max_depth;
    eval.variations[0].length = 1;

    return eval;
}