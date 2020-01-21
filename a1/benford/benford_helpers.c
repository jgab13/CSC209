#include <stdio.h>

#include "benford_helpers.h"

int count_digits(int num) {
    int count = 1;
    while (num >= BASE){
	num = num / BASE;
	count ++;
    }
    return count;
}

int get_ith_from_right(int num, int i) {
    int right = num % BASE;
    while (i >= 0){
	num = num / BASE;
	right = num % BASE;
	i--;
    }
    return right;
}

int get_ith_from_left(int num, int i) {
    int count = count_digits(num) - 1;
    int left = get_ith_from_right(num, count - i - 1); //need -1
    return left;
}

void add_to_tally(int num, int i, int *tally) {
    // TODO: Implement.
    tally[get_ith_from_left(num, i)] += 1;
    return;
}
