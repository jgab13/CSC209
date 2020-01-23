#include <stdio.h>
#include <stdlib.h>

#include "life2D_helpers.h"


int main(int argc, char **argv) {

    if (argc != 4) {
        fprintf(stderr, "Usage: life2D rows cols states\n");
        return 1;
    }
    int rows = strtol(argv[1], NULL, 10);
    int cols = strtol(argv[2], NULL, 10);
    int board[rows * cols];

    int states = strtol(argv[3], NULL, 10);
    FILE * data = stdin;
       
    int index = 0; 
    int value;
    while (fscanf(data, "%d", &value) == 1){
	board[index] = value;
	index++;
    }
    
    for (int j = 0; j < states; j++){
        print_state(board, rows, cols);
 	update_state(board, rows, cols);
    }
    
    if (fclose(data) != 0){
	fprintf(stderr, "fclose failed\n");
	return 1;
    }
    

    // TODO: Implement.
}
