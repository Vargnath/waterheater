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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "log.h"

#define NS_IN_MSEC 1000000

int flogf(FILE* stream, char const* format, ...) {
	struct timespec tspec;
	if (clock_gettime(CLOCK_REALTIME, &tspec) == -1) {
		// errno was set by clock_gettime
		return -1;
	}
	int msecs = tspec.tv_nsec / NS_IN_MSEC;
	struct tm* timeinfo = localtime(&tspec.tv_sec);
	char* timestamp = 0;
	if (asprintf(&timestamp, "%02d:%02d:%02d.%03d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, msecs) == -1) {
		return -1;
	}
	va_list args;
	va_start(args, format);
	char* line = 0;
	int result = 0;
	if ((result = vasprintf(&line, format, args)) >= 0) {
		result = fprintf(stream, "%s: %s\n", timestamp, line);
	}
	va_end(args);
	free(timestamp);
	free(line);
	return result;
}
