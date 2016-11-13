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
