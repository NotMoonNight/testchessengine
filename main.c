#define _CRT_SECURE_NO_WARNINGS

#include "ChessBoard.h"
#include "Evaluate.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

void playPvP(ChessBoard* board) {
    char uciMove[10];
    while (!ChessBoard_isGameOver(board)) {
        printf("Enter your move (UCI format, e.g., e2e4): ");
        scanf("%s", uciMove);
        if (!ChessBoard_moveFromUCI(board, uciMove)) {
            printf("Invalid move. Try again.\n");
            continue;
        }
        ChessBoard_printBoard(board);
    }
}

void playPvE(ChessBoard* board, int color) {
    char uciMove[10];
	while (!ChessBoard_isGameOver(board)) {
        if (board->side == color) {
            printf("Enter your move (UCI format, e.g., e2e4): ");
            scanf("%s", uciMove);
            if (!ChessBoard_moveFromUCI(board, uciMove)) {
                printf("Invalid move. Try again.\n");
                continue;
            }
            ChessBoard_printBoard(board);
        }
        else {
            printf("---------------------\n");
            Evaluate eval = Evaluate_findBestMove(board, 5, 1);
            Move bestMove = eval.variations[0].line[0];
            int from = bestMove.from;
            int to = bestMove.to;
            int scoreBot = eval.variations[0].score;
            printf("Best move: %c%d%c%d (%.2f)\n", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, scoreBot / 100.0f);
            snprintf(uciMove, sizeof(uciMove), "%c%d%c%d%c", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, bestMove.promoteTo);
            ChessBoard_moveFromUCI(board, uciMove);
            ChessBoard_printBoard(board);
        }
    }
}

void playEvE(ChessBoard* board) {
    char uciMove[10];
    while (!ChessBoard_isGameOver(board)) {
        printf("---------------------\n");
        Evaluate eval = Evaluate_findBestMove(board, 5, 1);
        Move bestMove = eval.variations[0].line[0];
        int from = bestMove.from;
        int to = bestMove.to;
        int scoreBot = eval.variations[0].score;
        printf("Best move: %c%d%c%d (%.2f)\n", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, scoreBot / 100.0f);
        snprintf(uciMove, sizeof(uciMove), "%c%d%c%d%c", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, bestMove.promoteTo);
        ChessBoard_moveFromUCI(board, uciMove);
        ChessBoard_printBoard(board);
    }
}


int main() {
    ChessBoard board;

    char* fenInicial = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    ChessBoard_setupFromFen(&board, fenInicial);

	int score = -scorePosition(&board), from, to, scoreBot;
    Evaluate eval;
    Move bestMove;
    char uciMove[10];

	//playPvP(&board);
	playPvE(&board, WHITE);
	//playEvE(&board);

    printf("\n\nGame Over: %d \n", ChessBoard_isGameOver(&board));

    return 0;
}