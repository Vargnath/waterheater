//#ifndef WH_NET_H
//#define WH_SIM_H 1
//
//#include <stdio.h>
//#include <string.h>
//#include <unistd.h>
//#include <sys/socket.h>
//#include <arpa/inet.h>
//
//typedef struct {
//	int s_desc;
//	int s_len;
//	struct sockaddr_in s_addr;
//} socket_t;
//
//typedef struct {
//	int s_desc;
//	struct sockaddr_in s_addr;
//} client_t;
//
//int clienttostr(char *s, const client_t *client);
//
//int writeln(int sock, const char* buffer);
//
//int init_socket(socket_t *sock);
//
//int bind_socket(socket_t *sock, unsigned port);
//
//int listen_socket(socket_t *sock, int n);
//
//int accept_socket(socket_t *sock, client_t *client);
//
//#endif	// WH_NET_H

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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

typedef int socket_t;

int socket_writeln(socket_t socket, char const format[static 1], ...);

int socket_readln(socket_t socket, char buffer[static 1], size_t size);

int socket_getpeer(socket_t socket, char buffer[static 1], size_t size);

socket_t socket_init();

bool socket_bind(socket_t socket, uint16_t port);

bool socket_listen(socket_t socket, int queue_size);

int socket_accept(socket_t socket, struct sockaddr_in* addr, socklen_t* length_ptr);
