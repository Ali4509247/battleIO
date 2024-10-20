/*
 * socket demonstrations:
 * This is the server side of an "internet domain" socket connection, for
 * communicating over the network.
 *
 * In this case we are willing to wait for chatter from the client
 * _or_ for a new connection.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
#define PORT 30100
#endif

# define STATE_NAME 0
# define STATE_WAIT 1
# define STATE_BATTLE_MYTURN 2
# define STATE_BATTLE_YOURTURN 3
# define STATE_BATTLE_SAY 4
# define STATE_LOBBY_SAY 5

struct client {
	int fd;
	struct client *next;
	char name[256];
	char buff[256];
	int len;
	int state;
	int hitpoints;
	int powerpoints;
	struct client *opponent;
	int seen;
};

static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *CurrentClient, struct client *top, char *s, int size);

int handleclient(struct client *p, struct client *top);
// My Helper Functions //
void findOpponent(struct client *currentClient, struct client *top);
void displaywin(struct client *winner, struct client *loser);
void display(struct client *p);
void busydisplay(struct client *p);
void generateHPandPP(struct client *p);
static void lobbybroadcast(struct client *currentClient, struct client *top, char *s, int size);
//static void getlobby(struct client *top);
void pickturn();
int flipcoin();
int generateattackdamage(void);
// End of My Helper Functions //
int bindandlisten(void);

int main(void) {
	srand(1234);
	int clientfd, maxfd, nready;
	struct client *p;
	struct client *head = NULL;
	socklen_t len;
	struct sockaddr_in q;
	fd_set allset;
	fd_set rset;

	int i;


	int listenfd = bindandlisten();
	// initialize allset and add listenfd to the
	// set of file descriptors passed into select
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);
	// maxfd identifies how far into the set to search
	maxfd = listenfd;

	while (1) {
		// make a copy of the set before we pass it into select
		rset = allset;
		/* timeout in seconds (You may not need to use a timeout for
		 * your assignment)*/

		nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
		if (nready == 0) {
			continue;
		}

		if (nready == -1) {
			perror("select");
			continue;
		}

		if (FD_ISSET(listenfd, &rset)){
			printf("a new client is connecting\n");
			len = sizeof(q);
			if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
				perror("accept");
				exit(1);
			}
			FD_SET(clientfd, &allset);
			if (clientfd > maxfd) {
				maxfd = clientfd;
			}
			printf("connection from %s\n", inet_ntoa(q.sin_addr));
			head = addclient(head, clientfd, q.sin_addr);
			char first[256];
			strcpy(first, "What is your name? ");
			write(head->fd, first, strlen(first));
		}

		for(i = 0; i <= maxfd; i++) {
			if (FD_ISSET(i, &rset)) {
				for (p = head; p != NULL; p = p->next) {
					if (p->fd == i) {
						int result = handleclient(p, head);
						if (result == -1) {
							int tmp_fd = p->fd;
							head = removeclient(head, p->fd);
							FD_CLR(tmp_fd, &allset);
							close(tmp_fd);
						}
						break;
					}
				}
			}
		}
	}
	return 0;
}

int handleclient(struct client *p, struct client *top) {
	int len = read(p->fd, p->buff + p->len, sizeof(p->buff) - (p->len) - 1);
	if (len <=0 ){
		char outbuf[1024];
		// socket is closed
		
		sprintf(outbuf, "** %s leaves **\r\n", p->name);
		broadcast(p, top, outbuf, strlen(outbuf));
		if ((p->state == STATE_BATTLE_MYTURN || p->state == STATE_BATTLE_MYTURN) && (p->opponent->opponent == p)){
		    findOpponent(p->opponent, top);
		}
		return -1;
	} else {
	p->len += len;
	(p->buff)[p->len] = '\0';
	// CHECK PROPER CHARACTER printf("%c\n", p->buff[p->len - 1]);
	char waitmessage[strlen(p->name) + 33];
	char entermessage[strlen(p->name) + 33];
	// DEBUG STATE printf("State = %d, Name = %s\n", p->state, p->name);
	switch(p->state){
		case STATE_NAME:
			if (p->len >= 255) {
			 	sprintf(waitmessage, "\nname is too long try again\n");
				write(p->fd,waitmessage, strlen(waitmessage));
			   	p->len = 0;
			}
			if (p->buff[p->len-1] == '\n'){
				memmove(p->name, p->buff, p->len - 1);
				p->name[p->len-1] = '\0';
				sprintf(waitmessage,"\nWelcome, %s! Awaiting opponent...",p->name);
				sprintf(entermessage, "\n**%s enters the arena**\n", p->name);
				broadcast(p, top, entermessage, strlen(entermessage));
				write(p->fd, waitmessage, strlen(waitmessage)); 
				p->len = 0;
				findOpponent(p, top);
			} 
			
			break;
		case STATE_WAIT:
			// Since we are waiting for an available opponent we don't do anything. 
			//The only way to get into the wait state is through the findOpponent function
			char waitingmessage[256];
			if (p->seen == 0){
			sprintf(waitingmessage, "\nYou are in the waiting lobby you can talk with others in the lobby by first typing s then writing your message.\n You can see this message next time you join the next time you connect by clicking r\n");
			write(p->fd, waitingmessage, strlen(waitingmessage));
			}
			p->seen = 1;
			p->len = 0;
			if (p->buff[0] == 's') {
			    p->state = STATE_LOBBY_SAY;
			}
			if (p->buff[0] == 'r') {
			    p->seen = 0;
			}
			break;


		case STATE_BATTLE_MYTURN:
			char attackmessage[600];
			char attackedmessage[600];
			printf("%c", p->buff[0]);
			if (p->buff[0] == 'a') {
				int damage = generateattackdamage();
				p->opponent->hitpoints -= damage;
				sprintf(attackmessage, "\nYou hit %s for %d damage!\nYour hitpoints %d\nYour powermoves: %d\n", p->opponent->name, damage, p->hitpoints, p->powerpoints);
				write(p->fd, attackmessage, strlen(attackmessage));
				sprintf(attackedmessage, "\n%s hit you for %d damage!\nYour hitpoints %d\nYour powermoves: %d\n", p->name, damage, p->opponent->hitpoints, p->opponent->powerpoints);
				write(p->opponent->fd, attackedmessage, strlen(attackedmessage));
				p->state = STATE_BATTLE_YOURTURN;
				p->opponent->state = STATE_BATTLE_MYTURN;
				char message[812];
				sprintf(message, "Waiting for %s to strike...\n",p->opponent->name);
			        write(p->fd, message, strlen(message));
				if (p->opponent->hitpoints <=0){
					displaywin(p, p->opponent);
					struct client *old = p->opponent;
					findOpponent(p, top);
					findOpponent(old, top);
				} else if (p->hitpoints <=0) {
					displaywin(p, p->opponent);
					struct client *old = p->opponent;
					findOpponent(p, top);
					findOpponent(old, top);
				} else {
					display(p->opponent);
				}

			} else if (p->buff[0] == 'p') {
				if (p->powerpoints == 0){
				    sprintf(attackmessage,"\nNo more power points\n");
				    write(p->fd, attackmessage, strlen(attackmessage));
				    p->len = 0;
				    break;
				}
				int coin = flipcoin();
				if (coin == 0){
				    p->powerpoints -=1;
				    int damage = generateattackdamage();
				    damage *= 3;
				    sprintf(attackmessage, "\nYou hit %s for %d damage!\nYour hitpoints %d\nYour powermoves: %d\n", p->opponent->name, damage, p->hitpoints, p->powerpoints);
				    write(p->fd, attackmessage, strlen(attackmessage));
				    sprintf(attackmessage, "\n%s hit you for %d damage!\nYour hitpoints %d\nYour powermoves: %d\n", p->name, damage, p->opponent->hitpoints, p->opponent->powerpoints);
				    write(p->opponent->fd, attackedmessage, strlen(attackedmessage));
				    p->state = STATE_BATTLE_YOURTURN;
				    p->opponent->state = STATE_BATTLE_MYTURN;
				    if (p->opponent->hitpoints <=0){
					displaywin(p, p->opponent);
					struct client *old = p->opponent;
					findOpponent(p, top);
					findOpponent(old, top);
				    } else if (p->hitpoints <=0) {
					displaywin(p, p->opponent);
					struct client *old = p->opponent;
					findOpponent(p, top);
					findOpponent(old, top);
				    } else {
					display(p->opponent);
				    }
				} else {
				    sprintf(attackmessage, "\nYour powermove missed!\n");
				    write(p->fd, attackmessage, strlen(attackmessage));
				    sprintf(attackedmessage, "\n%s powermove missed you!\n", p->name);
				    write(p->opponent->fd, attackedmessage, strlen(attackmessage));
				    p->state = STATE_BATTLE_YOURTURN;
				    p->opponent->state = STATE_BATTLE_MYTURN;
				    display(p->opponent);
				    char message[812];
				    sprintf(message, "Waiting for %s to strike...\n",p->opponent->name);
			            write(p->fd, message, strlen(message));
				}
				

			} else if (p->buff[0] == 's'){
				p->state = STATE_BATTLE_SAY;
				char displaysay[10];
				sprintf(displaysay, "\nSpeak: ");
				write(p->fd, displaysay, strlen(displaysay));
			}
			p->len = 0;
			break;
		case STATE_BATTLE_YOURTURN:  
			p->len = 0;
			break;

		case STATE_BATTLE_SAY:
			if (p->buff[p->len-1] == '\n'){
				p->name[p->len-1] = '\0';
				if (p->len >= 255) {
			 		sprintf(waitmessage, "\nmessage is too long try again\n");
					write(p->fd,waitmessage, strlen(waitmessage));
			   		p->len = 0;
			   		break;
				}
				char message[812];
			        sprintf(message, "%s takes a break to tell you: \n%s",p->name , p->buff);
			        write(p->opponent->fd, message, strlen(message));
			        busydisplay(p->opponent);
			        sprintf(message, "%s's hitpoints: %d\n", p->opponent->name, p->opponent->hitpoints);
				write(p->opponent->fd, message, strlen(message));
				
			        sprintf(message, "Waiting for %s to strike...\n",p->name);
			        write(p->opponent->fd, message, strlen(message));
			        sprintf(message, "You Speak: %s", p->buff);
			        write(p->fd, message, strlen(message));
			        busydisplay(p);
			        display(p);
				p->state = STATE_BATTLE_MYTURN;
				p->len = 0;
			}
		case STATE_LOBBY_SAY:
		     if (p->buff[p->len-1] == '\n'){
			 p->name[p->len-1] = '\0';
			 char message[812];
			 sprintf(message, "%s Says: %s",p->name, p->buff);
			 lobbybroadcast(p, top, message, strlen(message));
			 p->state = STATE_WAIT;
			 p->len = 0;
	 	     }
		default:
			break;
	}
    }
    return 0;
}

// MY HELPER FUNCTIONS //

void findOpponent(struct client *currentClient, struct client *top) {
	struct client *p;

	// Traverse the linked list
	for (p = top; p; p = p->next) {
		// Check if the current client is not the same as the client being evaluated
		if (p != currentClient && (p->state == STATE_WAIT) && (p->opponent != currentClient) && (currentClient-> opponent != currentClient)) {
			pickturn(currentClient, p);
			currentClient->opponent = p;
			p-> opponent = currentClient;
			char battlemessage[512];
			generateHPandPP(p);						// Sets the hp and pp (for the powermoves)
			generateHPandPP(currentClient);
			sprintf(battlemessage, "\nYou engaged with, %s!\nYour hitpoints: %d\nYour Powermoves: %d\n",p->name, p->hitpoints, p->powerpoints);
			write(currentClient->fd, battlemessage, strlen(battlemessage));
			sprintf(battlemessage, "\nYou engaged with, %s!\nYour hitpoints: %d\nYour Powermoves: %d\n",p->opponent->name,p->opponent->hitpoints, p->opponent->powerpoints);
			write(p->fd, battlemessage, strlen(battlemessage));
			currentClient->len = 0;
			p->len = 0; 
			if (p->state == STATE_BATTLE_MYTURN ){
				display(p);
				char waitmessage[512];
				sprintf(waitmessage,"\nAwaiting opponent...\n");
				write(currentClient->fd, waitmessage, strlen(waitmessage));
			} else {
				display(currentClient);
				char waitmessage[512];
				sprintf(waitmessage,"\nAwaiting opponent...\n");
				write(p->fd, waitmessage, strlen(waitmessage));
			}
			return;
		}
	}
	currentClient->state = STATE_WAIT;
}


void displaywin(struct client *winner, struct client *loser) {
	char displaywinmessage[277];
	char displaylosemessage[301];
	char waitmessage[512];
	sprintf(displaywinmessage, "%s gives up, You win!\n", loser->name);
	sprintf(displaylosemessage, "You are no match for %s. You scurry away...\n", winner->name);
	sprintf(waitmessage,"\nAwaiting opponent...\n");
	write(winner->fd, waitmessage, strlen(waitmessage));
	write(loser->fd, waitmessage, strlen(waitmessage));
	write(winner->fd, displaywinmessage, strlen(displaywinmessage));
	write(loser->fd, displaylosemessage, strlen(displaylosemessage));
}

void busydisplay(struct client *p){
	char message[812];
	sprintf(message, "\nYour hitpoints: %d\nYour powerpoints: %d\n", p->hitpoints, p->powerpoints);
	write(p->fd, message, strlen(message));
	sprintf(message, "\n%s's hitpoints: %d\n", p->opponent->name, p->opponent->hitpoints);
	write(p->fd, message, strlen(message));
}

void display(struct client *p){
	char displaymessage[42];
	if (p->powerpoints > 0){
		sprintf(displaymessage, "\n(a)ttack\n(p)owermove\n(s)peak something\n");
	} else {
		sprintf(displaymessage, "\n(a)ttack\n(s)peak something\n");
	}
	write(p->fd, displaymessage, strlen(displaymessage));
}

void pickturn(struct client *currentClient, struct client *p){
	int arr[] = {STATE_BATTLE_MYTURN, STATE_BATTLE_YOURTURN}; 
	int turn = rand() % 2; 		// This will generate either a 0 or 1
	currentClient -> state = arr[turn];
	p -> state = arr[(turn+1)%2];
}

void generateHPandPP(struct client *p){
	p->hitpoints = (rand() % 11) + 20 ; // This will generate a hitpoint value between 20 and 30
	p->powerpoints = (rand() % 2) + 1; // This will generate a hitpoint value between 1 and 3 inclusive for powerpoints
}

int flipcoin(){
    return (rand() % 2);    // This will generate a number either 0 or 1
}

int generateattackdamage(){
	return (rand() % 5) + 2 ; // This will generate a damage value between 2 and 6
}

static void lobbybroadcast(struct client *currentClient, struct client *top, char *s, int size) {
	struct client *p;
	for (p = top; p; p = p->next) {
		if ((p->state == STATE_WAIT || p->state == STATE_LOBBY_SAY)&& p!= currentClient){
		    write(p->fd, s, size);
		}
	}
}
	
/*static void getlobby(struct client *top) {
	struct client *p;
	for (p = top; p; p = p->next) {
		if (p->state == STATE_WAIT || p->state == STATE_LOBBY_SAY){
		    printf("\n%s is in the lobby\n", p->name);
		}
	}
}
*/	

// END OF MY HELPER FUNCTIONS //

/* bind and listen, abort on error
 * returns FD of listening socket
 */
int bindandlisten(void) {
	struct sockaddr_in r;
	int listenfd;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	int yes = 1;
	if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
		perror("setsockopt");
	}
	memset(&r, '\0', sizeof(r));
	r.sin_family = AF_INET;
	r.sin_addr.s_addr = INADDR_ANY;
	r.sin_port = htons(PORT);

	if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
		perror("bind");
		exit(1);
	}

	if (listen(listenfd, 5)) {
		perror("listen");
		exit(1);
	}
	return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
	struct client *p = malloc(sizeof(struct client));
	if (!p) {
		perror("malloc");
		exit(1);
	}

	printf("Adding client %s\n", inet_ntoa(addr));

	p->fd = fd;
	p->next = top;
	top = p;
	p->len = 0;
	p->state = STATE_NAME;
	p->seen = 0;
	return top;
}

static struct client *removeclient(struct client *top, int fd) {
	struct client **p;

	for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next);
	// Now, p points to (1) top, or (2) a pointer to another client
	// This avoids a special case for removing the head of the list
	if (*p) {
		struct client *t = (*p)->next;
		free(*p);
		*p = t;
	} else {
		fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
	}
	return top;
}


static void broadcast(struct client *currentClient , struct client *top, char *s, int size) {
	struct client *p;
	for (p = top; p; p = p->next) {
	    	if (p != currentClient){
	    		write(p->fd, s, size);
	    	}
	}
	/* should probably check write() return value and perhaps remove client */
}
