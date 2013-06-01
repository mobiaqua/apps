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

#ifndef DISPLAY_H
#define DISPLAY_H

#include "typedefs.h"

typedef enum {
	ARGB32,
	YUV420,
	NV12
} FORMAT_VIDEO;

typedef bool (display_init_t)();
typedef void (display_deinit_t)();
typedef bool (display_configure_t)(int, int);
typedef bool (display_putimage_t)(unsigned char *src[], int src_stride[], FORMAT_VIDEO);
typedef bool (display_flip_t)();

typedef struct {
	bool	initialized;
	void	*priv;

	display_init_t *init;
	display_deinit_t *deinit;
	display_configure_t *configure;
	display_putimage_t *putimage;
	display_flip_t *flip;
} display_t;

display_t *display_get(const char *driver_name);
void display_release(display_t *display);

#endif
