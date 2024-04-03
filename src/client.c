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
#define ACK_TIMEOUT 2

// Define a simple packet structure
typedef struct
{
    int  sequence_number;
    char data[BUFFER_SIZE];
} Packet;

void send_packet_with_retry(int sockfd, struct sockaddr_in *server_addr, Packet *packet);
int wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr);


int main(void)
{
    int                sockfd;
    struct sockaddr_in server_addr;
    Packet             packet;
    char               ack_message[BUFFER_SIZE];
    int                sequence_number = 0;
    socklen_t          len;
    ssize_t            n;


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

    // Send initial handshake message
    strcpy(packet.data, "HELLO");
    packet.sequence_number = sequence_number++;
    send_packet_with_retry(sockfd, &server_addr, &packet);



    while(1)
    {                                          // Use the flag in the loop condition
        int ch = getch();                      // Get character (non-blocking)
        memset(&packet, 0, sizeof(packet));    // Clear packet buffer
        packet.sequence_number = sequence_number;

        if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT) {
            sprintf(packet.data, "MOVE %d", ch); // Simplified, adjust as necessary
            send_packet_with_retry(sockfd, &server_addr, &packet);
        } else if (ch == 'q') {
            break; // Exit loop
        }
    }


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
            default:
                break;
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


void send_packet_with_retry(int sockfd, struct sockaddr_in *server_addr, Packet *packet) {
    int retries = 3; // Number of retries
    for (int attempt = 0; attempt < retries; ++attempt) {
        sendto(sockfd, packet, sizeof(*packet), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));
        char ack_message[BUFFER_SIZE];
        if (wait_for_ack(sockfd, ack_message, server_addr)) {
            printf("ACK received: %s\n", ack_message);
            return;
        } else {
            printf("ACK not received, retrying...\n");
        }
    }
    printf("Failed to receive ACK after %d attempts.\n", retries);
}

int wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr) {
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = ACK_TIMEOUT;
    tv.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    if (select(sockfd + 1, &readfds, NULL, NULL, &tv) > 0) {
        socklen_t len = sizeof(*server_addr);
        ssize_t n = recvfrom(sockfd, ack_message, BUFFER_SIZE, 0, (struct sockaddr *)server_addr, &len);
        if (n > 0) {
            ack_message[n] = '\0';
            return 1; // ACK received
        }
    }
    return 0; // ACK not received
}

