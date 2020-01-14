#include <stdio.h>
#include <stdlib.h>



int main() {
    char phone[11];
    scanf("%s", phone);
    int n;
    int exit = 0;

    while(scanf("%d", &n)){
	if (n == -1){
	    printf("%s\n", phone);
        } else if ((n >= 0) & (n <= 9)){
	    printf("%c\n", phone[n]);
        } else {
	    printf("ERROR\n");
	    exit = 1;
        }
    }
    return exit;

    
    
}  
