/*
 * Important!
 * Add "pthread" and  "m" to "GCC C Linker" under "Libraries"!
 * Define symbol "_GNU_SOURCE"
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>	/* uint64_t */
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "sim.h"
#include "net.h"
#include "log.h"

#define NET_DEFAULT_PORT 4242
#define NET_MAX_PENDING 3
#define NET_BUFFER_SIZE 513 // 512 Bytes + '\0'
#define NET_DEFAULT_INTERVAL 500
#define SIM_DEFAULT_ENV_TEMP 20.0
#define SIM_DEFAULT_WATER_VOLUME 200000.0
#define SIM_DEFAULT_POWER 2000.0

/* logln macro for stderror */
#define logerror(format, ...) flogf(stderr, format, ##__VA_ARGS__)

/* logln macro for stdout, prints only if verbose_flag is set */
#define logline(format, ...) if (verbose_flag) flogf(stdout, format, ##__VA_ARGS__)

/*
 * Contains all data the client's threads needs.
 */
typedef struct {
	unsigned c_upd_int;
	socket_t c_client;
	sim_t *c_sim;
} conn_t;

/* Flag set by "--verbose". */
static int verbose_flag = 0;

/*
 * Extracts information from the simulation and stores a status message in buffer.
 * Returns the number of characters stored in buffer, not including the terminating null character.
 */
int statustostr(char* buffer, sim_t const* sim) {
	/* "%lu" needs to be changes to "%llu" when compiling for ARM! */
	sim_temp_t sim_temp;
	sim_gettemp(&sim_temp, sim);
	return sprintf(buffer, "status %lf %s %" PRIu64 "", sim_temp.sim_water_temp, ((sim_temp.sim_state == ON) ? "ON" : "OFF"), sim_temp.sim_timestamp);
}

/*
 * Prints the error message and exits the application with EXIT_FAILURE.
 */
void error(char *msg) {
	flogf(stderr, msg);
	exit(EXIT_FAILURE);
}

/*
 * Sends the current information in a predefined interval to the client.
 */
void* status_handler(void *conn) {
	sim_t *sim = ((conn_t*)conn)->c_sim;
	socket_t client_sock = ((conn_t*)conn)->c_client;
	unsigned interval = ((conn_t*)conn)->c_upd_int;
	char client_addr[22] = {0};
	socket_getpeer(client_sock, client_addr, 22);
	char buffer[NET_BUFFER_SIZE];
	while (1) {
		memset(buffer, 0, NET_BUFFER_SIZE);
		statustostr(buffer, sim);
		if (socket_writeln(client_sock, buffer) < 0) {
			logerror("[%s]: ERROR sending data", client_addr);
			break;
		}
		logline(">> [%s]: %s", client_addr, buffer);
		usleep(interval * 1000);
	}
	return 0;
}

/*
 * Starts the status handler for the client and takes care of incoming data.
 */
void* connection_handler(void *conn) {
	sim_t *sim = ((conn_t*)conn)->c_sim;
	socket_t client_sock = ((conn_t*)conn)->c_client;
	int read_size = 0;
	char client_addr[22] = {0};
	socket_getpeer(client_sock, client_addr, 22);
	char buffer[NET_BUFFER_SIZE];
	memset(buffer, 0, NET_BUFFER_SIZE);
	pthread_t thread_id;
	/* Start the status handler */
	if (pthread_create(&thread_id, NULL, status_handler, (void*)conn) < 0) {
		logerror("ERROR starting status handler");
		logline(">> [%s]: error", client_addr);
		socket_writeln(client_sock, "error");
		close(client_sock);
	}
	/* Read incoming data */
	while ((read_size = recv(client_sock, buffer, NET_BUFFER_SIZE - 1, 0)) > 0) {
		/*
		 * Because it's unsure if the line terminates with "\r\n" or "\n"
		 * we try to remove both by replacing with '\0'.
		 * Example: "control on\r\n" -> "control on"
		 */
		for (unsigned i = 0; buffer[i]; ++i)
			buffer[i] = (buffer[i] != '\n' && buffer[i] != '\r') ? tolower(buffer[i]) : '\0';
		/* Parse the message */
		if (strcmp(buffer, "control on") == 0) {
			/* Turn the heater on! */
			logline("<< [%s]: control on", client_addr);
			sim_control(sim, ON);
			statustostr(buffer, sim);
			socket_writeln(client_sock, buffer);
			logline(">> [%s]: %s", client_addr, buffer);
		} else if (strcmp(buffer, "control off") == 0) {
			/* Turn the heater off! */
			logline("<< [%s]: control off", client_addr);
			sim_control(sim, OFF);
			statustostr(buffer, sim);
			socket_writeln(client_sock, buffer);
			logline(">> [%s]: %s", client_addr, buffer);
		} else if (strcmp(buffer, "request") == 0) {
			/* Send your status! */
			logline("<< [%s]: request", client_addr);
			statustostr(buffer, sim);
			socket_writeln(client_sock, buffer);
			logline(">> [%s]: %s", client_addr, buffer);
		} else {
			/* Unknown command! */
			logline("<< [%s]: %s", client_addr, buffer);
			logerror("[%s]: ERROR received unknown command: %s", client_addr, buffer);
			socket_writeln(client_sock, "error");
			logline(">> [%s]: error", client_addr);
		}
		/* Clean up */
		memset(buffer, 0, NET_BUFFER_SIZE - 1);
	}
	close(client_sock);
	if (read_size == 0) {
		logline("[%s] has disconnected!", client_addr);
	} else {
		logerror("[%s]: ERROR receiving data", client_addr);
	}
	pthread_join(thread_id, 0);	/* Wait for the status handler */
	return 0;
}

int main(int argc, char **argv) {
	int c;
	unsigned net_port = NET_DEFAULT_PORT;
	int upd_interval = NET_DEFAULT_INTERVAL;
	double sim_env_temp = SIM_DEFAULT_ENV_TEMP;
	double sim_water_vol = SIM_DEFAULT_WATER_VOLUME;
	double sim_power = SIM_DEFAULT_POWER;
	/* Check the passed arguments and set the respective variables */
	while (1) {
		static struct option long_options[] = {
			{ "verbose",		no_argument,		0,	'v' },	/* explain what is done */
			{ "help",			no_argument,		0,	'?' },	/* display the help */
			{ "interval",		required_argument,	0,	'i' },	/* status update interval */
			{ "port",			required_argument,	0,	'p' },	/* network port */
			{ "Power",			required_argument,	0,	'P' },	/* heating power */
			{ "Temperature",	required_argument,	0,	'T' },	/* environmental temperature */
			{ "Volume",			required_argument,	0,	'V' },	/* amount of water */
			{ 0, 0, 0, 0 }
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "v?i:p:P:T:V:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'v':	/* verbose */
			verbose_flag = 1;
		break;
		case 'P':	/* Power */
			sim_power = strtod(optarg, NULL);
		break;
		case 'T':	/* Temperature */
			sim_env_temp = strtod(optarg, NULL);
		break;
		case 'V':	/* Volume */
			sim_water_vol = strtod(optarg, NULL) * 1000;
		break;
		case '?':	/* help */
			printf("Usage: %s [options]\n", basename(argv[0]));
			printf("  -i, --interval=UNSIGNED\tstatus message interval in ms\n\t\t\t\tdefault: 500\n");
			printf("  -p, --port=UNSIGNED\t\tport for incoming connections\n\t\t\t\tdefault: 4242\n");
			printf("  -v, --verbose\t\t\texplain what is being done\n");
			printf("  -P, --Power=DOUBLE\t\theating power in Watt\n\t\t\t\tdefault: 2000\n");
			printf("  -T, --Temperature=DOUBLE\tenvironmental temperature in °C\n\t\t\t\tdefault: 20\n");
			printf("  -V, --Volume=DOUBLE\t\tamount of water in l\n\t\t\t\tdefault: 200\n");
			printf("\n  -?, --help\t\t\tdisplay this text and exit\n");
			exit(EXIT_FAILURE);
		break;
		case 'i':	/* interval */
			upd_interval = atoi(optarg);
		break;
		case 'p':	/* port */
			net_port = atoi(optarg);
		break;
		default:	/* something went wrong */
			/* Error was already printed */
			abort();
		}
	}
	/* Print start information */
	logline("*******************************************");
	logline("* Water Heating Simulation v1.0\t\t*");
	logline("*******************************************");
	logline("Network Port: %i", net_port);
	logline("Status Update Interval: %i ms", upd_interval);
	logline("Simulation Power: %.1lf W", sim_power);
	logline("Simulation Env. Temperature: %.1lf °C", sim_env_temp);
	logline("Simulation Water Volume: %.1lf l", (sim_water_vol / 1000));
	logline("*******************************************");
	/* Initialize the simulation */
	sim_t* sim = sim_create(sim_water_vol, sim_power, sim_env_temp, sim_env_temp);
	/* Initialize the network socket */
	socket_t sock;
	if ((sock = socket_init()) < 0) {
		error("ERROR opening socket");
	}
	if (!socket_bind(sock, net_port)) {
		error("ERROR binding socket");
	}
	if (!socket_listen(sock, NET_MAX_PENDING)) {
		error("ERROR listening socket");
	}
	logline("Waiting for incoming connections on port %i", net_port);
	struct sockaddr_in client = {0};
	socklen_t client_len = 0;
	pthread_t thread_id;
	socket_t csock = 0;
	/* Start the simulation */
	if (!sim_start(sim)) {
		error("ERROR starting sim");
	}
	/* Wait for incoming connections */
	while ((csock = socket_accept(sock, &client, &client_len)) != -1) {
		conn_t conn;
		memset(&conn, 0, sizeof(conn_t));
		conn.c_client = csock;
		conn.c_sim = sim;
		conn.c_upd_int = upd_interval;
		char buffer[22] = {0};
		socket_getpeer(csock, buffer, 22);
		logline("[%s] has connected!", buffer);
		/* Start the connection handler */
		if (pthread_create(&thread_id, NULL, connection_handler, (void*)&conn) < 0)
			error("ERROR creating connection handler");
	}
	if (csock < 0)	/* Socket is dead */
		error("ERROR accepting client_addr");
	sim_free(sim);
	logline("Simulation terminated!\n");	/* It's dead! */
	return EXIT_SUCCESS;
}
