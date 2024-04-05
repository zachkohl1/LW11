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
#define DEFAULT_DATA_SIZE_BYTES    	(int)512

#define OP_RRQ		                (uint16_t) 1	
#define OP_ACK		                (uint16_t) 4
#define OP_DATA		                (uint16_t) 3


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
	char* buffer = calloc(DEFAULT_DATA_SIZE_BYTES + sizeof(uint16_t) + sizeof(uint16_t),sizeof(char));
	char* send_buffer = calloc(DEFAULT_DATA_SIZE_BYTES + sizeof(uint16_t) + sizeof(uint16_t), sizeof(char));
	char* file_buffer;
	int file_len;
	int bytes_read;
    struct sockaddr_in from;
	socklen_t from_len;
	int num_bytes_to_send = 0;
	int bytes_sent = 0;
	int block_number = 0;
	int n;

	/**
	 * @TODO: Client give a read request, server should look for read-request and file from filename, then send data back
	*/
    while (1) 
	{
		from_len = sizeof(from);
		
		// read datagram and put into buffer
		bytes_read = recvfrom( sock ,buffer, DEFAULT_DATA_SIZE_BYTES,
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
			if (bytes_read < DEFAULT_DATA_SIZE_BYTES)
				buffer[bytes_read] = '\0';
			else
				buffer[DEFAULT_DATA_SIZE_BYTES-1] = '\0';

			// Only look at Least-significant-BYTE for opcode since should never be larger than 1 byte in value
			uint16_t opcode = ntohs(*(uint16_t*)&buffer[1]);

			switch(opcode)
			{
				case OP_RRQ:
					// Get filename
					char* path = &buffer[sizeof(uint16_t)];

					/* Create a file pointer to write the binary data from the UDP serve to local disk */
					FILE *file = fopen(path, "rb");
					if (!file) {
						perror("fopen");
						close(sock);
						return 1;
					}
					// Go to end of file
					fseek(file, 0, SEEK_END);

					// Record file length
					file_len = ftell(file);

					// Go back to beginning of file
					rewind(file);
					
					if((file_buffer = (char*)malloc(file_len)) == NULL)
					{
						perror("Error: Erorr reading the file %s \n");
						free(buffer);
						free(send_buffer);
						return 1;
					}

					// Read file
					bytes_read = fread(file_buffer, 1, DEFAULT_DATA_SIZE_BYTES, file);

					if(!bytes_read)
					{
						perror("Error reading file\n");
						fclose(file);
						free(buffer);
						free(file_buffer);
						free(send_buffer);
						return 1;
					} else {
						if(bytes_read <  DEFAULT_DATA_SIZE_BYTES)
						{
							num_bytes_to_send = DEFAULT_DATA_SIZE_BYTES - bytes_read;
						} else {
							num_bytes_to_send = DEFAULT_DATA_SIZE_BYTES;
						}
					}

					// Prepare to send data back. First 2 bytes is op code
					send_buffer[1] = htons(OP_DATA);
					uint16_t block = htons(block_number);
					// Attach block #
					memcpy(send_buffer + sizeof(uint16_t), &block, sizeof(uint16_t));

					// Attach bytes from file
					memcpy(send_buffer + sizeof(uint16_t) + sizeof(uint16_t), file_buffer, num_bytes_to_send);

					// Send first block worth
					bytes_sent = sendto(sock, send_buffer, num_bytes_to_send, 0, (struct sockaddr *)&from, from_len);
					block_number++;
					break;
				
				/* Continue sending data */
				case OP_ACK:
					// Determine done sending
					if(bytes_read - bytes_sent)
					{
						/* Not done sending. Send another block */
						if(bytes_read-DEFAULT_DATA_SIZE_BYTES <  DEFAULT_DATA_SIZE_BYTES)
						{
							num_bytes_to_send = DEFAULT_DATA_SIZE_BYTES-bytes_read;
						} else {
							// EOF signaled. No more needs to be sent
							num_bytes_to_send = DEFAULT_DATA_SIZE_BYTES;
						}

						// Prepare to send data back. First 2 bytes is op code
						send_buffer[1] = htons(OP_DATA);
						uint16_t block = htons(block_number);

						// Attach block #
						memcpy(send_buffer + sizeof(uint16_t), &block, sizeof(uint16_t));

						// Attach bytes from file
						memcpy(send_buffer + sizeof(uint16_t) + sizeof(uint16_t), file_buffer+DEFAULT_DATA_SIZE_BYTES, num_bytes_to_send);

						// Send first block worth
						bytes_sent += sendto(sock, send_buffer+DEFAULT_DATA_SIZE_BYTES, num_bytes_to_send, 0, (struct sockaddr *)&from, from_len);
						block_number++;
					} 
					break;

				default:
					perror("Invalid opcode from client\n");
					free(buffer);
					free(file_buffer);
					free(send_buffer);
					close(sock);
					return 1;
			}


			bytes_sent = sendto(sock, file_buffer, bytes_read+9, 0, (struct sockaddr *)&from, from_len);

			if (bytes_sent < 0)
				perror("Error sending file\n");
		
		}
    }

	// minor issue - we will never get here...

	// release buffer
	free(buffer);
	free(file_buffer);
	free(send_buffer);

	// close socket
	close(sock);

	// done
	return 0;
}








