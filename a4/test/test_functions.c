#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>


#ifndef PORT
    #define PORT 50373
#endif

#define LISTEN_SIZE 5
#define WELCOME_MSG "Welcome to CSC209 Twitter! Enter your username: "
#define SEND_MSG "send"
#define SHOW_MSG "show"
#define FOLLOW_MSG "follow"
#define UNFOLLOW_MSG "unfollow"
#define BUF_SIZE 256
#define MSG_LIMIT 8
#define FOLLOW_LIMIT 5

struct client {
    int fd;
    //struct in_addr ipaddr;
    char username[BUF_SIZE];
    char message[MSG_LIMIT][BUF_SIZE];
    struct client *following[FOLLOW_LIMIT]; // Clients this user is following
    struct client *followers[FOLLOW_LIMIT]; // Clients who follow this user
    char inbuf[BUF_SIZE]; // Used to hold input from the client
    char *in_ptr; // A pointer into inbuf to help with partial reads
    struct client *next;
};


void announce(struct client *active_clients, char *s){//Tested - appears to work
	struct client *cur = active_clients; 
	while (cur != NULL){
		write(cur->fd, s, strlen(s));
		cur = cur->next;
	}
	//this should write the same message to every client.
}


void remove_followers(struct client **clients, int fd){
	struct client **p;
	
	// for each client
    for (p = clients; *p ; p = &(*p)->next){
		if ((*p)->followers[0] != NULL){
			for (int i = 0; i < FOLLOW_LIMIT; i++){
				if ((*p)->followers[i]->fd == fd){
					(*p)->followers[i] = NULL;
				}
			}
		}
        
	}
}

void activate_client(struct client *c, 
    struct client **active_clients_ptr, struct client **new_clients_ptr){//Tested, looks okay to me.
		struct client **p;

		for (p = new_clients_ptr; *p && (*p)->fd != c->fd; p = &(*p)->next)
        ;
		struct client *t = (*p)->next;
		*p = t;
		
		//not sure how this heap work ->need to double check if this is necessary - it might not be.
		struct client * new = c;
		struct client *cur_active = *active_clients_ptr;
		while (cur_active->next != NULL){
			cur_active = cur_active->next;
		}
		cur_active->next = new;
		//add the new client to the end of the active client struct.
	}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char *buf, int n) { //Appears to wrok - tested
	//int j = 0;
	//while (j < n -1){
	//	if (buf[j] == '\r' && buf[j+1] == '\n'){
	//		return j + 2;
	//	}
	//	j++;
	//}
	int end = n - 1;
	while (end > -1){
		if (buf[end] == '\r' && buf[end + 1] == '\n'){
			return end + 2;
		}
		end -= 1;
	}
    return -1;
}

/*
 * Check username to see if username is an active_user.
 * Check username to see if username is the empty string.
 * Return 1 (False) if the above are correct. Return 0 (true) otherwise.
 */
int check_username(char *username, struct client **active_clients_ptr){//This works - tested
	if (!strcmp(username, "")){
		return 1; //username equals the empty string, return 1 (false)
	}
	struct client * active = (*active_clients_ptr);
	while (active != NULL){
		if (!strcmp(active->username, username)){//username equals an existing name
			return 1;//return 1 (false)
		}
		active = active->next;
	}
	return 0;//returns 0 (true) - your password is legit!
}

int main(){
	
	const char * testnewline = "Fagg\r\not";
	int n = 8;
	int newline = find_network_newline(testnewline, n);
	printf("The newline is at position %d\n", newline);
	
	struct client * James = malloc(sizeof(struct client));
	struct client * Jim = malloc(sizeof(struct client));
	struct client * Fred = malloc(sizeof(struct client));
	struct client * Frank = malloc(sizeof(struct client));
	struct client * Mark = malloc(sizeof(struct client));
	struct client * David = malloc(sizeof(struct client));
	struct client * Oli = malloc(sizeof(struct client));
	
	
	strcpy(James->username, "James123"); 
	strcpy(Jim->username, "Faggybear!");
	strcpy(Fred->username, "twopieces");
	
	James->fd = 1;
	Jim->fd = 2;
	Fred->fd = 3;
	Frank->fd = 5;
	Mark->fd = 6;
	David->fd = 7;
	Oli->fd = 8;
	
	James->next = Jim;
	
	struct client * Jones = malloc(sizeof(struct client));
	Jones->fd = 4;
	
	struct client ** active_client_ptr = &James;
	
	
	struct client ** new_clients_ptr = &Jones;
	
	announce(*active_client_ptr, "fuck you");
	
	int user_true = check_username("Faggybear!", active_client_ptr);
	printf("I expect 1 and got %d \n", user_true);
	int user_false1 = check_username("", new_clients_ptr);
	printf("I expect 1 and got %d \n", user_false1);
	int user_false2 = check_username("Fuck", active_client_ptr);
	printf("I expect 0 and got %d \n", user_false2);
	
	activate_client(Jones, active_client_ptr, new_clients_ptr);

	for (int i=0; i < FOLLOW_LIMIT; i++){
		Jim->followers[i] = Oli;
	}
	
	remove_followers(&Jim, James->fd);
	remove_followers(&Jim, Oli->fd);
	remove_followers(&James, Jim->fd);
	
	//for (int j=0; j < FOLLOW_LIMIT - 2; j++){
	//	Jim->followers[j] = NULL;
	//}
	//Jones->followers[0] = NULL;
	char fuck_me[] = "Fuck my ass bitch";
	char fuck[35];
	strncpy(fuck, &fuck_me[5], 31);
	fuck[31] = '\0';
	printf("The word is %s\n", fuck);
	
	
	return 0;
}