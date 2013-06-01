/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013 Pawel Kolodziejski <aquadran at users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h> 
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "typedefs.h"
#include "pthread.h"
#include "logger_private.h"
#include "logger.h"

static logger_private_t logger;

void logger_init() {
	struct timeval current_time;

	if (logger.initialized == true)
		return;

	if (pthread_mutex_init(&logger.lock, NULL) != 0)
		return;

	gettimeofday(&current_time, NULL);
	logger.started_timestamp = ((current_time.tv_sec - 1) * 1000 + (current_time.tv_usec / 1000));

	printf("- compiled at time: " __DATE__ " " __TIME__ " -\n");

	logger.initialized = true;
}

void logger_deinit() {
	if (logger.initialized == false)
		return;

	logger.initialized = false;

	if (pthread_mutex_destroy(&logger.lock) != 0)
		return;
}

#define MAX_LOG_MSG_SIZE 1000

void logger_printf(const char *format_str, ...) {
	va_list arguments;
	char message[MAX_LOG_MSG_SIZE];
	struct timeval current_time;

	if (logger.initialized == false)
		return;

	gettimeofday(&current_time, NULL);
	float timestamp = (current_time.tv_sec * 1000 + (current_time.tv_usec / 1000)) - logger.started_timestamp;

	va_start(arguments, format_str);
	vsnprintf(message, MAX_LOG_MSG_SIZE - 1, format_str, arguments);
	va_end(arguments);

	printf("[%.3f] %s", timestamp / 1000, message);

	pthread_mutex_lock(&logger.lock);

	FILE *file = fopen("log.txt", "a");
	if (file != NULL) {
		fprintf(file, "[%.3f] %s", timestamp / 1000, message);
		fclose(file);
	}

	pthread_mutex_unlock(&logger.lock);
}
