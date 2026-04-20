#include "ChessBoard.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static const char* piece_chars = "PNBRQKpnbrqk";

uint64_t mask_pawn_attacks(int sq, int side) {
    uint64_t attacks = 0ULL;
    uint64_t bit = (1ULL << sq);
    if (side == WHITE) {
        if (sq < 56) {
            if (sq % 8 != 0) attacks |= (bit << 7);
            if (sq % 8 != 7) attacks |= (bit << 9);
        }
    }
    else {
        if (sq > 7) {
            if (sq % 8 != 0) attacks |= (bit >> 9);
            if (sq % 8 != 7) attacks |= (bit >> 7);
        }
    }
    return attacks;
}

uint64_t mask_knight_attacks(int sq) {
    uint64_t attacks = 0ULL;
    uint64_t bit = (1ULL << sq);
    if ((sq + 17) < 64 && sq % 8 < 7) attacks |= (bit << 17);
    if ((sq + 15) < 64 && sq % 8 > 0) attacks |= (bit << 15);
    if ((sq + 10) < 64 && sq % 8 < 6) attacks |= (bit << 10);
    if ((sq + 6) < 64 && sq % 8 > 1)  attacks |= (bit << 6);
    if ((sq - 17) >= 0 && sq % 8 > 0) attacks |= (bit >> 17);
    if ((sq - 15) >= 0 && sq % 8 < 7) attacks |= (bit >> 15);
    if ((sq - 10) >= 0 && sq % 8 > 1) attacks |= (bit >> 10);
    if ((sq - 6) >= 0 && sq % 8 < 6)  attacks |= (bit >> 6);
    return attacks;
}

uint64_t mask_king_attacks(int sq) {
    uint64_t attacks = 0ULL;
    uint64_t bit = (1ULL << sq);
    if (sq % 8 != 7) {
        attacks |= (bit << 1);
        if (sq < 56) attacks |= (bit << 9);
        if (sq > 7)  attacks |= (bit >> 7);
    }
    if (sq % 8 != 0) {
        attacks |= (bit >> 1);
        if (sq < 56) attacks |= (bit << 7);
        if (sq > 7)  attacks |= (bit >> 9);
    }
    if (sq < 56) attacks |= (bit << 8);
    if (sq > 7)  attacks |= (bit >> 8);
    return attacks;
}

uint64_t get_slider_attacks(int sq, uint64_t occ, int is_rook) {
    uint64_t attacks = 0ULL;
    int dr[] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    int df[] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    int start = is_rook ? 0 : 4;
    int end = is_rook ? 4 : 8;

    for (int i = start; i < end; i++) {
        int r = sq / 8 + dr[i], f = sq % 8 + df[i];
        while (r >= 0 && r < 8 && f >= 0 && f < 8) {
            attacks |= (1ULL << (r * 8 + f));
            if (occ & (1ULL << (r * 8 + f))) break;
            r += dr[i]; f += df[i];
        }
    }
    return attacks;
}

int ChessBoard_uciToSq(const char* uci) {
    return (uci[0] - 'a') + (uci[1] - '1') * 8;
}

void ChessBoard_sqToUci(int sq, char* out) {
    out[0] = (sq % 8) + 'a';
    out[1] = (sq / 8) + '1';
    out[2] = '\0';
}

int ChessBoard_moveFromUCI(ChessBoard* board, const char* uci) {
    if (strlen(uci) < 4) return 0;

    int from = ChessBoard_uciToSq(uci);
    int to = ChessBoard_uciToSq(uci + 2);
    char promote = (strlen(uci) == 5) ? uci[4] : ' ';

    ChessMoveState state;
    return ChessBoard_makeMove(board, from, to, promote, &state);
}

int ChessBoard_moveFromSAN(ChessBoard* board, const char* san) {
    if (strcmp(san, "O-O") == 0) return ChessBoard_moveFromUCI(board, board->side == WHITE ? "e1g1" : "e8g8");
    if (strcmp(san, "O-O-O") == 0) return ChessBoard_moveFromUCI(board, board->side == WHITE ? "e1c1" : "e8c8");

    int len = (int)strlen(san);
    char clean[10];
    strcpy(clean, san);
    if (clean[len - 1] == '+' || clean[len - 1] == '#') clean[--len] = '\0';

    int toSq = ChessBoard_uciToSq(clean + len - 2);

    int pieceType = (board->side == WHITE) ? P : p;
    if (isupper(clean[0])) {
        if (clean[0] == 'N') pieceType = (board->side == WHITE) ? N : n;
        else if (clean[0] == 'B') pieceType = (board->side == WHITE) ? B : b;
        else if (clean[0] == 'R') pieceType = (board->side == WHITE) ? R : r;
        else if (clean[0] == 'Q') pieceType = (board->side == WHITE) ? Q : q;
        else if (clean[0] == 'K') pieceType = (board->side == WHITE) ? K : k;
    }

    // Desambiguação col/row
    int req_f = -1, req_r = -1;
    if (len > 3 && isupper(clean[0])) {
        if (clean[1] == 'x') { /* normal */ }
        else if (len >= 4 && isalpha(clean[1]) && clean[2] == 'x') req_f = clean[1] - 'a';
        else if (isalpha(clean[1])) req_f = clean[1] - 'a';
        else if (isdigit(clean[1])) req_r = clean[1] - '1';
    }
    else if (islower(clean[0]) && len >= 3 && clean[1] == 'x') {
        req_f = clean[0] - 'a';
    }

    uint64_t candidates = board->pieceBB[pieceType];
    while (candidates) {
        int fromSq = get_lsb(candidates);
        pop_bit(candidates, fromSq);

        if (req_f != -1 && (fromSq % 8) != req_f) continue;
        if (req_r != -1 && (fromSq / 8) != req_r) continue;

        ChessMoveState state;
        if (ChessBoard_makeMove(board, fromSq, toSq, ' ', &state)) return 1;
    }
    return 0;
}

int is_pseudo_legal(ChessBoard* board, int piece, int from, int to) {
    int side = (piece <= K) ? WHITE : BLACK;
    int type = piece % 6;

    if (from == to) return 0;
    if (get_bit(board->occupancy[side], to)) return 0;

    int diff = to - from;
    int from_r = from / 8, from_f = from % 8;
    int to_r = to / 8, to_f = to % 8;

    switch (type) {
    case P: {
        int dir = (side == WHITE) ? 8 : -8;
        int start_r = (side == WHITE) ? 1 : 6;

        if (to == from + dir && !get_bit(board->occupancy[BOTH], to)) return 1;

        if (from_r == start_r && to == from + 2 * dir &&
            !get_bit(board->occupancy[BOTH], from + dir) &&
            !get_bit(board->occupancy[BOTH], to)) return 1;

        if ((to == from + dir - 1 || to == from + dir + 1) && abs(from_f - to_f) == 1) {
            if (get_bit(board->occupancy[!side], to) || to == board->enpassant) return 1;
        }
        return 0;
    }
    case N:
        return (mask_knight_attacks(from) & (1ULL << to)) != 0;
    case K: {
        if ((mask_king_attacks(from) & (1ULL << to)) != 0) return 1;

        if (side == WHITE && from == 4 && from_r == 0 && to_r == 0) {
            if (to == 6 && (board->castle & WK) && !get_bit(board->occupancy[BOTH], 5) && !get_bit(board->occupancy[BOTH], 6) && !is_square_attacked(board, 4, BLACK) && !is_square_attacked(board, 5, BLACK)) return 1;
            if (to == 2 && (board->castle & WQ) && !get_bit(board->occupancy[BOTH], 1) && !get_bit(board->occupancy[BOTH], 2) && !get_bit(board->occupancy[BOTH], 3) && !is_square_attacked(board, 4, BLACK) && !is_square_attacked(board, 3, BLACK)) return 1;
        }
        if (side == BLACK && from == 60 && from_r == 7 && to_r == 7) {
            if (to == 62 && (board->castle & BK) && !get_bit(board->occupancy[BOTH], 61) && !get_bit(board->occupancy[BOTH], 62) && !is_square_attacked(board, 60, WHITE) && !is_square_attacked(board, 61, WHITE)) return 1;
            if (to == 58 && (board->castle & BQ) && !get_bit(board->occupancy[BOTH], 57) && !get_bit(board->occupancy[BOTH], 58) && !get_bit(board->occupancy[BOTH], 59) && !is_square_attacked(board, 60, WHITE) && !is_square_attacked(board, 59, WHITE)) return 1;
        }
        return 0;
    }
    case B:
        return (get_slider_attacks(from, board->occupancy[BOTH], 0) & (1ULL << to)) != 0;
    case R:
        return (get_slider_attacks(from, board->occupancy[BOTH], 1) & (1ULL << to)) != 0;
    case Q:
        return ((get_slider_attacks(from, board->occupancy[BOTH], 0) | get_slider_attacks(from, board->occupancy[BOTH], 1)) & (1ULL << to)) != 0;
    }
    return 0;
}

int is_square_attacked(ChessBoard* board, int sq, int side) {
    if (side == WHITE) {
        if (mask_pawn_attacks(sq, BLACK) & board->pieceBB[P]) return 1;
        if (mask_knight_attacks(sq) & board->pieceBB[N]) return 1;
        if (mask_king_attacks(sq) & board->pieceBB[K]) return 1;
        if (get_slider_attacks(sq, board->occupancy[BOTH], 1) & (board->pieceBB[R] | board->pieceBB[Q])) return 1;
        if (get_slider_attacks(sq, board->occupancy[BOTH], 0) & (board->pieceBB[B] | board->pieceBB[Q])) return 1;
    }
    else {
        if (mask_pawn_attacks(sq, WHITE) & board->pieceBB[p]) return 1;
        if (mask_knight_attacks(sq) & board->pieceBB[n]) return 1;
        if (mask_king_attacks(sq) & board->pieceBB[k]) return 1;
        if (get_slider_attacks(sq, board->occupancy[BOTH], 1) & (board->pieceBB[r] | board->pieceBB[q])) return 1;
        if (get_slider_attacks(sq, board->occupancy[BOTH], 0) & (board->pieceBB[b] | board->pieceBB[q])) return 1;
    }
    return 0;
}

void ChessBoard_updateOccupancy(ChessBoard* board) {
    memset(board->occupancy, 0, sizeof(board->occupancy));
    for (int i = P; i <= K; i++) board->occupancy[WHITE] |= board->pieceBB[i];
    for (int i = p; i <= k; i++) board->occupancy[BLACK] |= board->pieceBB[i];
    board->occupancy[BOTH] = board->occupancy[WHITE] | board->occupancy[BLACK];
}

void ChessBoard_setupFromFen(ChessBoard* board, const char* fen) {
    memset(board->pieceBB, 0, sizeof(board->pieceBB));
    int rank = 7, file = 0;
    const char* ptr = fen;

    while (*ptr != ' ') {
        if (isdigit(*ptr)) file += (*ptr - '0');
        else if (*ptr == '/') { rank--; file = 0; }
        else {
            for (int i = 0; i < 12; i++) {
                if (*ptr == piece_chars[i]) {
                    set_bit(board->pieceBB[i], rank * 8 + file);
                    break;
                }
            }
            file++;
        }
        ptr++;
    }

    ptr++;
    board->side = (*ptr == 'w') ? WHITE : BLACK;
    ptr += 2;

    board->castle = 0;
    while (*ptr != ' ') {
        if (*ptr == 'K') board->castle |= WK;
        else if (*ptr == 'Q') board->castle |= WQ;
        else if (*ptr == 'k') board->castle |= BK;
        else if (*ptr == 'q') board->castle |= BQ;
        ptr++;
    }

    ptr++;
    if (*ptr == '-') board->enpassant = -1;
    else {
        int f = ptr[0] - 'a';
        int r = ptr[1] - '1';
        board->enpassant = r * 8 + f;
    }

    ChessBoard_updateOccupancy(board);
}

void ChessBoard_unmakeMove(ChessBoard* board, ChessMoveState* state) {
    memcpy(board->pieceBB, state->pieceBB, sizeof(board->pieceBB));
    memcpy(board->occupancy, state->occupancy, sizeof(board->occupancy));
    board->enpassant = state->enpassant;
    board->castle = state->castle;
    board->halfMove = state->halfMove;
    board->side = state->side;
}

int ChessBoard_makeMove(ChessBoard* board, int from, int to, char promote, ChessMoveState* state) {
    memcpy(state->pieceBB, board->pieceBB, sizeof(board->pieceBB));
    memcpy(state->occupancy, board->occupancy, sizeof(board->occupancy));
    state->enpassant = board->enpassant;
    state->castle = board->castle;
    state->halfMove = board->halfMove;
    state->side = board->side;

    int piece = -1;
    for (int i = 0; i < 12; i++) {
        if (get_bit(board->pieceBB[i], from)) { piece = i; break; }
    }
    if (piece == -1) return 0;

    if (board->side != (piece <= K ? WHITE : BLACK)) return 0;

    if (!is_pseudo_legal(board, piece, from, to)) return 0;

    memcpy(state->pieceBB, board->pieceBB, sizeof(board->pieceBB));

    for (int i = 0; i < 12; i++) {
        if (get_bit(board->pieceBB[i], to)) { pop_bit(board->pieceBB[i], to); board->halfMove = 0; break; }
    }

    if ((piece == P || piece == p) && to == board->enpassant) {
        int capSq = (piece == P) ? to - 8 : to + 8;
        pop_bit(board->pieceBB[piece == P ? p : P], capSq);
    }

    pop_bit(board->pieceBB[piece], from);
    set_bit(board->pieceBB[piece], to);

    if (promote != ' ' && (piece == P || piece == p)) {
        pop_bit(board->pieceBB[piece], to);
        for (int i = 0; i < 12; i++) {
            if (tolower(piece_chars[i]) == tolower(promote) && ((board->side == WHITE && i <= K) || (board->side == BLACK && i > K))) {
                set_bit(board->pieceBB[i], to); break;
            }
        }
    }

    if (piece == K && abs(from - to) == 2) {
        if (to == 6) { pop_bit(board->pieceBB[R], 7); set_bit(board->pieceBB[R], 5); }
        if (to == 2) { pop_bit(board->pieceBB[R], 0); set_bit(board->pieceBB[R], 3); }
    }
    if (piece == k && abs(from - to) == 2) {
        if (to == 62) { pop_bit(board->pieceBB[r], 63); set_bit(board->pieceBB[r], 61); }
        if (to == 58) { pop_bit(board->pieceBB[r], 56); set_bit(board->pieceBB[r], 59); }
    }

    if (piece == P || piece == p) {
        board->enpassant = (abs(from - to) == 16) ? (from + to) / 2 : -1;
        board->halfMove = 0;
    }
    else {
        board->enpassant = -1;
        board->halfMove++;
    }

    if (piece == K) board->castle &= ~(WK | WQ);
    if (piece == k) board->castle &= ~(BK | BQ);
    if (from == 0 || to == 0) board->castle &= ~WQ;
    if (from == 7 || to == 7) board->castle &= ~WK;
    if (from == 56 || to == 56) board->castle &= ~BQ;
    if (from == 63 || to == 63) board->castle &= ~BK;

    ChessBoard_updateOccupancy(board);

    int kingSq = get_lsb(board->pieceBB[board->side == WHITE ? K : k]);
    if (is_square_attacked(board, kingSq, !board->side)) {
        ChessBoard_unmakeMove(board, state);
        return 0;
    }

    board->side = !board->side;
    return 1;
}

int ChessBoard_isGameOver(ChessBoard* board) {
    int kingSq = get_lsb(board->pieceBB[board->side == WHITE ? K : k]);
    int inCheck = is_square_attacked(board, kingSq, !board->side);

    for (int f = 0; f < 64; f++) {
        if (!get_bit(board->occupancy[board->side], f)) continue;
        for (int t = 0; t < 64; t++) {
            ChessMoveState s;
            if (ChessBoard_makeMove(board, f, t, ' ', &s)) {
                ChessBoard_unmakeMove(board, &s);
                return 0;
            }
        }
    }
    return inCheck ? 1 : 2;
}

void ChessBoard_printBoard(ChessBoard* board) {
    printf("\n  a b c d e f g h\n");

    for (int rank = 7; rank >= 0; rank--) {
        printf("%d ", rank + 1);

        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            char piece = '.';

            for (int i = 0; i < 12; i++) {
                if (get_bit(board->pieceBB[i], sq)) {
                    piece = piece_chars[i];
                    break;
                }
            }

            printf("%c ", piece);
        }

        printf("%d\n", rank + 1);
    }

    printf("  a b c d e f g h\n\n");
}