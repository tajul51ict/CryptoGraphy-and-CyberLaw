#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "layers.h"

static int parse_port(const char *text, unsigned short *port) {
    char *end;
    unsigned long value;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || *text == '\0' || *end != '\0' || value > 65535) {
        return -1;
    }
    *port = (unsigned short)value;
    return 0;
}

int main(int argc, char **argv) {
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    osi_socket_t listener = OSI_INVALID_SOCKET;
    osi_socket_t connection = OSI_INVALID_SOCKET;
    unsigned char frame[OSI_MAX_FRAME_SIZE];
    unsigned char message[OSI_MAX_MESSAGE];
    static const unsigned char response[] = "Message delivered";
    size_t frame_length;
    size_t message_length;
    unsigned short port;
    char actual_port[6];
    struct sockaddr_storage local_address;
    socklen_t local_address_length = sizeof(local_address);
    int result = 1;

#ifndef _WIN32
    listener = -1;
    connection = -1;
#endif

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <bind-host> <port>\n", argv[0]);
        return 2;
    }
    if (parse_port(argv[2], &port) != 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 2;
    }
    if (osi_socket_startup() != 0) {
        fprintf(stderr, "Socket initialization failed.\n");
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(argv[1], argv[2], &hints, &addresses) != 0) {
        fprintf(stderr, "Could not resolve bind address: %s\n", argv[1]);
        goto cleanup;
    }

    for (address = addresses; address != NULL; address = address->ai_next) {
        int reuse_address = 1;
        listener = socket(address->ai_family, address->ai_socktype,
                          address->ai_protocol);
#ifdef _WIN32
        if (listener == INVALID_SOCKET) {
#else
        if (listener < 0) {
#endif
            continue;
        }
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   (const char *)&reuse_address, sizeof(reuse_address));
        if (bind(listener, address->ai_addr,
                 (int)address->ai_addrlen) == 0 &&
            listen(listener, 1) == 0) {
            break;
        }
        osi_socket_close(listener);
#ifdef _WIN32
        listener = INVALID_SOCKET;
#else
        listener = -1;
#endif
    }
    if (address == NULL) {
        fprintf(stderr, "Could not bind to %s:%u.\n", argv[1], port);
        goto cleanup;
    }

    if (getsockname(listener, (struct sockaddr *)&local_address,
                    &local_address_length) != 0) {
        fprintf(stderr, "Could not determine the listening port.\n");
        goto cleanup;
    }
    if (local_address.ss_family == AF_INET) {
        snprintf(actual_port, sizeof(actual_port), "%u",
                 (unsigned int)ntohs(
                     ((struct sockaddr_in *)&local_address)->sin_port));
    } else {
        snprintf(actual_port, sizeof(actual_port), "%u",
                 (unsigned int)ntohs(
                     ((struct sockaddr_in6 *)&local_address)->sin6_port));
    }
    printf("READY|%s|%s\n", argv[1], actual_port);
    fflush(stdout);

    connection = accept(listener, NULL, NULL);
#ifdef _WIN32
    if (connection == INVALID_SOCKET) {
#else
    if (connection < 0) {
#endif
        fprintf(stderr, "Failed to accept a client connection.\n");
        goto cleanup;
    }

    if (osi_receive_frame(connection, frame, &frame_length) != 0 ||
        osi_decapsulate(frame, frame_length, message, &message_length) != 0) {
        fprintf(stderr, "Failed to receive or decode the client message.\n");
        goto cleanup;
    }
    printf("MESSAGE_RECEIVED|%u bytes\n", (unsigned int)message_length);

    if (osi_encapsulate(response, sizeof(response) - 1, frame,
                        &frame_length) != 0 ||
        osi_send_frame(connection, frame, frame_length) != 0) {
        fprintf(stderr, "Failed to send the server response.\n");
        goto cleanup;
    }
    result = 0;

cleanup:
    osi_socket_close(connection);
    osi_socket_close(listener);
    if (addresses != NULL) {
        freeaddrinfo(addresses);
    }
    osi_socket_cleanup();
    return result;
}
