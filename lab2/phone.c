#include <stdio.h>
#include <stdlib.h>



int main() {
    char phone[11];
    int n;
    scanf("%s", phone);
    scanf("%d", &n);

    if (n == -1){
	printf("%s", phone);
	return 0;
    } else if ((n >= 0) & (n <= 9)){
	printf("%c", phone[n]);
	return 0;
	
    } else {
	printf("ERROR");
	return 1;
    }
    
} 
