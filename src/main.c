/*
 * Important!
 * Add "pthread" and  "m" to "GCC C Linker" under "Libraries"!
 */
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
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "sim.h"
#include "net.h"

#define NET_DEFAULT_PORT 4242
#define NET_MAX_PENDING 3
#define NET_BUFFER_SIZE 513 // 512 Bytes + '\0'
#define NET_DEFAULT_INTERVAL 500
#define SIM_DEFAULT_ENV_TEMP 20.0
#define SIM_DEFAULT_WATER_VOLUME 200000.0
#define SIM_DEFAULT_POWER 2000.0

/* logln macro for stderror */
#define logerror(format, ...) do { \
	logln(stderr, format, ##__VA_ARGS__); \
} while (0)

/* logln macro for stdout, prints only if verbose_flag is set */
#define logline(format, ...) do { \
	if (verbose_flag) \
		logln(stdout, format, ##__VA_ARGS__); \
} while (0)

/*
 * Contains all data the client's threads needs.
 */
typedef struct {
	unsigned c_upd_int;
	client_t c_client;
	sim_t *c_sim;
} conn_t;

/* Flag set by "--verbose". */
static int verbose_flag = 0;

/*
 * Writes the string to the filestream and prefixes it with a timestamp.
 * If the string is written to stderr the corresponding error string will be appended in case errno is set.
 * Timestamp Format: 12:34:56.789
 * The string will be appended with a newline character.
 *
 * The function is thread-safe!
 * Returns 0 on success or -1 in case of error.
 */
static int logln(FILE *stream, const char *format, ...) {
	int err = errno;
	/*
	 * This function will be used by multiple thread simultaneously.
	 * A mutex is used to make it thread-safe.
	 */
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&lock);	/* Lock mutex */
	/* Get the time */
	struct timespec tspec;
	if (clock_gettime(CLOCK_REALTIME, &tspec) < 0) {
		perror("ERROR could not get time");
		return -1;
	}
	/* Write the time */
	uint32_t msecs = tspec.tv_nsec / NANOSECONDS_IN_MSEC;
	time_t rawtime = tspec.tv_sec;
	struct tm *timeinfo = localtime(&rawtime);
	if (fprintf(stream, "%02d:%02d:%02d.%03d: ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, msecs) < 0) {
		perror("ERROR could not write to stream");
		return -1;
	}
	/* Write the string */
	va_list args;
	va_start(args, format);
	if (vfprintf(stream, format, args) < 0) {
		perror("ERROR could not write to stream");
		return -1;
	}
	va_end(args);
	/* Write the error (if possible) */
	if (stream != stderr || err == 0)
		fprintf(stream, "\n");
	else
		fprintf(stream, ": %s\n", strerror(errno));
	pthread_mutex_unlock(&lock);	/* Unlock mutex */
	return 0;
}

/*
 * Extracts information from the simulation and stores a status message in buffer.
 * Returns the number of characters stored in buffer, not including the terminating null character.
 */
int statustostr(char *buffer, const sim_t *sim) {
	/* "%lu" needs to be changes to "%llu" when compiling for ARM! */
	return sprintf(buffer, "status %lf %s %lu", sim->sd_water_temp, ((sim->sd_state == ON) ? "ON" : "OFF"), sim->sd_update_time);
}

/*
 * Prints the error message and exits the application with EXIT_FAILURE.
 */
void error(char *msg) {
	logln(stderr, msg);
	exit(EXIT_FAILURE);
}

/*
 * Keeps the information of the simulation up to date.
 */
void* update_sim(void *sim_desc) {
	sim_t *sim = (sim_t*)sim_desc;
	while (1) {
		if (calc(sim) < 0)
			error("ERROR could not calculate temperature");
//		logline("Current Temperature: %.3lf", sim->sd_water_temp);
		usleep(1);
	}
	return 0;
}

/*
 * Sends the current information in a predefined interval to the client.
 */
void* status_handler(void *conn) {
	sim_t *sim = ((conn_t*)conn)->c_sim;
	int client_sock = ((conn_t*)conn)->c_client.s_desc;
	unsigned interval = ((conn_t*)conn)->c_upd_int;
	char client_addr[22];
	clienttostr(client_addr, &((conn_t*)conn)->c_client);
	char buffer[NET_BUFFER_SIZE];
	while (1) {
		memset(buffer, 0, NET_BUFFER_SIZE);
		statustostr(buffer, sim);
		if (writeln(client_sock, buffer) < 0) {
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
	int client_sock = ((conn_t*)conn)->c_client.s_desc;
	int read_size = 0;
	char client_addr[22];
	clienttostr(client_addr, &((conn_t*)conn)->c_client);
	char buffer[NET_BUFFER_SIZE];
	memset(buffer, 0, NET_BUFFER_SIZE);
	pthread_t thread_id;
	/* Start the status handler */
	if (pthread_create(&thread_id, NULL, status_handler, (void*)conn) < 0) {
		logerror("ERROR starting status handler");
		logline(">> [%s]: error", client_addr);
		writeln(client_sock, "error");
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
			sim->sd_state = ON;
			statustostr(buffer, sim);
			writeln(client_sock, buffer);
			logline(">> [%s]: %s", client_addr, buffer);
		} else if (strcmp(buffer, "control off") == 0) {
			/* Turn the heater off! */
			logline("<< [%s]: control off", client_addr);
			sim->sd_state = OFF;
			statustostr(buffer, sim);
			writeln(client_sock, buffer);
			logline(">> [%s]: %s", client_addr, buffer);
		} else if (strcmp(buffer, "request") == 0) {
			/* Send your status! */
			logline("<< [%s]: request", client_addr);
			statustostr(buffer, sim);
			writeln(client_sock, buffer);
			logline(">> [%s]: %s", client_addr, buffer);
		} else {
			/* Unknown command! */
			logline("<< [%s]: %s", client_addr, buffer);
			logerror("[%s]: ERROR received unknown command: %s", client_addr, buffer);
			writeln(client_sock, "error");
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
	sim_t sim;
	init_sim(&sim, OFF, sim_env_temp, sim_water_vol, sim_power);
	/* Initialize the network socket */
	socket_t sock;
	if (init_socket(&sock) < 0)
		error("ERROR opening socket");
	if (bind_socket(&sock, net_port) < 0)
		error("ERROR binding socket");
	if (listen_socket(&sock, NET_MAX_PENDING) < 0)
		error("ERROR listening socket");
	logline("Waiting for incoming connections on port %i", net_port);
	client_t client;
	pthread_t thread_id;
	/* Start the simulation */
	if (pthread_create(&thread_id, NULL, update_sim, (void*)&sim) < 0)
		error("ERROR starting sim");
	/* Wait for incoming connections */
	while (accept_socket(&sock, &client) > 0) {
		conn_t conn;
		memset(&conn, 0, sizeof(conn_t));
		conn.c_client = client;
		conn.c_sim = &sim;
		conn.c_upd_int = upd_interval;
		char buffer[22];
		clienttostr(buffer, &client);
		logline("[%s] has connected!", buffer);
		/* Start the connection handler */
		if (pthread_create(&thread_id, NULL, connection_handler, (void*)&conn) < 0)
			error("ERROR creating connection handler");
	}
	if (client.s_desc < 0)	/* Socket is dead */
		error("ERROR accepting client_addr");
	logline("Simulation terminated!\n");	/* It's dead! */
	return EXIT_SUCCESS;
}
