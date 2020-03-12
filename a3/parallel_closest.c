#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "point.h"
#include "serial_closest.h"
#include "utilities_closest.h"

void *malloc_wrapper(int size) {
	void *ptr;

	if ((ptr = malloc(size)) == NULL) {
		perror("malloc");
		exit(1);
	}
	return ptr;
}

int fork_wrapper() {
	int res;
	if ((res = fork()) == -1) {
		perror("fork");
		exit(1);
	}
	return res;
}

int wait_wrapper(int *status) {
	int res;

	if((res = wait(status)) == -1) {
		perror("wait");
		exit(1);
	}
	return res;
}

void pipe_wrapper(int *fd) {
	if (pipe(fd) == -1) {
		perror("pipe");
		exit(1);
	}
}


/*
 * Multi-process (parallel) implementation of the recursive divide-and-conquer
 * algorithm to find the minimal distance between any two pair of points in p[].
 * Assumes that the array p[] is sorted according to x coordinate.
 */
double closest_parallel(struct Point *p, int n, int pdmax, int *pcount) {
	if ((n < 4) || (pdmax == 0)){//base case - if < 4 or pdmax = 0, use single process and return.
		return closest_serial(p, n);
	} else {	
		//create pipe for the children.
		int pipe[2][2];
		//two statuses for children.
		int status[2];
		
		//result[0] and result[1] are for child processes - parent stores reads the results into result[3] and result[4].
		double result[4];

		//iterate for 2 children.
		for (int l = 0; l < 2; l++){
			//create pipe
			pipe_wrapper(pipe[l]);
			//call fork for each child.
			int r = fork_wrapper();
			//Check if the process is the child.
			if (r == 0){
				//close the reading end of the pipe.
				if (close(pipe[l][0]) == -1) {
					perror("close reading end from inside child"); 
					exit(1);
				}
				//close the reading end of other child - only an issue for child 1. Child 0 has nothing else to close. 
				int child_no;
				for (child_no = 1; child_no < l; child_no++) {
					if (close(pipe[child_no][0]) == -1) {
						perror("close reading ends of previously forked children");
						exit(1); 
					}
				}
				//compute the smallest of the child - pass in n/2 array to child 0 and n - n/2 array to child 1.
				if (l == 0){
					result[l] = closest_parallel(p, n/2, pdmax -1, pcount); 
				} else{
					result[l] = closest_parallel(p + n/2, (n - n/2), pdmax -1, pcount);
				}
				//write this value to the pipe.
				if (write(pipe[l][1], &result[l], sizeof(double)) != sizeof(double)) {
					perror("write from child to pipe");                
					exit(1);
				}
				//close the write end of the pipe.
				if (close(pipe[l][1]) == -1) {
					perror("close writing end from inside child"); 
					exit(1);
				}
				//exit with pcount + 1 since the process is a child.			
				exit(*pcount +1);
				
			} else{//parent responding to children.
				//wait for child to finish
				wait_wrapper(&status[l]);
				//update pcount for exit status.
				//printf("The wait value is %d \n", status[l]);
				if (WIFEXITED(status[l])) {
					*pcount = WEXITSTATUS(status[l]); 
					//printf("The pdvalue is %d \n", *pcount);
				}
				//read value into result[3] for child 1.
				if (read(pipe[l][0], &result[l+2], sizeof(double)) != sizeof(double)) {
					perror("reading from pipe from a child");
					exit(1);        
				}
				//close pipe after use
				if (close(pipe[l][0]) == -1) {
					perror("close reading end of pipe in parent");
					exit(1);
				}
			}
		}
				
		//use serial closest to find the smallest distance between left and right values.
		struct Point mid_point = p[n/2];

		double min_child = min(result[2],result[3]);
		
		//calculates minimum distance between left and right array.
		
		struct Point *strip = malloc(sizeof(struct Point) * n);
		if (strip == NULL) {
			perror("malloc");
			exit(1);
		}

		int t = 0;
		for (int q = 0; q < n; q++) {
			if (abs(p[q].x - mid_point.x) < min_child) {
				strip[t] = p[q], t++;
			}
		}

		// Find the closest points in strip.  Return the minimum of d and closest distance in strip[].
		double new_min = min(min_child, strip_closest(strip, t, min_child));
		free(strip);
		
		return new_min;
	}
}

