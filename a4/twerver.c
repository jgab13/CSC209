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
#include "socket.h"

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
#define INVALID_USER "This username is invalid. Enter your username: "

struct client {
    int fd;
    struct in_addr ipaddr;
    char username[BUF_SIZE];
    char message[MSG_LIMIT][BUF_SIZE];
    struct client *following[FOLLOW_LIMIT]; // Clients this user is following
	int nfollowing; // added by Jonathan
    struct client *followers[FOLLOW_LIMIT]; // Clients who follow this user
	int nfollowers; //added by Jonathan
    char inbuf[BUF_SIZE]; // Used to hold input from the client
    char *in_ptr; // A pointer into inbuf to help with partial reads
    struct client *next;
};


// Provided functions. 
void add_client(struct client **clients, int fd, struct in_addr addr);
void remove_client(struct client **clients, int fd);


int find_network_newline(const char *buf, int n);//Added by Jonathan.
int check_username(char *username, struct client **active_clients_ptr);//Added by Jonathan.

// These are some of the function prototypes that we used in our solution 
// You are not required to write functions that match these prototypes, but
// you may find them helpful when thinking about operations in your program.

// Send the message in s to all clients in active_clients. 
void announce(struct client *active_clients, char *s){//Tested- appears to work - need to test write call when the socket is closed
	struct client *cur = active_clients; 
	while (cur != NULL){
		if (write(cur->fd, s, strlen(s)) == -1){
			//fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
            fprintf(stderr, "Write to client %d failed\n", cur->fd);
			remove_client(&active_clients, cur->fd);
		}
		cur = cur->next;
	}
	//this should write the same message to every client.
}



// Move client c from new_clients list to active_clients list. 
void activate_client(struct client *c, 
    struct client **active_clients_ptr, struct client **new_clients_ptr){//Tested the second part of the function. 
		struct client **p;

		for (p = new_clients_ptr; *p && (*p)->fd != c->fd; p = &(*p)->next)
        ;
		struct client *t = (*p)->next;
		*p = t;
		
		//No need for the heap with the pointers here!
		struct client **q;
		for (q = active_clients_ptr; *q; q = &(*q)->next)
			;
		*q = c;
		(*q)->next = NULL;
		
	}


// The set of socket descriptors for select to monitor.
// This is a global variable because we need to remove socket descriptors
// from allset when a write to a socket fails. 
fd_set allset;

/* 
 * Create a new client, initialize it, and add it to the head of the linked
 * list.
 */
void add_client(struct client **clients, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));
    p->fd = fd;
    p->ipaddr = addr;
    p->username[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *clients;
	p->nfollowers = 0;//added by Jonathan
	p->nfollowing = 0;//added by Jonathan

    // initialize messages to empty strings
    for (int i = 0; i < MSG_LIMIT; i++) {
        p->message[i][0] = '\0';
    }

    *clients = p;
}

/* 
 * Remove followers from clients if the follower_fd is fd.
 */
void remove_followers(struct client **clients, int fd){///this doesn
	struct client **p;
	
	// for each client
    for (p = clients; *p && (*p)->fd != fd; p = &(*p)->next){
		if (*p && ((*p)->followers[0] != NULL)){//I need another check in order to make quit work.
			for (int i = 0; i < FOLLOW_LIMIT; i++){
				if (*p && ((*p)->followers[i]->fd == fd)){
					(*p)->followers[i] = NULL;
				}
			}
		}
        
	}
}

/* 
 * Remove client from the linked list and close its socket.
 * Also, remove socket descriptor from allset.
 */
void remove_client(struct client **clients, int fd) {
    struct client **p;

    for (p = clients; *p && (*p)->fd != fd; p = &(*p)->next)
        ;

    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        // TODO: Remove the client from other clients' following/followers
        // lists
		//remove_followers(clients, fd); 
		//the remove followers function is fucked - need to double check how it works!

        // Remove the client
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, 
            "Trying to remove fd %d, but I don't know about it\n", fd);
    }
}

void add_message(struct client * p, char * s){
	for (int i = 0; i < MSG_LIMIT; i++){
		if (p->message[i][0] == '\0'){
			strncpy(p->message[i], s, sizeof(p->message[i]));
			p->message[i][strlen(s)] = '\0';
			break;
		}
	}
}


int main (int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;

    // If the server writes to a socket that has been closed, the SIGPIPE
    // signal is sent and the process is terminated. To prevent the server
    // from terminating, ignore the SIGPIPE signal. 
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // A list of active clients (who have already entered their names). 
    struct client *active_clients = NULL;

    // A list of clients who have not yet entered their names. This list is
    // kept separate from the list of active clients, because until a client
    // has entered their name, they should not issue commands or 
    // or receive announcements. 
    struct client *new_clients = NULL;

    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, LISTEN_SIZE);
	free(server);

    // Initialize allset and add listenfd to the set of file descriptors
    // passed into select 
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;

        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            exit(1);
        } else if (nready == 0) {
            continue;
        }

        // check if a new client is connecting
        if (FD_ISSET(listenfd, &rset)) {
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd, &q);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_client(&new_clients, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if (write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, 
                    "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_client(&new_clients, clientfd);
            }
        }

        // Check which other socket descriptors have something ready to read.
        // The reason we iterate over the rset descriptors at the top level and
        // search through the two lists of clients each time is that it is
        // possible that a client will be removed in the middle of one of the
        // operations. This is also why we call break after handling the input.
        // If a client has been removed, the loop variables may no longer be 
        // valid.
        int cur_fd, handled;
        for (cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if (FD_ISSET(cur_fd, &rset)) {
                handled = 0;
                // Check if any new clients are entering their names
                for (p = new_clients; p != NULL; p = p->next) {
                    if (cur_fd == p->fd) {
                        // TODO: handle input from a new client who has not yet
                        // 1) partial read into input buf in p client structure
						int num_reads = read(p->fd, p->in_ptr, BUF_SIZE);
						// 2) Check if num_reads didn't work correctly - i.e. cur_fd closed
						if (num_reads == 0){
						// 		2 i) if cur_fd closed:
						//		2 ii) print message that this address disconnected
							printf("Disconnect from %s\n", inet_ntoa(q.sin_addr));
						//		2 iii) remove cur_fd from new clients
							remove_client(&new_clients, cur_fd);
						//		2 iv) print message that this client was removed.
						//	printf("Removing client %d %s\n", cur_fd, inet_ntoa(q.sin_addr)); - I believe this is printed in remove_client
						} else{//num_reads successfully reads into input buffer.
						// 3) print message about the number of bytes read by read call.
							printf("[%d] Read %d bytes\n", cur_fd, num_reads);
						// 4) check if a newline is present using network newline
							int where;
							if ((where = find_network_newline(p->inbuf, strlen(p->inbuf))) > 0){
								//null terminate p->inbuf.
								p->inbuf[where - 2] = '\0';
								// 4 i) Print that a newline was found with the message
								printf("[%d] Found newline: %s\n", p->fd, p->inbuf);
								// 5) check if password is valid (i.e. != '' and not in active list - create helper function to check passwords)
								if (!(check_username(p->inbuf, &active_clients))){
									//copy username from inbuf
									strcpy(p->username, p->inbuf);
									//null terminate username.
									p->username[strlen(p->inbuf)] = '\0';
									// 5 iii) print that this person just joined.
									char str[BUF_SIZE * 2];
									sprintf(str, "%s has just joined\n", p->username);
									//Write to active clients that a new user just joined.
									announce(active_clients, str);
									//print joined twitter to server
									printf("%s has just joined\n", p->username);
									// 5 ii) if password is valid - remove from new_client, move to active_clients
									activate_client(p, &active_clients, &new_clients);//PROGRAM DIES HERE!!!!!!!.
								} else { //if username is not valid
									char * invalid = INVALID_USER;
									// 6) if not valid, print that this is not valid (print to server log), please enter a proper password (write to file descriptor).
									if (write(p->fd, invalid, strlen(invalid)) == -1) {
										fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
										remove_client(&new_clients, p->fd);
									}
									//Print invalid user to the log.
									printf("[%d] %s\n", p->fd, invalid);
								}
								//reset inbuf to empty string.
								p->inbuf[0] = '\0';
								//reset pointer.
								p->in_ptr = p->inbuf;
							} else {//case where no newline was found
								//increment the p->in_ptr for the new num_reads
								p->in_ptr = (p->in_ptr + num_reads);
							}
							
						}
                        handled = 1;
                        break;
                    }
                }

                if (!handled) {
                    // Check if this socket descriptor is an active client
                    for (p = active_clients; p != NULL; p = p->next) {
                        if (cur_fd == p->fd) {
                            // TODO: handle input from an active client
							int nread = read(p->fd, p->in_ptr, BUF_SIZE);
							if (nread == 0){
								printf("Disconnect from %s\n", inet_ntoa(q.sin_addr));
								remove_client(&active_clients, cur_fd);
							} else{//num_reads successfully reads into input buffer.
								printf("[%d] Read %d bytes\n", cur_fd, nread);
								int location;
								if ((location = find_network_newline(p->inbuf, strlen(p->inbuf))) > 0){
									p->inbuf[location - 2] = '\0';
									printf("[%d] Found newline: %s\n", p->fd, p->inbuf);
									// 9) check if quit
									// 9 i) close socket connection and quit
									if (!strcmp("quit", p->inbuf)){//Nothing happens yet - need to figure this out!
										remove_client(&active_clients, cur_fd);
										printf("Disconnect from %s\n", inet_ntoa(q.sin_addr));
									}
									//Construct an unfollow + username message by iterating through each
									//active user and concatenating with unfollow - then compare against the p->inbuf
									//if not equal, then repeat in a loop, if equal, then remove that active user from 
									// 6) check if unfollow and check username to see if valid
									// 6 i) remove from follower and following list
									//char unfollow[] = "unfollow ";
									else if (!(strncmp(UNFOLLOW_MSG, p->inbuf, 6))){
										struct client *cur;
										for (cur = active_clients; cur != NULL; cur = cur->next){
											char full_message[BUF_SIZE];
											strncpy(full_message, UNFOLLOW_MSG, sizeof(full_message)); //add a space here
											full_message[strlen(UNFOLLOW_MSG)] = '\0';
											strncat(full_message, " ", sizeof(full_message) - strlen(full_message) - 1);
											strncat(full_message, cur->username, sizeof(full_message) - strlen(full_message) - 1);
											if (!(strcmp(p->inbuf, full_message))){
												//remove cur->username from following of p
												for (int m = 0; m < p->nfollowing; m++){
													if (!strcmp(p->following[m]->username, cur->username)){
														p->following[m] = NULL;
														p->nfollowing -= 1;
														//Print message to server that this guy is no longer following another guy.
														add_message(p, full_message);
														printf("%s is no longer following %s\n", p->username, cur->username);
														break;
													}
												}

												//remove p->username from folowers of cur
												for (int k = 0; k < cur->nfollowers; k++){
													if (!strcmp(cur->followers[k]->username, p->username)){
														cur->followers[k] = NULL;
														cur->nfollowers -= 1;
														//print message to server that this guy lost a follower.
														printf("%s no longer has %s as a follower\n",cur->username,p->username);
														break;
													}
												}
												break;
											}
										}
									}
									
									// 5) Check if follow and check username to see if valid
									// 5 i) if FOLLOW_LIMIT violated -> notify user that you cannot add
									// 5 ii) else add client to followers and add current client to following of username
									else if (!(strncmp(FOLLOW_MSG, p->inbuf, 6)) & (p->nfollowing < FOLLOW_LIMIT)){
										struct client *current;
										for (current = active_clients; current != NULL; current = current->next){
											char full_message2[BUF_SIZE];
											strncpy(full_message2, FOLLOW_MSG, sizeof(full_message2));//add a space here
											full_message2[strlen(FOLLOW_MSG)] = '\0';
											strncat(full_message2, " ", sizeof(full_message2) - strlen(full_message2) - 1);
											strncat(full_message2, current->username, sizeof(full_message2) - strlen(full_message2) - 1);
											
											if (!(strcmp(full_message2, p->inbuf)) & (current->nfollowers < FOLLOW_LIMIT)){
												p->following[p->nfollowing] = current;//fixed this bug
												p->nfollowing += 1;
												add_message(p, full_message2);
												printf("%s is now followiwng %s\n",p->username,current->username);
												
												current->followers[current->nfollowers] = p;//fixed this bug
												current->nfollowers +=1;
												printf("%s has %s as a follower\n",current->username,p->username);
												break;
											}
										}	
									}
									//reset inbuf to empty string.
									p->inbuf[0] = '\0';
									//reset pointer.
									p->in_ptr = p->inbuf;
								} else {
									//increment the p->in_ptr for the new num_reads
									p->in_ptr = (p->in_ptr + nread);
								}

								
							
							
							// 7) check if show
							// 7 i) for each user that the current client is following - iterate through each of their messages and write to the current client
							// 8) check send
							// 8 i) iterate through message list - if no space - can't send message. 
							// 8 ii) otherwise add message to message list
							// 8 iii) write message to followers
														// 10) if not valid, print that this is not valid (print to server log), please enter a proper password (write to file descriptor).
							// 11) if write command fails - go through each active client and remove from followers and following, then remove from active client.
                            //break;
							}
							break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char *buf, int n) { //Test in test_functions - seems to work.
	//int j = 0;
	//while (j < n -1){
	//	if (buf[j] == '\r' && buf[j+1] == '\n'){
	//		return j + 2;
	//	}
	//	j++;
	//}
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
int check_username(char *username, struct client **active_clients_ptr){
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
}	//if this returns 0 - the password is legit - if this returns 1, the password is bullshit!!!!!!!!!!!!!!!



