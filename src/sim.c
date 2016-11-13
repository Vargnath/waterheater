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
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "sim.h"

#define SPECIFIC_HEAT_CAPACITY 4.182
#define THRESHOLD_WATER_TEMP 100.0
#define NS_IN_SEC 1000000000.0

typedef struct sim_t {
	sim_state_t sim_heating_state;
	double sim_ambient_temp;
	double sim_water_temp;
	double sim_water_volume;
	double sim_power_capacity;
	uint64_t sim_updated_timestamp;
	bool sim_alive;
	pthread_t sim_thread;
} sim_t;

bool get_timestamp(uint64_t* timer) {
	if (!timer) {
		errno = EINVAL;
		return false;
	}
	struct timespec tspec;
	if (clock_gettime(CLOCK_REALTIME, &tspec) == 0) {
		*timer = tspec.tv_sec * NS_IN_SEC + tspec.tv_nsec;
		return true;
	}
	// errno was set by clock_gettime
	return false;
}

sim_t* sim_create(double water_volume, double power_capacity, double ambient_temp, double water_temp) {
	sim_t* sim = calloc(1, sizeof(sim_t));
	if (!sim) {
		errno = EINVAL;
		return 0;
	}
	if (!get_timestamp(&sim->sim_updated_timestamp)) {
		// free the incomplete sim struct
		free(sim);
		// errno was set by get_timestamp
		return 0;
	}
	sim->sim_alive = false;
	sim->sim_ambient_temp = ambient_temp;
	sim->sim_heating_state = OFF;
	sim->sim_power_capacity = power_capacity;
	sim->sim_thread = 0;
	sim->sim_water_temp = water_temp;
	sim->sim_water_volume = water_volume;
	return sim;
}

bool sim_free(sim_t* sim) {
	if (sim && sim->sim_thread) {
		sim->sim_alive = false;
		int err = pthread_join(sim->sim_thread, 0);
		if (!(err == 0 || err == ESRCH)) {
			// err's values match the errno values in errno.h
			errno = err;
			return false;
		}
	}
	free(sim);
	return true;
}

bool sim_heating_update(sim_t* sim) {
	if (!sim) {
		errno = EINVAL;
		return false;
	}
	uint64_t timestamp = 0;
	if (get_timestamp(&timestamp)) {
		sim->sim_water_temp = sim->sim_water_temp + (sim->sim_power_capacity * (timestamp - sim->sim_updated_timestamp) / NS_IN_SEC) / (sim->sim_water_volume * SPECIFIC_HEAT_CAPACITY);
		sim->sim_updated_timestamp = timestamp;
		return true;
	}
	// errno was set by get_timestamp
	return false;
}

bool sim_cooling_update(sim_t* sim) {
	if (!sim) {
		errno = EINVAL;
		return false;
	}
	uint64_t timestamp = 0;
	if (get_timestamp(&timestamp)) {
		sim->sim_water_temp = sim->sim_ambient_temp + (sim->sim_water_temp - sim->sim_ambient_temp) * exp(-((timestamp - sim->sim_updated_timestamp) / NS_IN_SEC) / sim->sim_water_volume);
		sim->sim_updated_timestamp = timestamp;
		return true;
	}
	// errno was set by get_timestamp
	return false;
}

void* sim_thread_handle(void* sim_ptr) {
	sim_t* sim = (sim_t*)sim_ptr;
	if (!sim) {
		errno = EINVAL;
		return 0;
	}
	sim->sim_alive = true;
	while (sim->sim_alive) {
		switch (sim->sim_heating_state) {
		case OFF:
			sim_cooling_update(sim);
		break;
		case ON:
			sim_heating_update(sim);
			if (sim->sim_water_temp >= THRESHOLD_WATER_TEMP) {
				sim->sim_heating_state = OFF;
			}
		break;
		default:
			errno = EINVAL;
			sim->sim_alive = false;
		}
		// keeps the CPU cool
		usleep(100);
	}
	sim->sim_thread = 0;
	return 0;
}

bool sim_start(sim_t* sim) {
	if (!sim) {
		errno = EINVAL;
		return false;
	}
	if (sim->sim_thread && sim->sim_alive) {
		return true;
	}
	int err = 0;
	if (!sim->sim_alive) {
		err = pthread_join(sim->sim_thread, 0);
	}
	if (err == 0 || err == ESRCH) {
		if ((err = pthread_create(&sim->sim_thread, 0, sim_thread_handle, (void*)sim)) != -1) {
			return true;
		}
	}
	sim->sim_thread = 0;
	// err's values match the errno values in errno.h
	errno = err;
	return false;
}

bool sim_status(sim_t const* sim) {
	if (!sim) {
		errno = EINVAL;
		return false;
	}
	return sim->sim_alive;
}

bool sim_stop(sim_t* sim) {
	if (!sim) {
		errno = EINVAL;
		return false;
	}
	sim->sim_alive = false;
	return true;
}

bool sim_control(sim_t* sim, sim_state_t sim_state) {
	if (!sim) {
		errno = EINVAL;
		return false;
	}
	sim->sim_heating_state = sim_state;
	return true;
}

bool sim_gettemp(sim_temp_t* sim_temp, sim_t const* sim) {
	if (!sim) {
		errno = EINVAL;
		return false;
	}
	sim_temp->sim_state = sim->sim_heating_state;
	sim_temp->sim_timestamp = sim->sim_updated_timestamp;
	sim_temp->sim_water_temp = sim->sim_water_temp;
	return true;
}
