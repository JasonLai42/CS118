#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVERPORT 5000
#define BACKLOG 10

int main() {
	int sockfd; // listen on sockfd
	int client_fd; // accept connection on client_fd
	struct sockaddr_in server_addr; // my addr
	struct sockaddr_in client_addr; // connector addr
	unsigned int sin_size;

	/* CREATE A SOCKET */
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("ERROR: FAILED TO CREATE SOCKET");
		exit(1);
	}

	/* SET ADDRESS INFO */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVERPORT); // short, network byte order
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	/* INADDR_ANY allows clients to connect to any one of the host's IP address. Optionally, 	 * use this line if you know the IP to use: 
	 * 				my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	 */

	/* BIND THE SOCKET */
	if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("ERROR: FAILED TO BIND SOCKET");
		exit(1);
	}

	/* LISTEN ON THE SOCKET */
	if(listen(sockfd, BACKLOG) == -1) {
		perror("ERROR: FAILED TO LISTEN ON SOCKET");
		exit(1);
	}

	/* MAIN ACCEPT LOOP */
	char recv_buf[1024];
	int recv_buf_size;
	while(1) {
		sin_size = sizeof(struct sockaddr_in);
		
		if((client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size)) == -1) {
			perror("ERROR: FAILED TO ACCEPT ON SOCKET");
			continue;
		}
		printf("server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
		
		/* READ AND WRITE */
		memset(recv_buf, 0, 1024);
		while((recv_buf_size = read(client_fd, recv_buf, 2024)) > 0) {
			printf("String received from and resent to the client: %.*s", recv_buf_size, recv_buf);
			if(memcmp(recv_buf, "bye", strlen("bye")) == 0) {
				printf("Will close the connection\n");
				close(client_fd);
				exit(0);
			}
			write(client_fd, recv_buf, recv_buf_size);
		}
		if(recv_buf_size < 0) {
			close(client_fd);
		}
	}
}
