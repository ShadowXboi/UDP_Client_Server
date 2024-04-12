#include "../include/client.h"
#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE 10
#define PORT 65535

int main(int argc, const char *argv[])
{
    const char *server_ip;
    long        server_port;
    char       *endptr;
    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s server|client [options]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if(strcmp(argv[1], "server") == 0)
    {
        const char **server_argv;
        if(argc < 3)
        {
            fprintf(stderr, "Insufficient arguments for server\n");
            exit(EXIT_FAILURE);
        }
        // Create a new array with const qualifiers to meet the expected signature
        server_argv = (const char **)malloc((size_t)(argc - 1) * sizeof(char *));
        if(!server_argv)
        {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        for(int i = 1; i < argc; i++)
        {
            server_argv[i - 1] = argv[i];
        }
        run_server(argc - 1, server_argv);
        free(server_argv);    // Don't forget to free the allocated memory
    }
    else if(strcmp(argv[1], "client") == 0)
    {
        if(argc != 4)
        {
            fprintf(stderr, "Usage: %s client <server_ip> <server_port>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        server_ip   = argv[2];
        server_port = strtol(argv[3], &endptr, BASE);

        if(*endptr != '\0')
        {
            fprintf(stderr, "Invalid number: %s\n", argv[3]);
            exit(EXIT_FAILURE);
        }

        if(server_port < 1 || server_port > PORT)
        {
            fprintf(stderr, "Port number out of valid range (1-65535): %ld\n", server_port);
            exit(EXIT_FAILURE);
        }

        run_client(server_ip, (int)server_port);
    }
    else
    {
        fprintf(stderr, "Invalid mode '%s'. Please choose 'server' or 'client'.\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
