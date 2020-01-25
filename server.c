#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <errno.h> // for accessing errno
#include <signal.h> // for trapping signals

#define POOLSIZE 5

// records offer amount, socket it is from, socket it is sent to, and if it is acccepted
struct offer 
{
	int amount;
	int sockfrom;
	int socksent;
	int accept;
};

struct offer pool[POOLSIZE];

FILE * fh;
pthread_mutex_t mtx, connmtx;
int game_no = 0, conn_no = 0;

// functions
void csv_file();
void assign_role(int sock, int * slot, pthread_t * offeror);
int assign_offeree(int sock);
int assign_offeror(int sock, int * slot, pthread_t * offeror);
void * worker(void * i);
void get_offer(int sock, int i);
void get_response(int sock, int i);
void send_msg(char * msg, int sock);
void reset_offer(int i);
void close_conn(int sock);
void update_conn(int count);
void handle_shutdown(int sig);
void error(char *msg);
void thread_error(char *msg);

int main(int argc, char ** argv)
{
	if (signal(SIGINT, handle_shutdown) == SIG_ERR)
		error("Cannot catch signal.\n");
	
	if (argc != 2)
	{
		fprintf(stderr, "usage: server port_number\n");
		exit(1);
	}
	
	char * port = argv[1];
	
	// create or reopen csv file for recording games
	csv_file();
	
	// begin setting up server
	int status;
	struct addrinfo hints; // contains internet address
	struct addrinfo *servinfo; // points to structs that are returned
	
	// initialize addrinfo
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // gets IP address of current system

	// check if getaddrinfo is successful
	if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
		error("ERROR getaddrinfo");
	
	// get a socket
	int sockfd;
	sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if (sockfd < 0)
		error("ERROR opening socket");
		
	// allow port reuse
	int optval = 1;
	status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (status < 0)
		error("ERROR setting port reuse");
	
	// setting recv timeout
	struct timeval tv;
	tv.tv_sec = 60;
	tv.tv_usec = 0; // need to initialize usec
	status = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (status < 0)
		error("ERROR setting recv timeout"); 
		
	// bind the socket to a port
	status = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
	if (status < 0)
		error("ERROR on bind");
	else
		printf("Server started...\n");
	
	// listen for connections
	if (listen(sockfd, 5) < 0)
		error("ERROR on listen");
	else
		printf("Listening for connections...\n");	
	
	struct sockaddr_storage their_addr; // for storing the client address
	socklen_t addr_size = sizeof(their_addr); // size of address
	
	pthread_t offeror;
	pthread_mutex_init(&mtx, 0);
	pthread_mutex_init(&connmtx, 0);
	
	memset(pool, 0, sizeof(pool));
	
	// accept connection
	while (1)
	{		
		int newsockfd = accept(sockfd, (struct sockaddr *) &their_addr, &addr_size);
		int offer_slot;
		if (newsockfd < 0)
		{
			if (errno == EAGAIN)
				continue;
			else
				error("ERROR on accept");
		}
		else
		{
			update_conn(1);			
			assign_role(newsockfd, &offer_slot, &offeror);
		}		
	}
	
	fclose(fh);
	close(sockfd);	
	freeaddrinfo(servinfo);
	pthread_mutex_destroy(&mtx);
	pthread_mutex_destroy(&connmtx);
	return 0;
}

void assign_role(int sock, int * slot, pthread_t * offeror)
{
	if(assign_offeree(sock))
		return;
	else if	(assign_offeror(sock, slot, offeror))
		return;
	else
	{
		send_msg("Server busy. Try again later.\n", sock);
		close_conn(sock);
	}
}

int assign_offeree(int sock)
{
	for (int i = 0; i < POOLSIZE; i++)
	{
		if (pool[i].amount > 0 && pool[i].sockfrom > 0 && pool[i].socksent == 0)
		{			
			pool[i].socksent = sock;
			return 1;
		}
	}
	return 0;
}

int assign_offeror(int sock, int * slot, pthread_t * offeror)
{
	for (int i = 0; i < POOLSIZE; i++)
	{
		// check for slot to take in offers
		if (pool[i].sockfrom == 0 && pool[i].amount == 0)
		{
			*slot = i;
			pool[i].sockfrom = sock;
			if(pthread_create(offeror, 0, worker, (void *) slot) != 0)
				error("ERROR creating thread for worker");
			if(pthread_detach(*offeror) != 0)
				error("ERROR deatching thread for worker");
			return 1;
		}
	}
	return 0;
}

void * worker(void * n)
{
	int i = *(int *) n;
	char buffer[256];
			
	get_offer(pool[i].sockfrom, i);
	
	// waiting for offer to be assigned
	while (pool[i].socksent == 0)
	{
		// check for dropped connection
		if(recv(pool[i].sockfrom, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0)
		{		
			close_conn(pool[i].sockfrom);
			reset_offer(i);
			pthread_exit(0);
		}
	}

	get_response(pool[i].socksent, i);

	// check if offeror still online
	if(recv(pool[i].sockfrom, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0)
	{
		close_conn(pool[i].sockfrom);
		send_msg("Your partner lost connection. Please reconnect.\n", pool[i].socksent);
		close_conn(pool[i].socksent);
		reset_offer(i);
		pthread_exit(0);
	}
	
	// send result message
	if (pool[i].accept == 1)
	{
		// send to offeror
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "Your offer of $%i is accepted.\nYou receive $%i.\n", pool[i].amount, 100 - pool[i].amount);
		send_msg(buffer, pool[i].sockfrom);
		close_conn(pool[i].sockfrom);
		
		// send to offeree
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "You receive $%i.\n", pool[i].amount);
		send_msg(buffer, pool[i].socksent);
		close_conn(pool[i].socksent);
	}
	else
	{
		// send to offeror
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "Your offer of $%i is rejected.\nYou receive $0.\n", pool[i].amount);
		send_msg(buffer, pool[i].sockfrom);
		close_conn(pool[i].sockfrom);
		
		// send to offeree
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "You receive $0.\n");
		send_msg(buffer, pool[i].socksent);
		close_conn(pool[i].socksent);
	}
	
	// store to file
	pthread_mutex_lock(&mtx); // same mutex locks both the csv file and game_no
	if (fprintf(fh, "\n%i,%i", pool[i].amount, pool[i].accept) < 0)
	{
		error("ERROR appending to file");
	}
	else
	{
		game_no++;
		printf("Number of games completed: %i\n", game_no);
	}
	pthread_mutex_unlock(&mtx);
	
	// reset offer slot
	reset_offer(i);
			
	pthread_exit(0);
}

// get offer from buffer
void get_offer(int sock, int i)
{
	send_msg("* RULES *\n* You have $100 to share with your partner.\n* Make your partner an offer between 0 and 100.\n* If your partner accepts your offer, each of you gets the agreed amount.\n* If your offer is rejected, both of you get nothing.\n\nKey in your offer: \n", sock);
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	int status = recv(sock, buffer, sizeof(buffer), 0);
	if (status < 0)
	{
		// timeout
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// open up array for new offer
			close_conn(sock);
			pool[i].sockfrom = 0;
			pthread_exit(0);
		}
		else
			thread_error("ERROR on recv offer");
	}
	else if (status == 0)
	{
		close_conn(sock);
		pool[i].sockfrom = 0;
		pthread_exit(0);
	}
	
	// validate offer to be between 0 and 100
	int amount = 0;
	sscanf(buffer, "%i", &amount);
	if (amount < 1 || amount > 99)
	{
		send_msg("Invalid offer. Offer must be an integer between 0 and 100.\n", sock);
		get_offer(sock, i);
	}
	else
	{
		pool[i].amount = atoi(buffer);
		send_msg("Offer sent. Please wait...\n", sock);
	}
}

// get response from buffer
void get_response(int sock, int i)
{
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	
	send_msg("* RULES *\n* Your partner has $100 to share with you.\n* Your partner will make you an offer between 0 and 100.\n* If you accept the offer, each of you gets the agreed amount.\n* If you reject, both of you get nothing.\n", sock);
	sprintf(buffer, "\nYou are offered %i.\nDo you accept? (y/n)\n", pool[i].amount);
	send_msg(buffer, sock);
				
	int status = recv(sock, buffer, sizeof(buffer), 0);

	// timeout or if socket is closed
	if ((status < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) || status == 0)
	{
		close_conn(sock);
		// open up array for new offer
		pool[i].socksent = 0;
		while (1)
		{
			// offer is reassigned - get response from new sock
			if (pool[i].socksent != 0)
			{		
				get_response(pool[i].socksent, i);
				return;
			}
			// offer is withdrawn - remove offer from array
			if(recv(pool[i].sockfrom, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0)
			{
				close_conn(pool[i].sockfrom);
				reset_offer(i);
				pthread_exit(0);
			}
		}
	}
	else if (status < 0)
			thread_error("ERROR on recv response");
	
	char response;
	sscanf(buffer, "%c", &response);
	
	if (response == 'y')
		pool[i].accept = 1;
	else if (response == 'n')
		pool[i].accept = -1;
	else
	{
		send_msg("Invalid choice. Enter 'y' or 'n'.\n\n", sock);
		get_response(sock, i);
	}
}

// sends msg to socket
void send_msg(char * msg, int sock)
{
	if (send(sock, msg, strlen(msg), 0) < 0)
		thread_error("ERROR on send");
}

// close socket and update count
void close_conn(int sock)
{
	close(sock);
	update_conn(-1);
}

// update connection count
void update_conn(int count)
{
	pthread_mutex_lock(&connmtx);
	conn_no += count;
	printf("Current connections: %i\n", conn_no);
	pthread_mutex_unlock(&connmtx);
}

// reset pool struct
void reset_offer(int i)
{
	pool[i].sockfrom = 0;
	pool[i].amount = 0;
	pool[i].accept = 0;
	pool[i].socksent = 0;
}

// csv record file handling
void csv_file()
{
	// get file name for record
	char filename[256];
	memset(filename, 0, sizeof(filename));
	printf("The result of the Ultimatum Game will be recorded in a csv file.\n");
	printf("If the input file exists, new records will be added to it. If not, the file will be created as a new record.\n");
	printf("Enter file name (without .csv):\n");
	scanf("%255s", filename);
	strcat(filename, ".csv");
	
	// try to open file
	fh = fopen(filename, "r");
	
	// if file does not exist
	if (fh == NULL)
	{
		printf("File does not exist. Creating new file...\n");
		// create it for appending
		fh = fopen(filename, "a");
		if (fh == NULL)
		{
			error("ERROR on creating file");
		}		
		else
		{
			// update user and insert column headers
			printf("%s created\n", filename);
			fprintf(fh, "Offer,Accepted");
		}
	}
	else
	{
		// close the reading file handler
		fclose(fh);
		printf("Recording into previous file %s...\n", filename);
		// create file for appending (append will create file if it does not exist)
		fh = fopen(filename, "a");
		if (fh == NULL)
		{
			error("ERROR opening file");
		}
	}
}

// error handling
void error(char *msg)
{
	perror(msg);
	exit(1);
}

// error handling for worker thread only so that main server thread continues running
void thread_error(char *msg)
{
	perror(msg);
	pthread_exit((void *) 1);
}

// handling sigint
void handle_shutdown(int sig)
{
	// exit closes file stream to flush buffer and free resources/FD
	exit(0);
}
