/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define PORT "80"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 100
void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void send_file_content(int client_socket, const char *filename) {
    FILE *file = fopen(filename, "r");
	size_t content_length;

	fseek(file, 0L, SEEK_END);
	content_length = ftell(file);
	fseek(file, 0L, SEEK_SET);

    if (file == NULL) {
        // Send 404 if file not found
        const char *not_found_response = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "404 Not Found";
        send(client_socket, not_found_response, strlen(not_found_response), 0);
        return;
    }

    // Send 200 OK header
    char response_header[MAXDATASIZE];
	snprintf(response_header, sizeof(response_header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n", content_length);
    send(client_socket, response_header, strlen(response_header), 0);

    char file_buffer[MAXDATASIZE];
    size_t n;

    // Read and send the file in chunks
    while ((n = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        if (send(client_socket, file_buffer, n, 0) == -1) {
            perror("Failed to send file chunk");
            break;
        }
    }
	fclose(file);
}

int main(int arg, char *argv[])
{
	char buf[MAXDATASIZE];
	int numbytes;

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	pthread_t thread_id;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM; //TCP
	hints.ai_flags = AI_PASSIVE; // use my IP
	
	//Setup TCP param structure
	//                  which ip              store info
	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		// create TCP socket
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		//                                reuse port immediately
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		// bind the socket
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure
	
	// Listen Mode
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}


	// check for child process
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		    // start accpet
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if(new_fd==-1){
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
				perror("recv");
				exit(1);
			}
			buf[numbytes] = '\0';
		
			if (strncmp(buf, "GET /", 5) == 0) {
				char *file_requested = strtok(buf + 5, " ");  // Extract file name from the request
				if (strcmp(file_requested, "") == 0 || strcmp(file_requested, "/") == 0) {
					file_requested = "index.html";  // Serve default file if none specified
				}

				// Send the content of the requested file
				send_file_content(new_fd, file_requested);
			} else {
				// Send a 400 Bad Request if the request is invalid
				const char *bad_request_response = 
					"HTTP/1.1 400 Bad Request\r\n"
					"Content-Length: 15\r\n"
					"\r\n"
					"400 Bad Request";
				send(new_fd, bad_request_response, strlen(bad_request_response),0);
			}
			
			close(new_fd);  
			exit(0);
		}
		close(new_fd); // parent doesn't need this
	}

	return 0;
}

