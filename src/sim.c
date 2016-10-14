#include "sim.h"

/*
 * Initializes the simulation with the given values.
 * The water temperatures is equal the environmental temperature.
 * The update time is set to the current time.
 */
void init_sim(sim_t *sim, state_t state, double env_temp, double water_vol, double power) {
	memset(sim, 0, sizeof(sim_t));
	sim->sd_state = state;
	get_nanotime(&sim->sd_update_time);
	sim->sd_env_temp = env_temp;
	sim->sd_water_temp =env_temp;
	sim->sd_water_vol = water_vol;
	sim->sd_power = power;
}

/*
 * Writes the currect time in nanoseconds into the given 64-bit unsigned.
 * Returns 0 on success or -1 in case of error.
 */
int get_nanotime(uint64_t *timer) {
	struct timespec tspec;
	if (clock_gettime(CLOCK_REALTIME, &tspec) == 0) {
		*timer = tspec.tv_sec * NANOSECONDS_IN_SEC + tspec.tv_nsec;
		return 0;
	}
	return -1;
}

/*
 * Calculates the temperature increase since the last calculation.
 * The old temperatures plus the increase is saved in the simulation.
 * Returns 0 on success or -1 in case of error.
 *
 * See: http://www.mrclab.com/Media/Uploads/Water_heating_time_calculating.pdf
 */
int calc_heating(sim_t *sd) {
	uint64_t time_nano;
	if (get_nanotime(&time_nano) == 0) {
		/* T_new = T_old + (Power * t_dif) / Volume * C */
		sd->sd_water_temp = sd->sd_water_temp + (sd->sd_power * (time_nano - sd->sd_update_time) / NANOSECONDS_IN_SEC) / (sd->sd_water_vol * SPECIFIC_HEAT_CAPACITY_WATER);
		sd->sd_update_time = time_nano;
		if (sd->sd_water_temp >= THRESHOLD_WATER_TEMP)
			sd->sd_state = OFF;
		return 0;
	}
	return -1;
}

/*
 * Calculates the temperature drop since the last caluclation.
 * The old temperatures minus the drop is saved in the simulation.
 * Returns 0 on success or -1 in case of error.
 *
 * See: http://ugrad.math.ubc.ca/coursedoc/math100/notes/diffeqs/cool.html
 */
int calc_cooling(sim_t *sd) {
	uint64_t time_nano;
	if (get_nanotime(&time_nano) == 0) {
		/* T_new = T_env + (T_old - T_env) * exp(-t_dif / Volume) */
		double new_water_temp = sd->sd_env_temp + (sd->sd_water_temp - sd->sd_env_temp) * exp(-((time_nano - sd->sd_update_time) / NANOSECONDS_IN_SEC) / sd->sd_water_vol);
		sd->sd_water_temp = (isnan(new_water_temp) ? round(sd->sd_water_temp) : new_water_temp);
		sd->sd_update_time = time_nano;
		return 0;
	}
	return -1;
}

/*
 * Calculates the new temperature based on the current state of the simulation.
 * Returns 0 on success or -1 in case of error.
 */
int calc(sim_t *sd) {
	switch (sd->sd_state) {
	case ON:	/* Heating */
		if (calc_heating(sd) < 0)
			return -1;
	break;
	case OFF:	/* Cooling */
		if (calc_cooling(sd) < 0)
			return -1;
	break;
	default:	/* This should never happen */
		return -1;
	}
	return 0;
}
