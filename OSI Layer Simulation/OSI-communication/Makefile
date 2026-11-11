CC = gcc
CFLAGS = -Wall -Wextra -std=c99

ifeq ($(OS),Windows_NT)
EXE = .exe
SOCKET_LIBS = -lws2_32
RM = del /Q
CFLAGS += -D_WIN32_WINNT=0x0600
else
EXE =
SOCKET_LIBS =
CLEAN = rm -f
CFLAGS += -D_POSIX_C_SOURCE=200112L
endif

all: osi_client$(EXE) osi_server$(EXE)

osi_client$(EXE): main.build.o layers.build.o
	$(CC) $(CFLAGS) $^ -o $@ $(SOCKET_LIBS)

osi_server$(EXE): server.build.o layers.build.o
	$(CC) $(CFLAGS) $^ -o $@ $(SOCKET_LIBS)

main.build.o: main.c layers.h
	$(CC) $(CFLAGS) -c main.c -o $@

server.build.o: server.c layers.h
	$(CC) $(CFLAGS) -c server.c -o $@

layers.build.o: layers.c layers.h
	$(CC) $(CFLAGS) -c layers.c -o $@

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) *.build.o *.d osi_client.exe osi_server.exe
else
	$(CLEAN) *.build.o *.d osi_client osi_server
endif

.PHONY: all clean
