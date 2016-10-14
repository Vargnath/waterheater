#include "net.h"

/*
 * Stores the client's IP and port in the array s.
 * Format: IP:Port => 127.0.0.1:12345
 * Returns the number of characters stored in the array s, not including the terminating null character.
 */
int clienttostr(char *s, const client_t *client) {
	return sprintf(s, "%s:%d", inet_ntoa(client->s_addr.sin_addr), ntohs(client->s_addr.sin_port));
}

/*
 * Writes the string buffer plus a newline character to the socket with descriptor sock.
 * Returns the number of characters stored in the array buffer.
 */
int writeln(int sock, const char* buffer) {
	unsigned len = strlen(buffer) + 2;
	char line[len];
	strcat(strcpy(line, buffer), "\n\0");
	return write(sock, line, len);
}

/*
 * Creates a socket for TCP/IPv4 and stores it's descriptor in sock.
 * Returns the socket descriptor, or -1 in case of error.
 */
int init_socket(socket_t *sock) {
	memset(sock, 0, sizeof(socket_t));
	sock->s_len = sizeof(struct sockaddr_in);
	return sock->s_desc = socket(AF_INET, SOCK_STREAM, 0);
}

/*
 * The bind function assigns an address to the socket socket.
 * Returns 0 on success or -1 in case of error.
 */
int bind_socket(socket_t *sock, unsigned port) {
	sock->s_addr.sin_family = AF_INET;
	sock->s_addr.sin_addr.s_addr = INADDR_ANY;
	sock->s_addr.sin_port = htons(port);
	return bind(sock->s_desc, (struct sockaddr*)&sock->s_addr, sizeof(struct sockaddr_in));
}

/*
 * The listen function enables the socket socket to accept connections, thus making it a server socket.
 * Returns 0 on success or -1 in case of error.
 */
int listen_socket(socket_t *sock, int n) {
	return listen(sock->s_desc, n);
}

/*
 * This function is used to accept a connection request on the server socket socket.
 * Returns 0 on success or -1 in case of error.
 */
int accept_socket(socket_t *sock, client_t *client) {
	return client->s_desc = accept(sock->s_desc, (struct sockaddr*)&client->s_addr, (socklen_t*)&sock->s_len);
}
