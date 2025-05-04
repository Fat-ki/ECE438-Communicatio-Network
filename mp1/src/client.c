/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#include <arpa/inet.h>

#define PORT "80" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void extract_port(const char *url, char *port, size_t port_size) {
    char *first_col;
	char *port_start;
    char *port_end;

    // Ensure the buffer is clean
    memset(port, 0, port_size);

    // Find the colon after the IP address (or hostname)
    first_col = strstr(url, ":");
	first_col++;
	port_start = strstr(first_col,":");
    if (port_start != NULL) {
        port_start++;  // Skip the colon
        port_end = strchr(port_start, '/');
        
        if (port_end != NULL) {
            // Copy the port substring into the port buffer (port_end - port_start characters)
            if (port_end - port_start < port_size) {
                strncpy(port, port_start, port_end - port_start);
                port[port_end - port_start] = '\0';  // Null-terminate the string
            } else {
                fprintf(stderr, "Port string too long.\n");
                exit(1);
            }
        } else {
            // If there's no '/', copy everything after the colon (assumed to be the port)
            strncpy(port, port_start, port_size - 1);
            port[port_size - 1] = '\0';  // Null-terminate
        }
    } else {
        strncpy(port, "80", port_size);
        port[port_size - 1] = '\0';  // Null-terminate just in case
    }
}

void extract_ip_address(const char *url, char *ip, size_t ip_size) {
    char *ip_start, *ip_end;

    ip_start = strstr(url, "://");
    if (ip_start != NULL) {
        ip_start += 3;  // Skip "://"
    } else {
        ip_start = (char *)url;  // In case there's no protocol, start at the beginning
    }

    ip_end = strpbrk(ip_start, ":/");
    if (ip_end != NULL) {
        strncpy(ip, ip_start, ip_end - ip_start);
        ip[ip_end - ip_start] = '\0';  // Null-terminate the string
    } else {
        strncpy(ip, ip_start, ip_size - 1);
        ip[ip_size - 1] = '\0';  // Null-terminate the string if no colon or slash
    }
}

void extract_file_path(const char *url, char *path, size_t path_size) {
    char *path_start;
	const char *get_prefix = "GET /";
	char *ip;

	ip = strstr(url, "://");
	ip+=3;
	path_start = strstr(ip, "/");
	
    if (path_start != NULL) {
        strncpy(path, path_start, path_size - 1);
        path[path_size - 1] = '\0';
    } else {
       	strncpy(path, "/", path_size - 1);  // If no path is provided, default to "/"
        path[path_size - 1] = '\0';  // Null-terminate
    }
}

void send_http_request(int sockfd, const char *path, const char *host, const char *port) {
    char request[MAXDATASIZE];

    // Format the GET request
    if (strcmp(port, "80") == 0) {
        snprintf(request, sizeof(request),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Connection: close\r\n\r\n", path, host);
    } else {
        snprintf(request, sizeof(request),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s:%s\r\n"
                 "Connection: close\r\n\r\n", path, host, port);
    }

    // Send the request to the server
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("send");
        close(sockfd);
        exit(1);
    }
}

void receive_and_write(int sockfd, FILE *file) {
    char buffer[MAXDATASIZE];
    int numbytes;
    char *header_end;
    int header_length = 0;

    if ((numbytes = recv(sockfd, buffer, MAXDATASIZE - 1, 0)) <= 0) {
        perror("recv");
        return;
    }
    buffer[numbytes] = '\0';
    header_end = strstr(buffer, "\r\n\r\n");

    if (header_end) {
        header_length = header_end - buffer + 4;  // Including the "\r\n\r\n"
        fwrite(buffer + header_length, 1, numbytes - header_length, file);
    }

    while ((numbytes = recv(sockfd, buffer, MAXDATASIZE - 1, 0)) > 0) {
        fwrite(buffer, 1, numbytes, file);
    }

    if (numbytes == -1) {
        perror("recv");
    }
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE],content[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	FILE *file = fopen("output","w");

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	char ip[MAXDATASIZE] = {0};
    char port[6] = {0};  // Buffer for port as string (5 digits + null terminator)
    char path[MAXDATASIZE] = {0};

    const char *url = argv[1];

    // Extract the IP address
    extract_ip_address(url, ip, sizeof(ip));
    // Extract the port (if provided)
    extract_port(url, port, sizeof(port));
    // Extract the file path
    extract_file_path(url, path, sizeof(path));
    //printf("ip: %s\n",ip);
    //printf("port: %s\n",port);
    //printf("path: %s\n",path);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	

	if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}
	
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	//send request
	send_http_request(sockfd,path,ip,port);

	//receive message
	receive_and_write(sockfd,file);
	
	freeaddrinfo(servinfo); // all done with this structure
	close(sockfd);
	
	return 0;
}

