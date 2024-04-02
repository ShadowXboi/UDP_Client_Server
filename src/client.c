#include <arpa/inet.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

// Define a simple packet structure
typedef struct
{
    int  sequence_number;
    char data[BUFFER_SIZE];
} Packet;

int main(void)
{
    int                sockfd;
    struct sockaddr_in server_addr;
    Packet             packet;
    char               ack_message[BUFFER_SIZE];
    int                sequence_number = 0;
    socklen_t          len;
    ssize_t            n;
    bool               should_exit = false;

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
    noecho();                // Don't echo inputted keys
    keypad(stdscr, TRUE);    // Enable special keys to be captured

    while(!should_exit)
    {                                          // Use the flag in the loop condition
        int ch = getch();                      // Get character (non-blocking)
        memset(&packet, 0, sizeof(packet));    // Clear packet buffer
        packet.sequence_number = sequence_number;

        switch(ch)
        {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
                // [Handle arrow keys as before, setting packet data]
                break;
            case 'q':    // Modify to set the flag instead of exiting directly
                should_exit = true;
                continue;    // Skip the rest of the loop iteration
        }

        // Send packet to server if one of the arrow keys was pressed
        if(strlen(packet.data) > 0)
        {
            sendto(sockfd, &packet, sizeof(packet), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

            // Wait for ACK
            len            = sizeof(server_addr);
            n              = recvfrom(sockfd, ack_message, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);
            ack_message[n] = '\0';    // Null-terminate the ACK message
            if(n > 0)
            {
                // Assuming ACK message format is "ACK for Seq: X"
                printf("Received: %s\n", ack_message);    // Optionally print the ACK message for debugging
                sequence_number++;                        // Increment sequence number for the next packet
            }
        }
    }

    // Cleanup
    endwin();    // End ncurses mode
    close(sockfd);
    return 0;
}
