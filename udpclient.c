// Simple UDP client
// CPE 3300, Daniel Nimsgern
//
// Build with gcc -o udpclient udpclient.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Max message to echo
#define MAX_MESSAGE     (int)       1000
#define LAB_BROADCAST   (in_addr_t) 0xC0A818FF
#define HOME_BROADCAST  (in_addr_t) 0xC0A801FF

/* server main routine */
int main(int argc, char** argv) 
{
    // locals
	unsigned short default_port = 3300; // default port
    struct sockaddr_in ip;
    ip.sin_family = AF_INET;
    ip.sin_addr.s_addr = htonl(LAB_BROADCAST);
    ip.sin_port = htons(default_port);

	int sock; // socket descriptor

    // Was help requested?  Print usage statement
	if (argc > 1 && ((!strcmp(argv[1],"-?"))||(!strcmp(argv[1],"-h"))))
	{
		printf("\nUsage: port %d.\n\n", default_port);
		exit(1);
	}

    // get the IPv4 address and port of the recipient
    if (argc > 1 && !strcmp(argv[1], "-ip"))
    {
        if(!inet_pton(AF_INET,argv[2],&(ip.sin_addr)))
        {
            printf("Improper IP address\n");
        }
        else
        {
            ip.sin_port = htons(atoi(argv[3]));

            printf("UDP echo Server configured on IP address: %s\n",
                   inet_ntoa(ip.sin_addr));
        }
    }

    // ready to go
	printf("UDP echo Server(s) configuring on port: %d\n",ntohs(ip.sin_port));
	
	// for UDP, we want IP protocol domain (AF_INET)
	// and UDP transport type (SOCK_DGRAM)
	// no alternate protocol - 0, since we have already specified IP
	
	if ((sock = socket( AF_INET, SOCK_DGRAM, 0 )) < 0) 
	{
		perror("Error on socket creation");
		exit(1);
	}

    char* messsage_buffer = calloc(MAX_MESSAGE,sizeof(char));
    int message_length;
    struct sockaddr_in from;
    socklen_t ip_len = sizeof(ip);
    int sent;
    int recieved;
    while(1)
    {
        fgets(messsage_buffer,MAX_MESSAGE,stdin);

        message_length = strlen(messsage_buffer) + 1;

        sent = sendto(sock,messsage_buffer, message_length, 0,
                      (struct sockaddr *)&ip, ip_len);

        printf("Sent %d bytes to %s\n",sent,inet_ntoa(ip.sin_addr));

        do
        {
            recieved = recvfrom(sock, messsage_buffer, MAX_MESSAGE, 0,
                                (struct sockaddr *)&from, &ip_len);

            // print info to console
		    printf("Received message from %s port %d\n",
			       inet_ntoa(from.sin_addr), ntohs(from.sin_port));

            if (recieved < 0)
		    {
			    perror("Error receiving data");
		    }
		    else
		    {
			    // Make sure there is a null terminator before trying to print
			    // to console.  There is no expectation null would be included
			    // in UDP payload.
			    if (recieved < MAX_MESSAGE)
				    messsage_buffer[recieved] = '\0';
			    else
				    messsage_buffer[MAX_MESSAGE-1] = '\0';
				
			    // put message to console
			    printf("Return message: %s\n",messsage_buffer);
            }

        } while (ntohl(ip.sin_addr.s_addr) == LAB_BROADCAST);
    }

    // close socket
	close(sock);
	// done
	return(0);
}