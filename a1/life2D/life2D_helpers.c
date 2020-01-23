#include <stdio.h>
#include <stdlib.h>


void print_state(int *board, int num_rows, int num_cols) {
    for (int i = 0; i < num_rows * num_cols; i++) {
        printf("%d", board[i]);
        if (((i + 1) % num_cols) == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

int count_neighbors(int *board, int i, int col){
    int count =0;
    if (board[i+1] == 1){
	count++;
    }
    if (board[i-1] == 1){
	count++;
    }
    if (board[i+col] == 1){
	count++;
    }
    if (board[i+col+1] == 1){
	count++;
    }
    if (board[i+col-1] == 1){
	count++;
    }
    if (board[i-col] == 1){
	count++;
    }
    if (board[i-col-1] == 1){
	count++;
    }
    if (board[i-col+1] == 1){
	count++;
    }
    return count;
}

void update_state(int *board, int num_rows, int num_cols) {
    int copy[num_rows * num_cols];
    for (int i = 0; i < num_rows * num_cols; i++){
	copy[i] = board[i];
    }
    for (int i = 0; i < num_rows * num_cols; i++){
	if ((i % num_cols != 0) && (i % num_cols != num_cols - 1) && (i > num_cols - 1) && (i < (num_cols * (num_rows - 1)))){
	    int count = count_neighbors(copy, i, num_cols);
	    if (count == 2 || count == 3){
		if (copy[i] == 0){
		    board[i] = 1;
		}
	    } else if (count < 2 || count > 3){
		if (copy[i] == 1){
		    board[i] = 0;
		}
	    }	
	}
    }
    // TODO: Implement.
    return;
}








