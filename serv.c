// *** Author: Zhiyi Zhang for CS118, Time: 04/10/2020, TCP Server Sample Code ***
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
 // *** Initialize socket for listening ***
 int sockfd;
 if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
   perror("socket");
   exit(1);
 }

 // *** Initialize local listening socket address ***
 struct sockaddr_in my_addr;
 memset(&my_addr, 0, sizeof(my_addr));
 my_addr.sin_family = AF_INET;
 my_addr.sin_port = htons(5000);
 my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY allows to connect to any one of the host’s IP address

 // *** Socket Bind ***
 if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
   perror("bind");
   exit(1);
 }

 // *** Socket Listen ***
 if (listen(sockfd, 10) == -1) {
   perror("listen");
   exit(1);
 }

 // *** Accept and Read&Write **
 int client_fd;
 struct sockaddr_in client_addr; // client address
 unsigned int sin_size;
 char recv_buf[1024];
 int recv_buf_size;
 while (1) { // main accept() loop
   sin_size = sizeof(struct sockaddr_in);
   if ((client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
     perror("accept");
     continue;
   }
   printf("server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
   memset(recv_buf, 0, 1024);
   while ((recv_buf_size = read(client_fd, recv_buf, 2024)) > 0)  {
     printf("String received from and resent to the client: %.*s", recv_buf_size, recv_buf);
     if (memcmp(recv_buf, "bye", strlen("bye")) == 0) {
       printf("Will close the connection\n");
       close(client_fd);
       exit(0);
     }
     write(client_fd, recv_buf, recv_buf_size);
   }
   if (recv_buf_size < 0) {
     close(client_fd);
   }
 }
}

