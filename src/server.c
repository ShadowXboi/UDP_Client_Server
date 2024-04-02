#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define PORT 65535

// Define a simple packet structure (for illustration)
typedef struct
{
    int  sequence_number;
    char data[BUFFER_SIZE];
} Packet;

// Function prototype
static void signal_handler(int signal_number);

int main(int argc, const char *argv[])
{
    int                sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t          client_addr_len = sizeof(client_addr);
    Packet             packet;

    // Signal handling setup
    signal(SIGINT, signal_handler);

    // Creating a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    // Bind the socket to the server address
    if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %d...\n", PORT);

    // Main loop for receiving data and sending ACKs
    while(1)
    {
        ssize_t recv_len;
        recv_len = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if(recv_len > 0)
        {
            // Assuming the packet structure and data are correctly received
            printf("Received: %s | Seq: %d\n", packet.data, packet.sequence_number);

            // Prepare and send an acknowledgment back to the client
            char ack_message[BUFFER_SIZE];
            snprintf(ack_message, BUFFER_SIZE, "ACK for Seq: %d", packet.sequence_number);
            sendto(sockfd, ack_message, strlen(ack_message), 0, (struct sockaddr *)&client_addr, client_addr_len);
        }
    }

    close(sockfd);
    return 0;
}

// Signal handler function
static void signal_handler(int signal_number)
{
    if(signal_number == SIGINT)
    {
        printf("\nReceived SIGINT, server shutting down...\n");
        exit(EXIT_SUCCESS);
    }
}
