#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#ifndef SOCK_CLOEXEC
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-macros"
    #define SOCK_CLOEXEC 0
    #pragma GCC diagnostic pop
#endif

#define BASE 10
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define DEFAULT_PORT 65535
#define MICRO_SECONDS 100000000L

// a simple packet structure (for illustration)
typedef struct
{
    int  sequence_number;
    char data[BUFFER_SIZE];
} Packet;

typedef struct
{
    struct sockaddr_in addr;
    socklen_t          addr_len;
    int                valid;    // 1 if the entry is used, 0 otherwise
} Client;

Client *get_clients(void)
{
    static Client clients[MAX_CLIENTS] = {0};
    return clients;
}

// Function prototype
static void signal_handler(int signal_number);
static void sigchld_handler(int sig);
void        add_client(struct sockaddr_in client_addr, socklen_t addr_len);
void        broadcast_to_clients(int sockfd, const char *message, size_t message_len);

int main(int argc, const char *argv[])
{
    char              *end;
    long               psort;
    int                sockfd;
    Packet             packet;
    struct sigaction   sa;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr     = {0};
    socklen_t          client_addr_len = sizeof(client_addr);

    // Initialized client list
    //memset(clients, 0, sizeof(clients));

    printf("To send a signal:\n");
    printf("\tCtrl+C: Sends the SIGINT signal to the process.\n");
    printf("\tCtrl+Z: Sends the SIGTSTP signal to the process.\n");
    printf("\tCtrl+\\: Sends the SIGQUIT signal to the process.\n");

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;               // or sa.sa_sigaction = ... for advanced handling
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;    // plus any other flags you need

    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("Error setting signal handler for SIGINT");
        exit(EXIT_FAILURE);
    }

    if(sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("Error setting signal handler for SIGCHLD");
        exit(EXIT_FAILURE);
    }

    // Repeat for other signals you're handling, e.g., SIGCHLD
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;

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
    // memset(&server_addr, 0, sizeof(server_addr));
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
        ssize_t recv_len;
        recv_len = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if(recv_len == -1)
        {
            perror("recvfrom failed");
            continue;    // Continue listening for packets
        }

        if(recv_len > 0)
        {
            // add client if not already added
            add_client(client_addr, client_addr_len);

            char ack_message[BUFFER_SIZE];
            int  ack_len;
            printf("Received: %s | Seq: %d\n", packet.data, packet.sequence_number);

            ack_len = snprintf(ack_message, BUFFER_SIZE, "ACK for Seq: %d", packet.sequence_number);

            if(ack_len > 0 && ack_len < BUFFER_SIZE)
            {
                ssize_t bytes_sent     = 0;
                int     retries        = 3;    // Maximum number of retries
                size_t  length_to_send = (size_t)ack_len;

                struct timespec req;
                struct timespec rem;
                req.tv_sec  = 0;                // seconds
                req.tv_nsec = MICRO_SECONDS;    // 100 milliseconds in nanoseconds

                for(int attempt = 0; attempt < retries; attempt++)
                {
                    bytes_sent = sendto(sockfd, ack_message, length_to_send, 0, (struct sockaddr *)&client_addr, client_addr_len);
                    if(bytes_sent != -1)
                    {
                        break;    // Successfully sent, exit retry loop
                    }

                    if(attempt < retries - 1)
                    {
                        fprintf(stderr, "Retry %d for ACK Seq: %d\n", attempt + 1, packet.sequence_number);
                        if(nanosleep(&req, &rem) == -1)
                        {
                            perror("nanosleep interrupted");
                        }
                    }
                }

                if(bytes_sent == -1)
                {
                    perror("Final sendto failed");
                    fprintf(stderr, "Failed to send ACK for Seq: %d after retries. Continuing...\n", packet.sequence_number);
                }
            }
            else
            {
                char fallback_msg[] = "ACK Error";
                fprintf(stderr, "Error preparing ACK message for Seq: %d.\n", packet.sequence_number);

                if(sendto(sockfd, fallback_msg, sizeof(fallback_msg), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1)
                {
                    perror("sendto failed with fallback message");
                }
            }

            // Broadcast the received packet to all clients
            broadcast_to_clients(sockfd, packet.data, strlen(packet.data));
        }
    }

    // close(sockfd);
    // return 0;
}

// Add a new client to the list
void add_client(struct sockaddr_in client_addr, socklen_t addr_len)
{
    Client *clients = get_clients(); // Get the clients array
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(!clients[i].valid)
        {
            clients[i].addr     = client_addr;
            clients[i].addr_len = addr_len;
            clients[i].valid    = 1;
            return;
        }
    }
}

// Broadcast a message to all clients
void broadcast_to_clients(int sockfd, const char *message, size_t message_len)
{
    Client *clients = get_clients(); // Get the clients array
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i].valid)
        {
            sendto(sockfd, message, message_len, 0, (struct sockaddr *)&(clients[i].addr), clients[i].addr_len);
        }
    }
}

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
