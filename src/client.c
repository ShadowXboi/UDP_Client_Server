#include <arpa/inet.h>
#include <errno.h>    // for errno, EWOULDBLOCK
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>    // For struct timeval
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
int  wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr);

int main(void)
{
    bool               should_exit = false;
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
    nodelay(stdscr, TRUE);    // Make getch non-blocking, Set getch() to non-blocking mode

    // Send initial handshake message
    strcpy(packet.data, "HELLO");
    packet.sequence_number = sequence_number++;
    send_packet_with_retry(sockfd, &server_addr, &packet);

    while(!should_exit)
    {
        int                ch = getch();    // Non-blocking getch
        struct sockaddr_in src_addr;
        socklen_t          src_addr_len = sizeof(src_addr);
        char               buffer[BUFFER_SIZE];
        ssize_t            recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *)&src_addr, &src_addr_len);
        if(ch != ERR)
        {    // Check if a key was pressed
            memset(&packet, 0, sizeof(packet));
            packet.sequence_number = sequence_number++;

            if(ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT)
            {
                // Packet data setup for movement
                sprintf(packet.data, "MOVE %d", ch);    // Example payload
                send_packet_with_retry(sockfd, &server_addr, &packet);
            }
            else if(ch == 'q' || ch == 'Q')
            {
                should_exit = true;    // Exit condition
            }
        }

        // Attempt to receive data from the server

        if(recv_len > 0)
        {
            buffer[recv_len] = '\0';    // Ensure the string is null-terminated
            // Process the received data
            // For instance, updating the game state or printing server messages
            printw("Server: %s\n", buffer);    // Display received message in ncurses window
        }
        else if(errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("recvfrom failed");
            // Optional: handle the error, such as attempting to reconnect
        }

        // Refresh the screen to update any changes made by printw
        refresh();
    }

    // Cleanup before exiting
    endwin();    // End ncurses session
    close(sockfd);
    return 0;
}

void send_packet_with_retry(int sockfd, struct sockaddr_in *server_addr, const Packet *packet)
{
    char ack_message[BUFFER_SIZE];
    int  retries = 3;    // Number of retries
    for(int attempt = 0; attempt < retries; ++attempt)
    {
        sendto(sockfd, packet, sizeof(*packet), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));

        if(wait_for_ack(sockfd, ack_message, server_addr))
        {
            printf("ACK received: %s\n", ack_message);
            return;
        }

        printf("ACK not received, retrying...\n");
    }
    printf("Failed to receive ACK after %d attempts.\n", retries);
}

int wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr)
{
    struct timeval tv;
    fd_set         readfds;

    // Set the timeout for ACK reception
    tv.tv_sec  = ACK_TIMEOUT;
    tv.tv_usec = 0;

    // Initialize the set of active sockets
    // FD_ZERO(&readfds);
    // use memset to clear the fd_set structure.
    memset(&readfds, 0, sizeof(readfds));

    FD_SET(sockfd, &readfds);    // Add sockfd to the set after clearing.

    if(select(sockfd + 1, &readfds, NULL, NULL, &tv) > 0)
    {
        socklen_t len = sizeof(*server_addr);
        ssize_t   n   = recvfrom(sockfd, ack_message, BUFFER_SIZE - 1, 0, (struct sockaddr *)server_addr, &len);
        if(n > 0)
        {
            ack_message[n] = '\0';    // Null-terminate the received message
            return 1;                 // ACK received
        }
    }
    return 0;    // ACK not received
}
