#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

void send_msg(char * msg, int sock);
void * readsock(void * sock);
void * writesock(void * sock);
void error(char * msg);
void handle_shutdown(int sig);

int conn_open = 1;

int main(int argc, char ** argv)
{		
	if (argc != 3)
	{
		fprintf(stderr, "usage: client hostname port_number\n");
		exit(1);
	}
	
	char * node = argv[1];
	char * port = argv[2];
	
	int status = 0;
	struct addrinfo hints;
	struct addrinfo * servinfo;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	// don't need to set AI_PASSIVE for client
	
	if ((status = getaddrinfo(node, port, &hints, &servinfo)) != 0)
		error("ERROR getaddrinfo");
	
	int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if (sockfd < 0)
		error("ERROR opening socket");
	
	// connect
	if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
		error("ERROR connecting");
	else
	{
		printf("Connected!\n");
		printf("Welcome to the Ultimatum Game!\n\n");
	}
	
	pthread_t readt, writet;
	
	pthread_create(&readt, 0, readsock, &sockfd);
	pthread_create(&writet, 0, writesock, &sockfd);
	
	pthread_join(writet, 0);
	pthread_join(readt, 0);
	freeaddrinfo(servinfo);
	exit(0);
}

void * readsock(void * sock)
{
	int socket = *(int *) sock;
	char buffer[256];
	int status;
	while (conn_open == 1)
	{
		memset(buffer, 0, sizeof(buffer));  
		status = recv(socket, buffer, sizeof(buffer) - 1, 0);
		buffer[255] = '\0';
		if (status > 0)
			printf("%s", buffer);
		else if (status == 0)
		{
			conn_open = 0;
			// press key to get over blocking fget call
			printf("Server closed the connection.\nPress any key to quit...\n");
			break;
		}
		else
			error("ERROR on receive");	
	}
	pthread_exit(0);
}

void * writesock(void * sock)
{
	int socket = *(int *) sock;
	char buffer[256];
	int status;
	while (conn_open == 1)
	{
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 256, stdin);
		status = send(socket, buffer, strlen(buffer), 0);
		if (status < 0)
			error("ERROR on send");
	}
	close(socket);
	pthread_exit(0);
}

// sends msg to socket
void send_msg(char * msg, int sock)
{
	int s = send(sock, msg, strlen(msg), 0);
	if (s < 0)
		error("ERROR on send");
}

// error handling
void error(char * msg)
{
	perror(msg);
	exit(1);
}
