#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	OFF, ON
} sim_state_t;

typedef struct sim_t sim_t;

typedef struct {
	sim_state_t sim_state;
	double sim_water_temp;
	uint64_t sim_timestamp;
} sim_temp_t;

sim_t* sim_create(double water_volume, double power_capacity, double ambient_temp, double water_temp);

bool sim_free(sim_t* sim);

bool sim_start(sim_t* sim);

bool sim_status(sim_t const* sim);

bool sim_stop(sim_t* sim);

bool sim_control(sim_t* sim, sim_state_t sim_state);

bool sim_gettemp(sim_temp_t* sim_temp, sim_t const* sim);
