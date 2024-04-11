#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>


#ifndef SOCK_CLOEXEC
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-macros"
    #define SOCK_CLOEXEC 0
    #pragma GCC diagnostic pop
#endif

#define BASE 10
#define BUFFER_SIZE 1024
#define DEFAULT_PORT 65535

// a simple packet structure (for illustration)
typedef struct
{
    int  sequence_number;
    char data[BUFFER_SIZE];
} Packet;

typedef struct
{
    int x;
    int y;
} Position;

static Position charPosition = {0, 0};    // Initialize character position

// Function prototype
void broadcast_position(int sockfd, struct sockaddr_in *client_addr, socklen_t client_addr_len);

int main(int argc, const char *argv[])
{
    char  *end;
    long   psort;
    int    sockfd;
    Packet packet;
    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr;
    socklen_t          client_addr_len = sizeof(client_addr);


    // Validate command line arguments for port number
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Validate the port number

    psort = strtol(argv[1], &end, BASE);
    if(*end != '\0' || psort <= 0 || psort > DEFAULT_PORT)
    {
        fprintf(stderr, "Invalid port number. Please specify a port number between 1 and 65535.\n");
        exit(EXIT_FAILURE);
    }

    // Creating and binding a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if(sockfd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port        = htons((uint16_t)psort);

    // Bind the socket to the server address
    if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %s...\n", argv[1]);

    while(1)
    {
        char    ack_message[BUFFER_SIZE];
        ssize_t recv_len = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if(recv_len > 0)
        {
            printf("Received: %s | Seq: %d\n", packet.data, packet.sequence_number);

            // Construct an ACK message including the sequence number
            snprintf(ack_message, BUFFER_SIZE, "ACK %d", packet.sequence_number);

            // Send the ACK back to the client
            if(sendto(sockfd, ack_message, strlen(ack_message), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0)
            {
                perror("sendto failed while sending ACK");
            }
        }
        else if(recv_len == -1)
        {
            perror("recvfrom failed");
        }
    }

    //   return 0;
}

void broadcast_position(int sockfd, struct sockaddr_in *client_addr, socklen_t client_addr_len)
{
    char updateMsg[BUFFER_SIZE];
    snprintf(updateMsg, sizeof(updateMsg), "New Position: X=%d, Y=%d", charPosition.x, charPosition.y);
    sendto(sockfd, updateMsg, strlen(updateMsg), 0, (struct sockaddr *)client_addr, client_addr_len);
}
