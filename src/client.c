#include "client.h"
#include <arpa/inet.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef SOCK_CLOEXEC
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-macros"
    #define SOCK_CLOEXEC 0
    #pragma GCC diagnostic pop
#endif

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 65535
#define BUFFER_SIZE 1024
#define ACK_TIMEOUT 2    // timeout value

// Define a simple packet structure
typedef struct
{
    int  sequence_number;
    char data[BUFFER_SIZE];
} Packet;

void send_packet_with_retry(int sockfd, struct sockaddr_in *server_addr, const Packet *packet);
int wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr, int expected_seq_num);


int run_client(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <Server IP> <Server Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char* SERVER_IP = argv[1];
    int SERVER_PORT = atoi(argv[2]);

    int                sockfd;
    struct sockaddr_in server_addr;
    Packet             packet;
    int                sequence_number = 0;

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Fill server information
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();                 // Don't echo inputted keys
    keypad(stdscr, TRUE);     // Enable special keys to be captured
    nodelay(stdscr, TRUE);    // Make getch non-blocking

    printw("Press arrow keys to move the character. 'q' to quit.\n");
    refresh();

    while(1)
    {
        bool send_command = true;
        int  ch           = getch();

        memset(packet.data, 0, BUFFER_SIZE);    // Clear previous command
        packet.sequence_number = sequence_number++;

        switch(ch)
        {
            case KEY_UP:
                strcpy(packet.data, "MOVE UP");
                break;
            case KEY_DOWN:
                strcpy(packet.data, "MOVE DOWN");
                break;
            case KEY_LEFT:
                strcpy(packet.data, "MOVE LEFT");
                break;
            case KEY_RIGHT:
                strcpy(packet.data, "MOVE RIGHT");
                break;
            case 'q':        // Quit command
                endwin();    // End ncurses mode before exiting
                close(sockfd);
                return 0;
            default:
                send_command = false;    // No recognized command, don't send
                break;
        }

        if(send_command)
        {
            send_packet_with_retry(sockfd, &server_addr, &packet);
        }
    }


     return 0; //should never reach here
}

void send_packet_with_retry(int sockfd, struct sockaddr_in *server_addr, const Packet *packet)
{
    char ack_message[BUFFER_SIZE];
    int  retries = 3;    // Number of retries
    for(int attempt = 0; attempt < retries; ++attempt)
    {
        sendto(sockfd, packet, sizeof(*packet), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));

        // Pass the expected sequence number to wait_for_ack
        if(wait_for_ack(sockfd, ack_message, server_addr, packet->sequence_number))
        {
            printf("ACK received for Seq: %d\n", packet->sequence_number);
            return;
        }

        printf("ACK not received for Seq: %d, retrying...\n", packet->sequence_number);
    }
    printf("Failed to receive ACK for Seq: %d after %d attempts.\n", packet->sequence_number, retries);
}

// Adjust the function prototype to include the expected sequence number.
int wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr, int expected_seq_num)
{
    struct timeval tv;
    fd_set         readfds;

    // Set the timeout for ACK reception
    tv.tv_sec  = ACK_TIMEOUT;
    tv.tv_usec = 0;

    // Clear the set and add sockfd
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    if(select(sockfd + 1, &readfds, NULL, NULL, &tv) > 0)
    {
        int       received_seqnum;
        socklen_t len = sizeof(*server_addr);
        ssize_t   n   = recvfrom(sockfd, ack_message, BUFFER_SIZE - 1, 0, (struct sockaddr *)server_addr, &len);
        if(n > 0)
        {
            ack_message[n] = '\0';    // Null-terminate the received message
            // Check if the ACK message contains the expected sequence number
            if(sscanf(ack_message, "ACK %d", &received_seqnum) == 1)
            {
                if(received_seqnum == expected_seq_num)
                {
                    return 1;    // Correct ACK received
                }
            }
        }
    }
    return 0;    // ACK not received or not correct
}
