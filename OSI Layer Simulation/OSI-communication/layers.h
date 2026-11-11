#ifndef LAYERS_H
#define LAYERS_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET osi_socket_t;
#define OSI_INVALID_SOCKET INVALID_SOCKET
#else
#include <netdb.h>
#include <sys/socket.h>
typedef int osi_socket_t;
#define OSI_INVALID_SOCKET (-1)
#endif

#define OSI_LAYER_COUNT 7u
#define OSI_MAX_MESSAGE 4096u
#define OSI_HEADER_SIZE 9u
#define OSI_MAX_FRAME_SIZE (OSI_MAX_MESSAGE + OSI_LAYER_COUNT * OSI_HEADER_SIZE)

int osi_socket_startup(void);
void osi_socket_cleanup(void);
void osi_socket_close(osi_socket_t socket_fd);

int osi_encapsulate(const unsigned char *message, size_t message_length,
                    unsigned char *frame, size_t *frame_length);
int osi_decapsulate(unsigned char *frame, size_t frame_length,
                    unsigned char *message, size_t *message_length);
int osi_send_frame(osi_socket_t socket_fd, const unsigned char *frame,
                   size_t frame_length);
int osi_receive_frame(osi_socket_t socket_fd, unsigned char *frame,
                      size_t *frame_length);

#endif
