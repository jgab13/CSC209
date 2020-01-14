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
	char phone2[n+1];
	for (int i = 0; i < n; i++){
	    phone2[i] = phone[i];
	}
	printf("%s", phone2);
	return 0;
	
    } else {
	printf("ERROR");
	return 1;
    }
    
} 
