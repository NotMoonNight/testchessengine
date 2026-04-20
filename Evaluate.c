#include "Evaluate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <intrin.h>

#include "ChessBoard.h"

#define TT_SIZE 1048576
#define TT_MASK (TT_SIZE - 1)
#define TT_EXACT 0
#define TT_LOWER 1
#define TT_UPPER 2
#define MAX_QUIESCENCE_DEPTH 10
#define NULL_MOVE_DEPTH 3

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

// History heuristic: tracks how often moves cause cutoffs
int history_table[2][64][64];

int popcount64(unsigned long long x) {
    int count = 0;
    while (x) {
        x &= x - 1;
        count++;
    }
    return count;
}

void tt_init() {
    if (!transposition_table) transposition_table = calloc(TT_SIZE, sizeof(TTEntry));
    memset(history_table, 0, sizeof(history_table));
}

void tt_free() {
    if (transposition_table) free(transposition_table);
    transposition_table = NULL;
}

void tt_clear() {
    if (transposition_table) memset(transposition_table, 0, TT_SIZE * sizeof(TTEntry));
    memset(history_table, 0, sizeof(history_table));
}

// Store position in transposition table
void tt_store(unsigned long long hash, int score, int depth, char flag, Move bestMove) {
    int index = hash & TT_MASK;
    transposition_table[index].hash = hash;
    transposition_table[index].score = score;
    transposition_table[index].depth = depth;
    transposition_table[index].flag = flag;
    transposition_table[index].bestMove = bestMove;
}

// Probe transposition table
int tt_probe(unsigned long long hash, int depth, int alpha, int beta, int* score, Move* bestMove) {
    int index = hash & TT_MASK;
    TTEntry* entry = &transposition_table[index];

    if (entry->hash != hash) return 0;
    if (entry->depth < depth) return 0;

    *bestMove = entry->bestMove;

    if (entry->flag == TT_EXACT) {
        *score = entry->score;
        return 1;
    }
    if (entry->flag == TT_LOWER && entry->score > alpha) {
        alpha = entry->score;
    }
    if (entry->flag == TT_UPPER && entry->score < beta) {
        beta = entry->score;
    }

    if (alpha >= beta) {
        *score = entry->score;
        return 1;
    }
    return 0;
}

// MVV-LVA (Most Valuable Victim - Least Valuable Attacker) scoring for captures
int mvv_lva_score(ChessBoard* board, int from, int to) {
    int victim = -1, attacker = -1;

    // Find victim piece
    for (int i = 0; i < 12; i++) {
        if (get_bit(board->pieceBB[i], to)) {
            victim = i % 6;
            break;
        }
    }

    // Find attacker piece
    for (int i = 0; i < 12; i++) {
        if (get_bit(board->pieceBB[i], from)) {
            attacker = i % 6;
            break;
        }
    }

    if (victim == -1) return 0;

    return (victim * 6 - attacker) * 10000;
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

int quiescence(ChessBoard* board, int alpha, int beta, int qdepth) {
    if (qdepth <= 0) return scorePosition(board);

    int stand_pat = scorePosition(board);
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    for (int f = 0; f < 64; f++) {
        if (!get_bit(board->occupancy[board->side], f)) continue;
        for (int t = 0; t < 64; t++) {
            if (!get_bit(board->occupancy[!board->side], t)) continue;

            ChessMoveState state;
            if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                int score = -quiescence(board, -beta, -alpha, qdepth - 1);
                ChessBoard_unmakeMove(board, &state);

                if (score >= beta) return beta;
                if (score > alpha) alpha = score;
            }
        }
    }
    return alpha;
}

// Optimized move ordering with captured move prioritization
typedef struct {
    int from;
    int to;
    int score;
} ScoredMove;

int compare_moves(const void* a, const void* b) {
    return ((ScoredMove*)b)->score - ((ScoredMove*)a)->score;
}

int search(ChessBoard* board, int depth, int alpha, int beta, int ply) {
    if (depth == 0) return quiescence(board, alpha, beta, MAX_QUIESCENCE_DEPTH);

    Move dummy_move = { 0, 0, ' ' };
    int tt_score;
    if (tt_probe(board->hash, depth, alpha, beta, &tt_score, &dummy_move)) {
        return tt_score;
    }

    int original_alpha = alpha;
    ScoredMove moves[256];
    int movesCount = 0;
    int kingSq = get_lsb(board->pieceBB[board->side == WHITE ? K : k]);

    // Generate all legal moves with scores
    for (int f = 0; f < 64; f++) {
        if (!get_bit(board->occupancy[board->side], f)) continue;
        for (int t = 0; t < 64; t++) {
            ChessMoveState state;
            if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                moves[movesCount].from = f;
                moves[movesCount].to = t;

                int is_capture = get_bit(board->occupancy[!board->side], t);
                if (is_capture) {
                    moves[movesCount].score = 10000 + mvv_lva_score(board, f, t);
                }
                else if (killer_moves[ply][0] == (f | (t << 6))) {
                    moves[movesCount].score = 9000;
                }
                else if (killer_moves[ply][1] == (f | (t << 6))) {
                    moves[movesCount].score = 8000;
                }
                else {
                    moves[movesCount].score = history_table[board->side][f][t];
                }

                ChessBoard_unmakeMove(board, &state);
                movesCount++;
            }
        }
    }

    if (movesCount == 0) {
        return is_square_attacked(board, kingSq, !board->side) ? -100000 + ply : 0;
    }

    // Sort moves using quicksort (O(n log n) instead of bubble sort O(n²))
    qsort(moves, movesCount, sizeof(ScoredMove), compare_moves);

    // Null move pruning for non-endgame positions
    if (depth >= NULL_MOVE_DEPTH + 2) {
        int non_pawn_material = 0;
        for (int i = N; i <= Q; i++) {
            non_pawn_material += popcount64(board->pieceBB[i] | board->pieceBB[i + 6]);
        }

        if (non_pawn_material > 0) {
            int side_backup = board->side;
            board->side = !board->side;
            ChessBoard_updateOccupancy(board);

            int null_score = -search(board, depth - NULL_MOVE_DEPTH - 1, -beta, -beta + 1, ply + 1);

            board->side = side_backup;
            ChessBoard_updateOccupancy(board);

            if (null_score >= beta) return beta;
        }
    }

    int best_score = -100000;
    Move best_move = { -1, -1, ' ' };

    for (int i = 0; i < movesCount; i++) {
        ChessMoveState state;
        ChessBoard_makeMove(board, moves[i].from, moves[i].to, ' ', &state);
        int score = -search(board, depth - 1, -beta, -alpha, ply + 1);
        ChessBoard_unmakeMove(board, &state);

        if (score > best_score) {
            best_score = score;
            best_move.from = moves[i].from;
            best_move.to = moves[i].to;
        }

        if (score >= beta) {
            // Update killer moves
            if (!get_bit(board->occupancy[!board->side], moves[i].to)) {
                killer_moves[ply][1] = killer_moves[ply][0];
                killer_moves[ply][0] = moves[i].from | (moves[i].to << 6);
            }

            // Update history heuristic
            history_table[board->side][moves[i].from][moves[i].to] += depth * depth;

            // Store in transposition table
            tt_store(board->hash, beta, depth, TT_LOWER, best_move);
            return beta;
        }
        if (score > alpha) alpha = score;
    }

    char flag = (best_score <= original_alpha) ? TT_UPPER :
        (best_score >= beta) ? TT_LOWER : TT_EXACT;
    tt_store(board->hash, best_score, depth, flag, best_move);

    return best_score;
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
        ScoredMove moves[256];
        int movesCount = 0;
        int alpha = -1000000;
        int beta = 1000000;

        // Generate root moves
        for (int f = 0; f < 64; f++) {
            if (!get_bit(board->occupancy[board->side], f)) continue;
            for (int t = 0; t < 64; t++) {
                ChessMoveState state;
                if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                    moves[movesCount].from = f;
                    moves[movesCount].to = t;
                    moves[movesCount].score = (f == best_from && t == best_to) ? 20000 : 0;
                    ChessBoard_unmakeMove(board, &state);
                    movesCount++;
                }
            }
        }

        // Sort root moves
        qsort(moves, movesCount, sizeof(ScoredMove), compare_moves);

        for (int i = 0; i < movesCount; i++) {
            ChessMoveState state;
            ChessBoard_makeMove(board, moves[i].from, moves[i].to, ' ', &state);
            int score = -search(board, current_depth - 1, -beta, -alpha, 1);
            ChessBoard_unmakeMove(board, &state);

            if (score > current_best_score) {
                current_best_score = score;
                best_from = moves[i].from;
                best_to = moves[i].to;
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