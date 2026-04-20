#ifndef CHESSBOARD_H
#define CHESSBOARD_H

#include <stdint.h>
#include <stdlib.h>
#include <intrin.h>

enum { P, N, B, R, Q, K, p, n, b, r, q, k };
enum { WHITE, BLACK, BOTH };
enum { WK = 1, WQ = 2, BK = 4, BQ = 8 };

typedef struct Move {
    int from;
    int to;
    char promoteTo;
} Move;

typedef struct ChessBoard {
    uint64_t pieceBB[12];
    uint64_t occupancy[3];
    int side;
    int enpassant;
    int castle;
    int halfMove;
    int fullMove;
    unsigned long long hash;

    char** history;
    int historyCount;
    int historyCap;
} ChessBoard;

typedef struct ChessMoveState {
    uint64_t pieceBB[12];
    uint64_t occupancy[3];
    int enpassant;
    int castle;
    int halfMove;
    int side;
} ChessMoveState;

#define get_bit(bb, sq) (((bb) >> (sq)) & 1ULL)
#define set_bit(bb, sq) ((bb) |= (1ULL << (sq)))
#define pop_bit(bb, sq) ((bb) &= ~(1ULL << (sq)))

static inline int get_lsb(uint64_t bb) {
    if (bb == 0) return -1;
    unsigned long idx = 0;
    _BitScanForward64(&idx, bb);
    return (int)idx;
}

void ChessBoard_setupFromFen(ChessBoard* board, const char* fen);
int ChessBoard_makeMove(ChessBoard* board, int from, int to, char promote, ChessMoveState* state);
void ChessBoard_unmakeMove(ChessBoard* board, ChessMoveState* state);
uint64_t ChessBoard_getAttackers(ChessBoard* board, int sq, int side);

int ChessBoard_uciToSq(const char* uci);
void ChessBoard_sqToUci(int sq, char* out);
int ChessBoard_moveFromUCI(ChessBoard* board, const char* uci);
int ChessBoard_moveFromSAN(ChessBoard* board, const char* san);

int ChessBoard_isGameOver(ChessBoard* board);

int is_square_attacked(ChessBoard* board, int sq, int side);

void ChessBoard_printBoard(ChessBoard* board);

#endif