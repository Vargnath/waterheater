#ifndef WH_SIM_H
#define WH_SIM_H 1

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define SPECIFIC_HEAT_CAPACITY_WATER 4.182
#define THRESHOLD_WATER_TEMP 100.0
#define NANOSECONDS_IN_SEC 1000000000.0
#define NANOSECONDS_IN_MSEC 1000000.0

typedef enum {
	OFF, ON
} state_t;

typedef struct {
	state_t sd_state;			/* Current state of the heating unit */
	uint64_t sd_update_time;	/* Timestamp of the last update in nanoseconds */
	double sd_env_temp;			/* Environment temperature */
	double sd_water_temp;		/* Current water temperature */
	double sd_water_vol;		/* Amount of water in g (1.000 g ~ 1 l) */
	double sd_power;			/* Power of the heating unit in W */
} sim_t;

void init_sim(sim_t *sim, state_t state, double env_temp, double water_vol, double power);

int get_nanotime(uint64_t *timer);

int calc_heating(sim_t *sd);

int calc_cooling(sim_t *sd);

int calc(sim_t *sd);

#endif	// WH_SIM_H
