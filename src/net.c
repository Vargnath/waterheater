/* WaterHeater - Simple water heating simulation written in C
 * Copyright (C) 2015 Sebastian Kaiser
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. */

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "net.h"

int socket_writeln(socket_t socket, char const format[static 1], ...) {
	va_list args;
	va_start(args, format);
	char* buffer = 0;
	if (vasprintf(&buffer, format, args) == -1) {
		return -1;
	}
	va_end(args);
	size_t length = strlen(buffer) + 2;
	char line[length];
	strcpy(line, buffer);
	strcat(line, "\n");
	int result = send(socket, line, length - 1, 0);
	free(buffer);
	// errno was set by write on failure
	return result;
}

// TODO implement dynamic approach
int socket_readln(socket_t socket, char buffer[static 1], size_t size) {
	// XXX check for null pointer?
	int num_bytes = 0;
	int total_bytes = 0;
	char ch;
	while (1) {
		num_bytes = recv(socket, &ch, sizeof(ch), 0);
		if (num_bytes == -1) {
			if (errno == EINTR) {
				continue;
			}
			// errno was set by recv
			return -1;
		} else if (num_bytes == 0) {
			if (total_bytes == 0) {
				return 0;
			}
			break;
		}
		if (total_bytes < size - 1) {
			++total_bytes;
			*buffer = ch;
			++buffer;
		}
		if (ch == '\n') {
			break;
		}
	}
	*buffer = '\0';
	printf("buffer");	// XXX Debug code
	return total_bytes;
}

// TODO handle IPv6
int socket_getpeer(socket_t socket, char buffer[static 1], size_t size) {
	struct sockaddr_in addr = {0};
	socklen_t length_ptr = sizeof(addr);
	if (getpeername(socket, (struct sockaddr*)&addr, &length_ptr) == -1) {
		// errno was set by getpeername
		return -1;
	}
	char addrstr[INET_ADDRSTRLEN] = {0};
	inet_ntop(AF_INET, &addr.sin_addr, addrstr, INET_ADDRSTRLEN);
	uint16_t port = ntohs(addr.sin_port);
	return snprintf(buffer, size, "%s:%" PRIu16 "", addrstr, port);
}

socket_t socket_init() {
	// errno will be set by socket on failure
	return socket(AF_INET, SOCK_STREAM, 0);
}

bool socket_bind(socket_t socket, uint16_t port) {
	if (port == 0) {
		errno = EINVAL;
		return false;
	}
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	// errno will be set by bind on failure
	return (bind(socket, (struct sockaddr*)&addr, sizeof(addr)) == 0);
}

bool socket_listen(socket_t socket, int queue_size) {
	// errno will be set by listen on failure
	return (listen(socket, queue_size) == 0);
}

int socket_accept(socket_t socket, struct sockaddr_in* addr, socklen_t* length_ptr) {
	// errno will be set by accept on failure
	return accept(socket, (struct sockaddr*)addr, length_ptr);
}

//#include "net.h"
//
///*
// * Stores the client's IP and port in the array s.
// * Format: IP:Port => 127.0.0.1:12345
// * Returns the number of characters stored in the array s, not including the terminating null character.
// */
//int clienttostr(char *s, const client_t *client) {
//	return sprintf(s, "%s:%d", inet_ntoa(client->s_addr.sin_addr), ntohs(client->s_addr.sin_port));
//}
//
///*
// * Writes the string buffer plus a newline character to the socket with descriptor sock.
// * Returns the number of characters stored in the array buffer.
// */
//int writeln(int sock, const char* buffer) {
//	unsigned len = strlen(buffer) + 2;
//	char line[len];
//	strcat(strcpy(line, buffer), "\n\0");
//	return write(sock, line, len);
//}
//
///*
// * Creates a socket for TCP/IPv4 and stores it's descriptor in sock.
// * Returns the socket descriptor, or -1 in case of error.
// */
//int init_socket(socket_t *sock) {
//	memset(sock, 0, sizeof(socket_t));
//	sock->s_len = sizeof(struct sockaddr_in);
//	return sock->s_desc = socket(AF_INET, SOCK_STREAM, 0);
//}
//
///*
// * The bind function assigns an address to the socket socket.
// * Returns 0 on success or -1 in case of error.
// */
//int bind_socket(socket_t *sock, unsigned port) {
//	sock->s_addr.sin_family = AF_INET;
//	sock->s_addr.sin_addr.s_addr = INADDR_ANY;
//	sock->s_addr.sin_port = htons(port);
//	return bind(sock->s_desc, (struct sockaddr*)&sock->s_addr, sizeof(struct sockaddr_in));
//}
//
///*
// * The listen function enables the socket socket to accept connections, thus making it a server socket.
// * Returns 0 on success or -1 in case of error.
// */
//int listen_socket(socket_t *sock, int n) {
//	return listen(sock->s_desc, n);
//}
//
///*
// * This function is used to accept a connection request on the server socket socket.
// * Returns 0 on success or -1 in case of error.
// */
//int accept_socket(socket_t *sock, client_t *client) {
//	return client->s_desc = accept(sock->s_desc, (struct sockaddr*)&client->s_addr, (socklen_t*)&sock->s_len);
//}
