#ifndef WH_NET_H
#define WH_SIM_H 1

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

typedef struct {
	int s_desc;
	int s_len;
	struct sockaddr_in s_addr;
} socket_t;

typedef struct {
	int s_desc;
	struct sockaddr_in s_addr;
} client_t;

int clienttostr(char *s, const client_t *client);

int writeln(int sock, const char* buffer);

int init_socket(socket_t *sock);

int bind_socket(socket_t *sock, unsigned port);

int listen_socket(socket_t *sock, int n);

int accept_socket(socket_t *sock, client_t *client);

#endif	// WH_NET_H
