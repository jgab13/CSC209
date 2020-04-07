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
	int nmessage;//added by Jonathan
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


//Helper functions.
int find_network_newline(const char *buf, int n);
int check_username(char *username, struct client **active_clients_ptr);
void remove_followers(struct client **clients, int fd, struct client *p);
void add_message(struct client * p, char * s);
void reset_inputbuf(struct client *p); 
void write_wrapper(int fd, char * message, struct client ** p, struct sockaddr_in q);
int read_wrapper(int fd, char * pointer, int size, struct sockaddr_in q, struct client **p);
int check_follow(struct client *p, struct client *q);



// These are some of the function prototypes that we used in our solution 
// You are not required to write functions that match these prototypes, but
// you may find them helpful when thinking about operations in your program.

// Send the message in s to all clients in active_clients. 
void announce(struct client *active_clients, char *s){
	struct client *cur = active_clients; 
	while (cur != NULL){
		if (write(cur->fd, s, strlen(s)) == -1){
            fprintf(stderr, "Write to client %d failed\n", cur->fd);
			remove_client(&active_clients, cur->fd);
		}
		cur = cur->next;
	}
}


// Move client c from new_clients list to active_clients list. 
void activate_client(struct client *c, 
    struct client **active_clients_ptr, struct client **new_clients_ptr){
		struct client **p;

		for (p = new_clients_ptr; *p && (*p)->fd != c->fd; p = &(*p)->next)
        ;
		struct client *t = (*p)->next;
		*p = t;
		
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
	p->nfollowers = 0;
	p->nfollowing = 0;
	p->nmessage = 0;

    // initialize messages to empty strings
    for (int i = 0; i < MSG_LIMIT; i++) {
        p->message[i][0] = '\0';
    }

    *clients = p;
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
		remove_followers(clients, fd, *p); 

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
                        // Read into input buf for client p.
						int num_reads = read_wrapper(p->fd, p->in_ptr, BUF_SIZE, q, &new_clients);
						if (num_reads != 0){
						// print message about the number of bytes read by read call.
							printf("[%d] Read %d bytes\n", cur_fd, num_reads);
						// check if a newline is present using network newline
							int where;
							if ((where = find_network_newline(p->inbuf, strlen(p->inbuf))) > 0){
								p->inbuf[where - 2] = '\0';
								printf("[%d] Found newline: %s\n", p->fd, p->inbuf);
								// 5) check if password is valid (i.e. != '' and not in active list)
								if (!(check_username(p->inbuf, &active_clients))){
									//copy username from inbuf
									strcpy(p->username, p->inbuf);
									p->username[strlen(p->inbuf)] = '\0';
									// 5 iii) print that this person just joined.
									char str[BUF_SIZE * 2];
									sprintf(str, "%s has just joined\n", p->username);
									//Write to active clients that a new user just joined.
									announce(active_clients, str);
									//print joined twitter to server
									printf("%s has just joined\n", p->username);
									// Remove from new_client, move to active_clients
									activate_client(p, &active_clients, &new_clients);
								} else { //if username is not valid
									char * invalid = INVALID_USER;
									// Tell client that username is not valid and write to server log.
									write_wrapper(p->fd, invalid, &new_clients, q);
									printf("[%d] %s\n", p->fd, invalid);
								}
								reset_inputbuf(p);
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
							int nread = read_wrapper(p->fd, p->in_ptr, BUF_SIZE, q, &active_clients);
							if (nread != 0){
								printf("[%d] Read %d bytes\n", cur_fd, nread);
								int location;
								if ((location = find_network_newline(p->inbuf, strlen(p->inbuf))) > 0){
									p->inbuf[location - 2] = '\0';
									printf("[%d] Found newline: %s\n", p->fd, p->inbuf);
									//If user issues quit command.
									if (!strcmp("quit", p->inbuf)){
										//Save username before removing client.
										char quit_user[BUF_SIZE];
										strncpy(quit_user, p->username, sizeof(quit_user));
										quit_user[strlen(p->username)] = '\0';
										printf("Disconnect from %s\n", inet_ntoa(q.sin_addr));
										remove_client(&active_clients, cur_fd);
										//Announce to fellow users that p client left.
										char quit[BUF_SIZE];
										sprintf(quit, "%s has just left\n", quit_user);
										announce(active_clients, quit);
										//reset buffer
										reset_inputbuf(p);
									//If user issues unfollow command.
									} else if (!(strncmp(UNFOLLOW_MSG, p->inbuf, 6))){
										struct client *cur;
										int exists;
										for (cur = active_clients; cur != NULL; cur = cur->next){
											char full_message[BUF_SIZE];
											strncpy(full_message, UNFOLLOW_MSG, sizeof(full_message)); //add a space here
											full_message[strlen(UNFOLLOW_MSG)] = '\0';
											strncat(full_message, " ", sizeof(full_message) - strlen(full_message) - 1);
											strncat(full_message, cur->username, sizeof(full_message) - strlen(full_message) - 1);
											if ((!(strcmp(p->inbuf, full_message))) & (!(check_follow(p, cur)))){
												exists++;
												//remove cur->username from following of p
												for (int m = 0; m < FOLLOW_LIMIT; m++){
													if (p->following[m] != NULL){
														if (!strcmp(p->following[m]->username, cur->username)){
															p->following[m] = NULL;
															p->nfollowing -= 1;
															//Print message to server that this guy is no longer following another guy.
															//add_message(p, full_message);
															printf("%s: %s\n", p->username, full_message);
															printf("%s is no longer following %s\n", p->username, cur->username);
															break;
														}
													}
												}

												//remove p->username from followers of cur
												for (int k = 0; k < FOLLOW_LIMIT; k++){
													if (cur->followers[k] != NULL){
														if (!strcmp(cur->followers[k]->username, p->username)){
															cur->followers[k] = NULL;
															cur->nfollowers -= 1;
															//print message to server that this guy lost a follower.
															printf("%s no longer has %s as a follower\n",cur->username,p->username);
															break;
														}
													}
												}
												break;
											}
										}
										if (exists == 0){
											char *invalid_unfollow = "User not found. Unfollow request failed.\n";
											write_wrapper(p->fd, invalid_unfollow, &active_clients, q);
										}
										exists = 0;
										reset_inputbuf(p);
									}
									// 5) Check if follow and check username to see if valid
									// 5 i) if FOLLOW_LIMIT violated -> notify user that you cannot add
									// 5 ii) else add client to followers and add current client to following of username
									else if (!(strncmp(FOLLOW_MSG, p->inbuf, 6))){
										struct client *current;
										int exist = 0;
										for (current = active_clients; current != NULL; current = current->next){
											char full_message2[BUF_SIZE];
											strncpy(full_message2, FOLLOW_MSG, sizeof(full_message2));//add a space here
											full_message2[strlen(FOLLOW_MSG)] = '\0';
											strncat(full_message2, " ", sizeof(full_message2) - strlen(full_message2) - 1);
											strncat(full_message2, current->username, sizeof(full_message2) - strlen(full_message2) - 1);
											
											if (!(strcmp(full_message2, p->inbuf)) & (current->nfollowers < FOLLOW_LIMIT) & 
													(p->nfollowing < FOLLOW_LIMIT) & ((check_follow(p, current)))){
												exist++;
												for (int b = 0; b < FOLLOW_LIMIT; b++){
													if (p->following[b] == NULL){
														p->following[b] = current;
														p->nfollowing += 1;
														break;
													}
												}
												
												for (int c = 0; c < FOLLOW_LIMIT; c++){
													if	(current->followers[c] == NULL){
														current->followers[c] = p;
														current->nfollowers +=1;
														break;
													}
												}
												printf("%s: %s\n", p->username, full_message2);
												printf("%s is now followiwng %s\n",p->username,current->username);
												printf("%s has %s as a follower\n",current->username,p->username);
												break;
											}
										}
										if (exist == 0){
											char *invalid_follow = "Follow request failed due to invalid username or limited follow space.\n";
											write_wrapper(p->fd, invalid_follow, &active_clients, q);
										
										}
										exist = 0;
										reset_inputbuf(p);
									} 
									// 7) check if show
									// 7 i) for each user that the current client is following - iterate through each of their messages and write to the current client
									else if (!strcmp(p->inbuf, SHOW_MSG)){
										for (int i = 0; i < FOLLOW_LIMIT; i++){
											for (int j = 0; j < MSG_LIMIT; j++){
												char msg[BUF_SIZE];
												if ((p->following[i] != NULL)){
													if(p->following[i]->message[j][0] != '\0'){
														strncpy(msg, p->following[i]->username, sizeof(msg));
														msg[strlen(p->following[i]->username)] ='\0';
														strncat(msg, ": ", sizeof(msg) - strlen(msg) - 1);
														strncat(msg, p->following[i]->message[j], sizeof(msg) - strlen(msg) - 1);
														strncat(msg, "\n", sizeof(msg) - strlen(msg) - 1);									
														write_wrapper(p->fd, msg, &active_clients, q);
													}
												}
											}
										}
										printf("%s: %s\n", p->username, p->inbuf);
										reset_inputbuf(p);
									}
									// 8) check send
									else if (!(strncmp(SEND_MSG, p->inbuf, 4))){
										char tweet[BUF_SIZE];
										strncpy(tweet, p->inbuf + 4, strlen(p->inbuf) - 4);
										tweet[strlen(p->inbuf) - 4] = '\0';	
										// 8 i) iterate through message list - if no space - can't send message. 
										// 8 ii) otherwise add message to message list
										// 8 iii) write message to followers
										
										if (p->nmessage < MSG_LIMIT){
											for (int t=0; t < MSG_LIMIT; t++){
												if (p->message[t][0] == '\0'){
													for (int a = 0; a < FOLLOW_LIMIT; a++){
														if (p->followers[a] != NULL){
															char tweet2[BUF_SIZE];
															strncpy(tweet2, p->username, sizeof(tweet2));
															tweet2[strlen(p->username)] = '\0';
															strncat(tweet2, ": ", sizeof(tweet2) - strlen(tweet2) - 1);
															strncat(tweet2, tweet, sizeof(tweet2) - strlen(tweet2) - 1);
															strncat(tweet2, "\n", sizeof(tweet2) - strlen(tweet2) - 1);									
															write_wrapper(p->followers[a]->fd, tweet2, &active_clients, q);
														}
													}
													printf("%s: %s\n", p->username, p->inbuf);
													add_message(p, tweet);
													p->nmessage++;
													break;
												} 
											}
										} else{
											char *fail = "Message limit execeed. Message not sent.\n";
											write_wrapper(p->fd, fail, &active_clients, q);
										}
										reset_inputbuf(p);
										
									} else{
										char *invalid = "Invalid command. Please enter a valid command.\n";
										write_wrapper(p->fd, invalid, &active_clients, q);	
										reset_inputbuf(p);
									}
								} else {
									//increment the p->in_ptr for the new num_reads
									p->in_ptr = (p->in_ptr + nread);
								}
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
int find_network_newline(const char *buf, int n) {
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
		return 1; 
	}
	struct client * active = (*active_clients_ptr);
	while (active != NULL){
		if (!strcmp(active->username, username)){
			return 1;
		}
		active = active->next;
	}
	return 0;
}	


/* 
 * Remove followers from clients if the follower_fd equals fd.
 */
void remove_followers(struct client **clients, int fd, struct client *p){
	struct client *q;
	for (q = *clients; q != NULL; q = q->next){
		for (int m = 0; m < FOLLOW_LIMIT; m++){
			if ((q->following[m] != NULL)){
				if (q->following[m]->fd == fd){
					q->following[m] = NULL;
					q->nfollowing -= 1;
					printf("%s is no longer following %s due to disconnection.\n", q->username, p->username);
					printf("%s no longer has %s as a follower.\n", p->username, q->username);
					break;
				}
			}				
		}
	}
}

/* 
 * Add message s to client p message list if there is available space.
 */
void add_message(struct client * p, char * s){
	for (int i = 0; i < MSG_LIMIT; i++){
		if (p->message[i][0] == '\0'){
			strncpy(p->message[i], s, sizeof(p->message[i]));
			p->message[i][strlen(s)] = '\0';
			break;
		}
	}
}

/* 
 * Resets the input buffer and pointer to the input buffer for client p.
 */
void reset_inputbuf(struct client *p){
	p->inbuf[0] = '\0';
	p->in_ptr = p->inbuf;
}

/* 
 * Write to socket wrapper function.
 */
void write_wrapper(int fd, char * message, struct client ** p, struct sockaddr_in q){
	if (write(fd, message, strlen(message)) == -1) {
		fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
		remove_client(p, fd);
	}
}

/* 
 * Write to socket wrapper function.
 */
int read_wrapper(int fd, char * pointer, int size, struct sockaddr_in q, struct client **p){
	int num_reads = read(fd, pointer, size);
	if (num_reads == 0){
		printf("Disconnect from %s\n", inet_ntoa(q.sin_addr));
		remove_client(p, fd);
	}
	return num_reads;

}

/*
* Returns 0 if p follows q and 1 otherwise.
*/
int check_follow(struct client *p, struct client *q){
	for (int i=0; i < FOLLOW_LIMIT; i++){
		if (p->following[i] != NULL){
			if (!strcmp(p->following[i]->username, q->username)){
				return 0;
			}
		}
	}
	return 1;
}






