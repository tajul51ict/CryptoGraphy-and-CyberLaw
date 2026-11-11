#include <stdio.h>
#include <string.h>

#include "layers.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#endif

static const char *const layer_names[OSI_LAYER_COUNT + 1] = {
    "", "Physical", "Data Link", "Network", "Transport",
    "Session", "Presentation", "Application"
};

static void write_u32(unsigned char *destination, uint32_t value) {
    destination[0] = (unsigned char)(value >> 24);
    destination[1] = (unsigned char)(value >> 16);
    destination[2] = (unsigned char)(value >> 8);
    destination[3] = (unsigned char)value;
}

static uint32_t read_u32(const unsigned char *source) {
    return ((uint32_t)source[0] << 24) |
           ((uint32_t)source[1] << 16) |
           ((uint32_t)source[2] << 8) |
           (uint32_t)source[3];
}

int osi_socket_startup(void) {
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data);
#else
    return 0;
#endif
}

void osi_socket_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

void osi_socket_close(osi_socket_t socket_fd) {
#ifdef _WIN32
    if (socket_fd != INVALID_SOCKET) {
        closesocket(socket_fd);
    }
#else
    if (socket_fd >= 0) {
        close(socket_fd);
    }
#endif
}

int osi_encapsulate(const unsigned char *message, size_t message_length,
                    unsigned char *frame, size_t *frame_length) {
    size_t length = message_length;

    if (message_length > OSI_MAX_MESSAGE ||
        (message_length > 0 && message == NULL) ||
        frame == NULL || frame_length == NULL) {
        return -1;
    }

    if (message_length > 0) {
        memcpy(frame, message, message_length);
    }

    for (unsigned int layer = OSI_LAYER_COUNT; layer > 0; --layer) {
        memmove(frame + OSI_HEADER_SIZE, frame, length);
        memcpy(frame, "OSI1", 4);
        frame[4] = (unsigned char)layer;
        write_u32(frame + 5, (uint32_t)length);
        length += OSI_HEADER_SIZE;
        printf("OSI_EVENT|TX|%u|%s|%u\n", layer, layer_names[layer],
               (unsigned int)(length - OSI_HEADER_SIZE));
    }

    *frame_length = length;
    return 0;
}

int osi_decapsulate(unsigned char *frame, size_t frame_length,
                    unsigned char *message, size_t *message_length) {
    size_t length = frame_length;

    if (frame == NULL || message == NULL || message_length == NULL) {
        return -1;
    }

    for (unsigned int layer = 1; layer <= OSI_LAYER_COUNT; ++layer) {
        uint32_t payload_length;

        if (length < OSI_HEADER_SIZE ||
            memcmp(frame, "OSI1", 4) != 0 ||
            frame[4] != (unsigned char)layer) {
            return -1;
        }

        payload_length = read_u32(frame + 5);
        if ((size_t)payload_length != length - OSI_HEADER_SIZE) {
            return -1;
        }

        printf("OSI_EVENT|RX|%u|%s|%u\n", layer, layer_names[layer],
               payload_length);
        memmove(frame, frame + OSI_HEADER_SIZE, payload_length);
        length = payload_length;
    }

    if (length > OSI_MAX_MESSAGE) {
        return -1;
    }
    if (length > 0) {
        memcpy(message, frame, length);
    }
    *message_length = length;
    return 0;
}

static int send_all(osi_socket_t socket_fd, const unsigned char *data,
                    size_t length) {
    size_t sent = 0;
    while (sent < length) {
        int result;
#ifdef _WIN32
        result = send(socket_fd, (const char *)data + sent,
                      (int)(length - sent), 0);
#else
#ifdef MSG_NOSIGNAL
        result = (int)send(socket_fd, data + sent, length - sent, MSG_NOSIGNAL);
#else
        result = (int)send(socket_fd, data + sent, length - sent, 0);
#endif
#endif
        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }
    return 0;
}

static int receive_all(osi_socket_t socket_fd, unsigned char *data,
                       size_t length) {
    size_t received = 0;
    while (received < length) {
        int result;
#ifdef _WIN32
        result = recv(socket_fd, (char *)data + received,
                      (int)(length - received), 0);
#else
        result = (int)recv(socket_fd, data + received, length - received, 0);
#endif
        if (result <= 0) {
            return -1;
        }
        received += (size_t)result;
    }
    return 0;
}

int osi_send_frame(osi_socket_t socket_fd, const unsigned char *frame,
                   size_t frame_length) {
    unsigned char length_prefix[4];

    if (frame == NULL || frame_length == 0 ||
        frame_length > OSI_MAX_FRAME_SIZE) {
        return -1;
    }
    write_u32(length_prefix, (uint32_t)frame_length);
    if (send_all(socket_fd, length_prefix, sizeof(length_prefix)) != 0 ||
        send_all(socket_fd, frame, frame_length) != 0) {
        return -1;
    }
    return 0;
}

int osi_receive_frame(osi_socket_t socket_fd, unsigned char *frame,
                      size_t *frame_length) {
    unsigned char length_prefix[4];
    uint32_t length;

    if (frame == NULL || frame_length == NULL ||
        receive_all(socket_fd, length_prefix, sizeof(length_prefix)) != 0) {
        return -1;
    }
    length = read_u32(length_prefix);
    if (length == 0 || length > OSI_MAX_FRAME_SIZE ||
        receive_all(socket_fd, frame, length) != 0) {
        return -1;
    }
    *frame_length = length;
    return 0;
}
