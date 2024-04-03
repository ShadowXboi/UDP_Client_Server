#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
// #include <unistd.h>

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

// Function prototype
static void signal_handler(int signal_number);

static void sigchld_handler(int sig);

int main(int argc, const char *argv[]) {
    char *end;
    long psort;
    int sockfd;
    Packet packet;
    struct sigaction sa;
    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    printf("To send a signal:\n");
    printf("\tCtrl+C: Sends the SIGINT signal to the process.\n");
    printf("\tCtrl+Z: Sends the SIGTSTP signal to the process.\n");
    printf("\tCtrl+\\: Sends the SIGQUIT signal to the process.\n");

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;               // or sa.sa_sigaction = ... for advanced handling
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;    // plus any other flags you need

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting signal handler for SIGINT");
        exit(EXIT_FAILURE);
    }

    // Repeat for other signals you're handling, e.g., SIGCHLD
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error setting signal handler for SIGCHLD");
        exit(EXIT_FAILURE);
    }
    // Validate command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Validate the port number

    psort = strtol(argv[1], &end, BASE);
    if (*end != '\0' || psort <= 0 || psort > DEFAULT_PORT) {
        fprintf(stderr, "Invalid port number. Please specify a port number between 1 and 65535.\n");
        exit(EXIT_FAILURE);
    }

    // Creating a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    // memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t) psort);

    // Bind the socket to the server address
    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %s...\n", argv[1]);

    // change the current working directory
    //    if(chdir("../src") == -1)
    //    {
    //        perror(" changing directory failed ");
    //        exit(EXIT_FAILURE);
    //    }

    while (1) {
        ssize_t recv_len = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &client_addr,
                                    &client_addr_len);
        if (recv_len == -1) {
            perror("recvfrom failed");
            continue;    // Continue listening for packets
        }

        if (recv_len > 0) {
            char ack_message[BUFFER_SIZE];
            printf("Received: %s | Seq: %d\n", packet.data, packet.sequence_number);

            // Send acknowledgment back to the client

            snprintf(ack_message, sizeof(ack_message), "ACK for Seq: %d", packet.sequence_number);
            if (sendto(sockfd, ack_message, strlen(ack_message), 0, (struct sockaddr *) &client_addr,
                       client_addr_len) == -1) {
                perror("sendto failed");
                continue;
            }
        } else {
            // the packet was received but with no data.
            printf("Packet received with no data.\n");
        }
    }
}

// Cleanup
    close(sockfd);    // This line is technically unnecessary due to the exit call in signal_handler
    return 0;         // This line is unreachable without changes to the loop


static void sigchld_handler(int sig)
{
    (void)sig;    // Mark the parameter as unused.
    // Use waitpid to clean up the child processes.
    while(waitpid(-1, NULL, WNOHANG) > 0)
    {
        // No body needed; loop just reaps zombies.
    }
}

// Signal handler function
static void signal_handler(int signal_number)
{
    switch(signal_number)
    {
        case SIGINT:
            printf("\nReceived SIGINT, server shutting down...\n");
            // Add cleanup and shutdown code here
            exit(EXIT_SUCCESS);
        case SIGTSTP:
            // Handle SIGTSTP if needed
            printf("\nReceived SIGTSTP, action not implemented.\n");
            break;
        case SIGQUIT:
            printf("\nReceived SIGQUIT, server shutting down...\n");
            // Add cleanup and shutdown code here
            exit(EXIT_SUCCESS);
        default:
            printf("Unhandled signal: %d\n", signal_number);
    }
}
