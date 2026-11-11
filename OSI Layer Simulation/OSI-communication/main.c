#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "layers.h"

static int parse_port(const char *text, char **end) {
    unsigned long port;
    errno = 0;
    port = strtoul(text, end, 10);
    if (errno != 0 || **end != '\0' || port == 0 || port > 65535) {
        return -1;
    }
    return (int)port;
}

int main(int argc, char **argv) {
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    osi_socket_t connection = OSI_INVALID_SOCKET;
    unsigned char frame[OSI_MAX_FRAME_SIZE];
    unsigned char response[OSI_MAX_MESSAGE];
    size_t frame_length;
    size_t response_length;
    char *end;
    int port;
    int result = 1;

#ifndef _WIN32
    connection = -1;
#endif

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <port> <message>\n", argv[0]);
        return 2;
    }
    port = parse_port(argv[2], &end);
    if (port < 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 2;
    }
    if (strlen(argv[3]) > OSI_MAX_MESSAGE) {
        fprintf(stderr, "Message exceeds the %u-byte limit.\n",
                OSI_MAX_MESSAGE);
        return 2;
    }
    if (osi_socket_startup() != 0) {
        fprintf(stderr, "Socket initialization failed.\n");
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(argv[1], argv[2], &hints, &addresses) != 0) {
        fprintf(stderr, "Could not resolve server address: %s\n", argv[1]);
        goto cleanup;
    }

    for (address = addresses; address != NULL; address = address->ai_next) {
        connection = socket(address->ai_family, address->ai_socktype,
                            address->ai_protocol);
#ifdef _WIN32
        if (connection == INVALID_SOCKET) {
#else
        if (connection < 0) {
#endif
            continue;
        }
        if (connect(connection, address->ai_addr,
                    (int)address->ai_addrlen) == 0) {
            break;
        }
        osi_socket_close(connection);
#ifdef _WIN32
        connection = INVALID_SOCKET;
#else
        connection = -1;
#endif
    }
    if (address == NULL) {
        fprintf(stderr, "Could not connect to %s:%d.\n", argv[1], port);
        goto cleanup;
    }

    if (osi_encapsulate((const unsigned char *)argv[3], strlen(argv[3]),
                        frame, &frame_length) != 0 ||
        osi_send_frame(connection, frame, frame_length) != 0) {
        fprintf(stderr, "Failed to send the encapsulated message.\n");
        goto cleanup;
    }

    if (osi_receive_frame(connection, frame, &frame_length) != 0 ||
        osi_decapsulate(frame, frame_length, response, &response_length) != 0) {
        fprintf(stderr, "Failed to receive or decode the server response.\n");
        goto cleanup;
    }
    if (response_length != strlen("Message delivered") ||
        memcmp(response, "Message delivered", response_length) != 0) {
        fprintf(stderr, "Received an unexpected server response.\n");
        goto cleanup;
    }

    printf("ACK|Message delivered\n");
    result = 0;

cleanup:
    if (addresses != NULL) {
        freeaddrinfo(addresses);
    }
    osi_socket_close(connection);
    osi_socket_cleanup();
    return result;
}
