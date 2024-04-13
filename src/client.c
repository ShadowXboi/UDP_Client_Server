#include "../include/client.h"
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

#define BUFFER_SIZE 1024
#define ACK_TIMEOUT 2
#define BASE 10

typedef struct
{
    int  sequence_number;
    char data[BUFFER_SIZE];
} Packet;

typedef struct node
{
    Packet       packet;
    struct node *next;
} Node;

typedef struct
{
    Node *front, *rear;
} Queue;

// Declare static functions for internal queue management
static void enqueue(const Packet *packet);
static int  dequeue(Packet *packet);

// Define the queue in a completely hidden way
static Queue *get_queue(void)
{
    static Queue packet_queue = {NULL, NULL};
    return &packet_queue;
}

// Function prototypes
void send_packet_with_retry(int sockfd, struct sockaddr_in *server_addr, Packet *packet);
int  wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr, int expected_seq_num);

int run_client(const char *server_ip, int server_port)
{
    int                sockfd;
    struct sockaddr_in server_addr;
    Packet             packet;
    int                sequence_number = 0;
    int                windowHeight    = 24;                            // Define the height of the window
    int                windowWidth     = 80;                            // Define the width of the window
    int                start_y         = (LINES - windowHeight) / 2;    // Calculating y position to center the window
    int                start_x         = (COLS - windowWidth) / 2;      // Calculating x position to center the window

    initscr();               // Start ncurses mode
    cbreak();                // Line buffering disabled
    keypad(stdscr, TRUE);    // Enable special keys to be read as single values
    noecho();                // Don't echo keystrokes

    WINDOW *win = newwin(windowHeight, windowWidth, start_y, start_x);
    box(win, 0, 0);    // Draw a box around the edges of the window
    wrefresh(win);     // Refresh the window to show the box

    sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    //    initscr();
    //    cbreak();
    //    noecho();
    //    keypad(stdscr, TRUE);
    //    nodelay(stdscr, TRUE);

    printw("Press arrow keys to move the character. 'q' to quit.\n");
    refresh();

    while(1)
    {
        int    ch = getch();
        Packet temp_packet;
        if(ch != ERR)
        {
            bool send_command = true;
            memset(packet.data, 0, BUFFER_SIZE);
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
                case 'q':
                    endwin();
                    close(sockfd);
                    return 0;
                default:
                    send_command = false;
                    break;
            }

            if(send_command)
            {
                enqueue(&packet);
            }
        }

        if(dequeue(&temp_packet))
        {
            send_packet_with_retry(sockfd, &server_addr, &temp_packet);
        }
    }

    // return 0;
}

// Function definitions
static void enqueue(const Packet *packet)
{
    Node  *newNode = malloc(sizeof(Node));
    Queue *queue   = get_queue();    // Access the queue via getter
    if(!newNode)
    {
        perror("Failed to allocate node");
        exit(EXIT_FAILURE);
    }
    newNode->packet = *packet;    // Copy packet data from the pointer
    newNode->next   = NULL;
    if(!queue->rear)
    {    // Use 'queue' instead of 'packet_queue'
        queue->front = queue->rear = newNode;
    }
    else
    {
        queue->rear->next = newNode;
        queue->rear       = newNode;
    }
}

static int dequeue(Packet *packet)
{
    Node  *temp;
    Queue *queue = get_queue();    // Access the queue via getter
    if(!queue->front)
    {
        return 0;    // Queue is empty
    }
    temp         = queue->front;
    *packet      = temp->packet;
    queue->front = queue->front->next;
    if(!queue->front)
    {
        queue->rear = NULL;
    }
    free(temp);
    return 1;
}

void send_packet_with_retry(int sockfd, struct sockaddr_in *server_addr, Packet *packet)
{
    char ack_message[BUFFER_SIZE];
    int  retries = 3;
    for(int attempt = 0; attempt < retries; ++attempt)
    {
        sendto(sockfd, packet, sizeof(*packet), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));
        if(wait_for_ack(sockfd, ack_message, server_addr, packet->sequence_number))
        {
            printw("ACK received for Seq: %d\n", packet->sequence_number);
            refresh();
            return;
        }
        printw("ACK not received for Seq: %d, retrying...\n", packet->sequence_number);
        refresh();
    }
    printw("Failed to receive ACK for Seq: %d after %d attempts.\n", packet->sequence_number, retries);
    refresh();
}

int wait_for_ack(int sockfd, char *ack_message, struct sockaddr_in *server_addr, int expected_seq_num)
{
    struct timeval tv;
    fd_set         readfds;

    tv.tv_sec  = ACK_TIMEOUT;
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    if(select(sockfd + 1, &readfds, NULL, NULL, &tv) > 0)
    {
        char *endptr;

        socklen_t len = sizeof(*server_addr);
        ssize_t   n   = recvfrom(sockfd, ack_message, BUFFER_SIZE - 1, 0, (struct sockaddr *)server_addr, &len);
        if(n > 0)
        {
            ack_message[n] = '\0';
            if(strncmp(ack_message, "ACK ", 4) == 0)
            {
                long received_seqnum;
                received_seqnum = strtol(ack_message + 4, &endptr, BASE);
                if(*endptr == '\0')
                {    // Check if the conversion was successful and reached the end of the number
                    if(received_seqnum == expected_seq_num)
                    {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
