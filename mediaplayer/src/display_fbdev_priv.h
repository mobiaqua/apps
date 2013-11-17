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

#ifndef DISPLAY_FBDEV_PRIV_H
#define DISPLAY_FBDEV_PRIV_H

#include <linux/fb.h>

#include "typedefs.h"

typedef struct {
	bool initialized;
	int fd;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	U8 *fb_ptr;
	U32 fb_size;
	U32 fb_stride;
	U32 fb_width;
	U32 fb_height;
	U32 dst_x, dst_y;
	U32 dst_width, dst_height;
} display_fbdev_t;

bool display_fbdev_init();
void display_fbdev_deinit();
bool display_fbdev_configure(U32 width, U32 height);
bool display_fbdev_putimage(U8 *src[], U32 src_stride[], FORMAT_VIDEO format);
bool display_fbdev_flip();

#endif
