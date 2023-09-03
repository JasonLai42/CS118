#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

/* SERVER VALUES */
#define SERVERPORT 5000
#define BACKLOG 10

/* REQUEST FIELDS */
#define REQUEST_SIZE 4096
// TA said filenames no longer than 20 characters (20 bytes)
#define FILENAME_SIZE 32
// longest extension is no longer than 4 characters (4 bytes)
#define EXTENSION_SIZE 8

/* RESPONSE FIELDS */
#define RESPONSE_SIZE 4096
#define DATE_SIZE 64
// TA said max file size will be 1MB
#define MAX_FILE_SIZE 1000000

int main() {
	int sockfd; // listen on sockfd
	int client_fd; // accept connection on client_fd
	struct sockaddr_in server_addr; // my addr
	struct sockaddr_in client_addr; // connector addr
	unsigned int sin_size;

	/* CREATE A SOCKET */
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	/* SET ADDRESS INFO */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVERPORT); // short, network byte order
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	/* INADDR_ANY allows clients to connect to any one of the host's IP address. Optionally, 
 	 * use this line if you know the IP to use: 
	 *					 my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	 */

	/* BIND THE SOCKET */
	if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	/* LISTEN ON THE SOCKET */
	if(listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	/* MAIN ACCEPT LOOP */
	// request variables
	char request_buf[REQUEST_SIZE];
	int request_buf_size;
	// processing variables
	char file_name[FILENAME_SIZE];
	char extension[EXTENSION_SIZE];
	// response variables
	char response_buf[RESPONSE_SIZE];
	while(1) {
		sin_size = sizeof(struct sockaddr_in);
		
		if((client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}
		printf("SERVER: Got connection from %s\n", inet_ntoa(client_addr.sin_addr));
		
		/* READ AND WRITE */
		memset(request_buf, 0, REQUEST_SIZE);
		memset(file_name, 0, FILENAME_SIZE);
		memset(extension, 0, EXTENSION_SIZE);
		memset(response_buf, 0, RESPONSE_SIZE);
		int has_extension = 0;

		request_buf_size = read(client_fd, request_buf, REQUEST_SIZE);
		if(request_buf_size < 0) {
			perror("read");
			exit(1);
		}
		printf("SERVER: Message received from client: \n%.*s", request_buf_size, request_buf);

		/* GET FILE */
		// TA said all requests are formatted correctly
		// get first line of request message
		char* GET_line = strtok(request_buf, "\n");
		int GET_size = strlen(GET_line);

		// get indices of spaces
		int fn_start = 0;
		int fn_end = 0;
		for(int i = 0; i < GET_size; i++) {
			if(GET_line[i] == ' ') {
				fn_start = i;
				break;
			}
		}
		for(int i = GET_size-1; i > 0; i--) {
			if(GET_line[i] == ' ') {
				fn_end = i;
				break;
			}
		}

		// get filename
		int fn_count = 0, ext_count = 0;
		for(int i = fn_start+1; i < fn_end; i++) {
			if(GET_line[i] == '/') {
				continue;
			}
			file_name[fn_count] = GET_line[i];
			fn_count++;

			if(has_extension) {
				extension[ext_count] = GET_line[i];
				ext_count++;
			}
			if(GET_line[i] == '.') {
				has_extension = 1;
			}
		}
		file_name[fn_count] = '\0';
		extension[ext_count] = '\0';
		printf("SERVER: Client requested file %s\n", file_name);

		// handler for favicon.ico request
		if(strcmp(file_name, "favicon.ico") == 0) {
			char* empty_str = "";
			write(client_fd, empty_str, 0);
			close(client_fd);
			continue;
		}

		// file is guaranteed to exist
		// get file
		FILE* fp;
		if((fp = fopen(file_name, "r")) == NULL) {
			perror("fopen");
			exit(1);
		}

		int fd = fileno(fp);
		struct stat file_stat;
		if(fstat(fd, &file_stat) == -1) {
			perror("fstat");
			exit(1);
		}

		/* FORMAT RESPONSE */
		// status line
		char* status = "HTTP/1.1 200 OK";
		// Connection: header
		char* connection_h = "Connection: close";
		// Date: header
		char date_h[DATE_SIZE];
		time_t current = time(0);
		struct tm curr_tm = *gmtime(&current);
		strftime(date_h, DATE_SIZE, "Date: %a, %d %b %Y %H:%M:%S %Z", &curr_tm);
		// Server: header
		char* server_h = "Server: Jason's server (Ubuntu)";
		// Last-Modified: header
		char mod_h[DATE_SIZE];
		struct tm mod_tm = *gmtime(&(file_stat.st_mtime));
		strftime(mod_h, DATE_SIZE, "Last-Modified: %a, %d %b %Y %H:%M:%S %Z", &mod_tm);
		// Content-Length: header; TA said file is no larger than 1MB
		// easier to write to response buffer in sprintf
		// Content-Type: header
		char* con_type = NULL;
	
		if(has_extension) {	
			if((strcmp(extension, "html") == 0) || (strcmp(extension, "htm") == 0)) {
				con_type = "Content-Type: text/html";
			} else if(strcmp(extension, "txt") == 0) {
				con_type = "Content-Type: text/plain";
			} else if((strcmp(extension, "jpeg") == 0) || (strcmp(extension, "jpg") == 0)) {
				con_type = "Content-Type: image/jpeg";
			} else if(strcmp(extension, "png") == 0) {
				con_type = "Content-Type: image/png";
			}
		} else {
			con_type = "Content-Type: application/octet-stream";
		}

		/* SEND HTTP RESPONSE */
		// compose entire response
		sprintf(response_buf, "%s\r\n%s\r\n%s\r\n%s\r\n%s\r\nContent-Length: %jd\r\n%s\r\n\r\n", 
			status, connection_h, date_h, server_h, mod_h, (intmax_t)file_stat.st_size, con_type);
		// send response
		write(client_fd, response_buf, strlen(response_buf));
		// send file
		// allocate a buffer that is the size of the requested file plus 1 for EOF (times size of char)
		char* file_buf = (char*) malloc(sizeof(char)*(file_stat.st_size + 1));
		if(file_buf == NULL) {
			perror("malloc");
			exit(1);
		}

		// using fread since using fileno to get file descriptor is throwing "implicit declaration" warning
		ssize_t bytes_read = read(fd, file_buf, file_stat.st_size);
		if(bytes_read < 0) {
			perror("read");
			exit(1);
		}

		write(client_fd, file_buf, bytes_read);

		fclose(fp);
		free(file_buf);
		close(client_fd);
	}
	close(sockfd);
	return 0;
}
