#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVERPORT 5000
#define BACKLOG 10

int main() {
	int sockfd, client_fd; /* listen on sock_fd, new connection on new_fd */
	struct sockaddr_in client_addr; /* my address */
	struct sockaddr_in server_addr; /* connector addr */
	int sin_size;

	/* CREATE A SOCKET */
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("ERROR: FAILED TO CREATE SOCKET");
		exit(1);
	}

	/* SET ADDRESS INFO */
	server_addr.sin_family = AF_INET; /* interp'd b host */
	server_addr.sin_port = htons(SERVERPORT);
	struct hostent *host_name = gethostbyname("localhost");
	server_addr.sin_addr = *((struct in_addr*)host_name->h_addr);

	memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

	/* CONNECT TO THE SOCKET */
	if(connect(sockfd, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) == -1) {
		perror("ERROR: FAILED TO CONNECT ON SOCKET");
		exit(1);
	}

	int recvline_size;
	char sendline[1024], recvline[1024];
	while(fgets(sendline, 1024, stdin) != NULL) {
		write(sockfd, sendline, strlen(sendline));
		if(memcmp(sendline, "bye", strlen("bye")) == 0) {
			printf("Will close the connection\n");
			close(sockfd);
			exit(0);
		}
		memset(recvline, 0, 1024);
		if((recvline_size = read(sockfd, recvline, 1024)) == 0) {
			// error: server terminated prematurely
			perror("The server terminated prematurely");
			exit(4);
		}
		printf("String received from the server: %.*s", recvline_size, recvline);
	}

	return 0;
}
