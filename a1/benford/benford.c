#include <stdio.h>
#include <stdlib.h>

#include "benford_helpers.h"

/*
 * The only print statement that you may use in your main function is the following:
 * - printf("%ds: %d\n")
 *
 */
int main(int argc, char **argv) {

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "benford position [datafile]\n");
        return 1;
    }
    int position = strtol(argv[1], NULL, 10);
    int value;
    int arr[10] = {0};

    FILE *data;
    data = fopen(argv[2], "r");
    
    if (data == NULL){
	data = stdin;
    }
    
    while (fscanf(data, "%d", &value) == 1){
	add_to_tally(value, position, arr);
	//printf("%d\n", value);
    }
    // TODO: Implement.
    for (int i = 0; i < BASE; i++){
    	printf("%ds: %d\n", i, arr[i]);
    }

    if (fclose(data) != 0){
	fprintf(stderr, "fclose failed\n");
	return 1;
    }
    //printf("%d", position);
    //printf("%d", value);
    return 0;
}
