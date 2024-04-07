#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define DEFAULT_DATA_SIZE_BYTES     (int)512
#define TFTP_PORT                   (int)69
#define LAB_BROADCAST               (in_addr_t)0xC0A818FF
#define MODE                        (char*)"octet"

/* TFTP op-codes */
#define OP_RRQ		                (uint16_t) 1	
#define OP_DATA		                (uint16_t) 3
#define OP_ACK		                (uint16_t) 4
#define	OP_ERROR	                (uint16_t) 5

#define TIMEOUT_SECS                (int) 1
#define MAX_RETRIES                 (int) 5



static void help(void);
int main(int argc, char* argv[])
{
    // Set port to 69
    int port = TFTP_PORT;

    // Socket & IP vars
    struct sockaddr_in server;
    int sock = 0;   // Socket desriptor
    char* file_path = NULL;  // -f <filename>
    uint8_t buffer[DEFAULT_DATA_SIZE_BYTES] = {'\0'};
    int data_packets_recieved = 0;
    int retry_count = 0;
    int all_data_recieved = 0;
    int data_recieved = 0;
    
    server.sin_addr.s_addr = htonl(LAB_BROADCAST);
    server.sin_port = htons(port);
    server.sin_family = AF_INET;

    char c = '\0';     // - char

    // Modification: Removed user ability to change output file name
    // -g for tftp get
    // -i for server ip address in dotted decimal
    // -f for file name
    while ((c = getopt(argc, argv, "hi:f:"))!= -1)
    {
        switch (c)
        {
            case 'i':
                if(!inet_pton(AF_INET,optarg,&(server.sin_addr)))
                {
                    printf("Improper IP address\n");
                }
                break;
            case 'p':
                port = atoi(optarg);
                printf("Connecting to port: %d\n", port);
                break;
            case 'f':
                file_path = optarg;
                break;
            case 'h':
                help();
                exit(1);
                break;
         }
    }

    if (file_path == NULL) {
        fprintf(stderr, "Error: -f option is required\n");
        exit(1);
    }

    /* Always a RRQ when first run */
    // Set client mode to octet
    uint16_t opcode = htons(OP_RRQ);

    // Put opcode into bytes 1 and 2 in buffer
    memcpy(buffer, &opcode, sizeof(uint16_t));

    // Put file name into packet
    strncat((char *)(buffer + sizeof(uint8_t)+sizeof(uint8_t)), file_path, sizeof(buffer) - sizeof(uint16_t) - 1);      
    // Put octet mode into buffer
    strncat((char*)buffer+sizeof(uint16_t) + strlen(file_path)+1, MODE, sizeof(buffer) - sizeof(uint16_t) - strlen(file_path) - 1);

    // Calculate packet size
    int packet_size = sizeof(uint16_t) + strlen(file_path) + 1 + strlen(MODE) + 1;
    /* End of RRQ packet setup*/


    /* Open socket to send and recieve data from TFTP Sserver*/
	if ((sock = socket( AF_INET, SOCK_DGRAM, 0 )) < 0) 
	{
		perror("Error on socket creation");
		exit(1);
	}
    
    socklen_t server_len = sizeof(server);

    // Send initial RRQ
    if (sendto(sock,buffer, packet_size, 0, (struct sockaddr *)&server, server_len) < 0) {
        perror("Error sending RRQ packet");
        exit(1);
    }

    /* Create a file pointer to write the binary data from the UDP serve to local disk */
    FILE *file = fopen(file_path, "wb");
    if (!file) {
        perror("fopen");
        close(sock);
        return 1;
    }

    /* Set a 1s timeout */
    // Taken from https://stackoverflow.com/questions/13547721/udp-socket-set-timeout
    // Added interror if statement commands to execute
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SECS;
    tv.tv_usec = 0;

     if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        fclose(file);
        close(sock);
        return 1;
    }

    // Create a temp array to hold last ACK
    uint8_t* prev_ack = NULL;

    // Bytes recieved by server
    int bytes_received = 0;

    do 
    {
        socklen_t server_len = sizeof(server);

        // Attempt to recieve data from the UDP server
        memset(buffer, 0, DEFAULT_DATA_SIZE_BYTES);
        bytes_received = recvfrom(sock, buffer, DEFAULT_DATA_SIZE_BYTES, 0, (struct sockaddr *)&server, &server_len);
        printf("Bytes recieved: %i\n", bytes_received);

        /* If the timeout was triggered, make sure that it was from expected flags */
        if (bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout occurred. No data received.\n");
                data_recieved=0;


                // Resend ACK
                if (prev_ack) {
                    sendto(sock, prev_ack, sizeof(uint16_t) + sizeof(uint16_t), 0, (struct sockaddr*)&server, server_len);
                }
                retry_count++;

                continue; // Continue to the next iteration of the loop
            } else {
                perror("Recieve Error\n");
                fclose(file);
                close(sock);
                free(prev_ack);
                return 1;
            }
        }

        // Make sure there is a null terminator before trying to print
        // to console.  There is no expectation null would be included
        // in UDP payload.
        (bytes_received <= DEFAULT_DATA_SIZE_BYTES) ? (buffer[bytes_received] = '\0') : (buffer[DEFAULT_DATA_SIZE_BYTES - 1] = '\0');

        // Only look at Least-significant-BYTE for opcode since should never be larger than 1 byte in value
        opcode = ntohs(*(uint16_t*)&buffer[0]);     
//        printf("opcode: %d\n", opcode);   
        
        switch(opcode)
        {
            case OP_ERROR:
                data_recieved=0;
                // Get error code and error message
                uint16_t error_code = ntohs(*(uint16_t*)&buffer[sizeof(uint16_t)]);
                char* error_msg = (char*)buffer+sizeof(uint16_t) + sizeof(uint16_t);

                // Print the error
                printf("Error Code: %hu - %s\n", error_code, error_msg);
                break;

            case OP_DATA:
                retry_count = 0;
                data_recieved=1;
                data_packets_recieved++;
                
                uint16_t block_number = ntohs(buffer[3]<<8)|(ntohs(buffer[2])); // Combine the two block number bytes
                uint16_t data_size = bytes_received - sizeof(uint16_t) - sizeof(uint16_t);
                

                /* Display Progress */
                printf("Block: %i\t Data Size: %i Bytes\n", block_number,data_size);

                if(data_size < DEFAULT_DATA_SIZE_BYTES-4)
                {
                    all_data_recieved = 1;
                }

                /* Write data to local disk */
                size_t bytes_written = fwrite(buffer + sizeof(uint16_t) + sizeof(uint16_t), 1, data_size, file);
//                printf("Buffer: %s",buffer+2*sizeof(uint16_t));
                if (bytes_written != data_size) {
                    perror("Error writing to file");
                    fclose(file);
                    close(sock);
                    free(prev_ack);
                    return 1;
                }
//                printf("Bytes written: %li\n", bytes_written);
                
                /* Data recieved from server, so send ACK back */
                // Only opcode is changed
                uint16_t ack_opcode = htons(OP_ACK);
                memcpy(buffer, &ack_opcode, sizeof(uint16_t));

                // Create copy of ACK packet in case of timeout and retransmission
                if (prev_ack) {
                    free(prev_ack);
                }
                
                prev_ack = (uint8_t*)calloc(sizeof(uint8_t) + sizeof(uint8_t), sizeof(uint16_t));
                memcpy(prev_ack, buffer, sizeof(buffer[0]) + sizeof(buffer[1]) + 1);

                // Set the block number in the ACK packet
                uint16_t ack_block_number = htons(block_number);
                memcpy(prev_ack + sizeof(uint16_t), &ack_block_number, sizeof(uint16_t));

                if (sendto(sock, prev_ack, sizeof(uint16_t) + sizeof(uint16_t), 0, (struct sockaddr*)&server, server_len) < 0) {
                    perror("Error sending ACK packet");
                    fclose(file);
                    close(sock);
                    free(prev_ack);
                    return 1;
                }


                // printf("opcode: %d\n", ntohs(*(uint16_t*)&prev_ack[0]));
                // printf("Block Number: %d\n", ntohs(*(uint16_t*)&prev_ack[2]));
                break;
            default:
                perror("Unsupported opcode from server\n");
                fclose(file);
                close(sock);
                free(prev_ack);
                return 1;
        }

        // Print amount of bytes sent and the IP of the server it sent it to
        //printf("Sent %d bytes to %s\n",packet_size,inet_ntoa(server.sin_addr));
    } while(retry_count < MAX_RETRIES && (bytes_received > 0 || bytes_received == -1) && !all_data_recieved);

    all_data_recieved = 0;

    

    /* Delete File */
    if(!data_recieved && retry_count >= MAX_RETRIES)
    {
        (!remove(file_path))? printf("Deleted succesfully\n"): printf("Unable to delete the file\n");
    }


    // Close file
    fclose(file);

    // close socket
	close(sock);

    if(prev_ack)
    {
        free(prev_ack);

    }
	// done
	return 0;
}


static void help(void)
{
    printf("-i <ip of udp server> -p <port to listen> -f <file name> -h (print help)\n");
    printf("-f option is required to specify the file name to download\n");
    printf("-i option is optional and can be used to specify the IP address of the UDP server\n");
    printf("-p option is optional and can be used to specify the port number to listen on\n");
    printf("If -i and -p options are not specified, the program will use the default IP address and port number\n");
    printf("The program will download the specified file from the UDP server using the TFTP protocol\n");
}