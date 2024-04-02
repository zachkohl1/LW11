// Simple UDP echo server
// CPE 3300, Dr. Rothe, Daniel Nimsgern
//
// Build with gcc -o udpechoserver udpecchoserver.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Max message to echo
#define MAX_MESSAGE	1000

/* server main routine */
int main(int argc, char** argv) 
{
	// locals
	unsigned short port = 3300; // default port
	int sock; // socket descriptor

	// Was help requested?  Print usage statement
	if (argc > 1 && ((!strcmp(argv[1],"-?"))||(!strcmp(argv[1],"-h"))))
	{
		printf("\nUsage: udpechoserver [-p port] where port is the requested \
			port that the server monitors.  If no port is provided, the server \
			listens on port %d.\n\n", port);
		exit(1);
	}
	
	// get the port from ARGV
	if (argc > 1 && !strcmp(argv[1],"-p"))
	{
		if (sscanf(argv[2],"%hu",&port)!=1)
		{
			perror("Error parsing port option");
			exit(1);
		}
	}
	
	// ready to go
	printf("UDP Echo Server configuring on port: %d\n",port);
	
	// for UDP, we want IP protocol domain (AF_INET)
	// and UDP transport type (SOCK_DGRAM)
	// no alternate protocol - 0, since we have already specified IP
	
	if ((sock = socket( AF_INET, SOCK_DGRAM, 0 )) < 0) 
	{
		perror("Error on socket creation");
		exit(1);
	}
  
	// establish address - this is the server and will
	// only be listening on the specified port
	struct sockaddr_in sock_address;
	
	// address family is AF_INET
	// fill in INADDR_ANY for address (any of our IP addresses)
	// for a client, this would be the desitation address
    // the port number is per default or option above
	// note that address and port must be in memory in network order

	sock_address.sin_family = AF_INET;
	sock_address.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_address.sin_port = htons(port);

	// we must now bind the socket descriptor to the address info
	if (bind(sock, (struct sockaddr *) &sock_address, sizeof(sock_address))<0)
	{
		perror("Problem binding");
		exit(1);
	}

	// go into forever loop and echo whatever message is received
	// to console and back to source
	char* buffer = calloc(MAX_MESSAGE,sizeof(char));
	char* sendBuffer = calloc(MAX_MESSAGE,sizeof(char));
	char* echoTag = "Dan says ";
	int bytes_read;
    struct sockaddr_in from;
	socklen_t from_len;
	int echoed;
	
    while (1) 
	{
		from_len = sizeof(from);
		
		// read datagram and put into buffer
		bytes_read = recvfrom( sock ,buffer, MAX_MESSAGE,
				0, (struct sockaddr *)&from, &from_len);

		// print info to console
		printf("Received message from %s port %d\n",
			inet_ntoa(from.sin_addr), ntohs(from.sin_port));

		if (bytes_read < 0)
		{
			perror("Error receiving data");
		}
		else
		{
			// Make sure there is a null terminator before trying to print
			// to console.  There is no expectation null would be included
			// in UDP payload.
			if (bytes_read < MAX_MESSAGE)
				buffer[bytes_read] = '\0';
			else
				buffer[MAX_MESSAGE-1] = '\0';
				
			// put message to console
			printf("Message: %s\n",buffer);

			strcpy(sendBuffer,echoTag);
			printf("Echotag: %s\n",sendBuffer);
			strcat(sendBuffer,buffer);
			printf("Sendmessage: %s\n",sendBuffer);

			echoed = sendto(sock, sendBuffer, bytes_read+9, 0,
				(struct sockaddr *)&from, from_len);

			if (echoed < 0)
				perror("Error sending echo");
			else	
				printf("Bytes echoed: %d\n",echoed);			
		}
    }

	// minor issue - we will never get here...

	// release buffer
	free(buffer);
	free(sendBuffer);
	// close socket
	close(sock);
	// done
	return(0);
}








