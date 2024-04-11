#include <stdio.h>
#include <string.h>
#include "client.h"
#include "server.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [server|client]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        return run_server(argc, argv);
    } else if (strcmp(argv[1], "client") == 0) {
        return run_client(argc, argv);
    } else {
        printf("Invalid argument: %s\n", argv[1]);
        printf("Usage: %s [server|client]\n", argv[0]);
        return 1;
    }
}
