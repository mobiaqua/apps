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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include "display.h"
#include "display_fbdev_priv.h"
#include "logger.h"

static display_fbdev_t device;

static bool fbdev_init() {

	memset(&device, 0, sizeof(display_t));

	device.fd = open("/dev/fb0", O_RDWR);
	if (device.fd == -1) {
		logger_printf("fbdev_init(): Failed open /dev/fb0 %s\n", strerror(errno));
		goto fail;
	}

	if (ioctl(device.fd, FBIOGET_VSCREENINFO, &device.vinfo) == -1) {
		logger_printf("fbdev_init(): Failed FBIOGET_VSCREENINFO on /dev/fb0. %s\n", strerror(errno));
		goto fail;
	}

	device.vinfo.xres_virtual = device.vinfo.xres;
	device.vinfo.yres_virtual = device.vinfo.yres;

	if (ioctl(device.fd, FBIOPUT_VSCREENINFO, &device.vinfo) == -1) {
		logger_printf("fbdev_init(): Failed FBIOPUT_VSCREENINFO on /dev/fb0. %s\n", strerror(errno));
		goto fail;
	}

	if (device.vinfo.bits_per_pixel != 32) {
		logger_printf("fbdev_init(): Display buffer is not 32 bits per pixel!\n");
		goto fail;
	}

	if (ioctl(device.fd, FBIOGET_FSCREENINFO, &device.finfo) == -1) {
		logger_printf("fbdev_init(): Failed FBIOGET_FSCREENINFO on /dev/fb0. %s\n", strerror(errno));
		goto fail;
	}

	if (device.finfo.type != FB_TYPE_PACKED_PIXELS) {
		logger_printf("fbdev_init(): Only type FB_TYPE_PACKED_PIXELS is supported!\n");
		goto fail;
	}

	if (device.finfo.visual != FB_VISUAL_TRUECOLOR) {
		logger_printf("fbdev_init(): Only FB_VISUAL_TRUECOLOR is supported!\n");
		goto fail;
	}

	device.fb_size = device.finfo.smem_len;
	device.fb_stride = device.finfo.line_length;
	device.fb_width = device.vinfo.xres;
	device.fb_height = device.vinfo.yres;
	device.fb_ptr = (U8 *)mmap(0, device.fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, device.fd, 0);
	if (device.fb_ptr == (U8 *)-1) {
		logger_printf("fbdev_init(): Failed get frame buffer! %s\n", strerror(errno));
		goto fail;
	}

	memset(device.fb_ptr, 0, device.fb_size);

	device.initialized = true;
	return true;

fail:

	if (device.fd != -1)
		close(device.fd);

	return false;
}

static void fbdev_deinit() {
	if (device.initialized == false)
		return;

	if (device.fb_ptr)
		munmap(device.fb_ptr, device.fb_size);

	if (device.fd != -1)
		close(device.fd);

	device.initialized = false;
}

bool display_fbdev_init() {
	if (device.initialized == true)
		return false;

	memset(&device, 0, sizeof(display_fbdev_t));

	if (fbdev_init() == false)
		return false;

	return true;
}

void display_fbdev_deinit() {
	if (device.initialized == false)
		return;

	fbdev_deinit();
}

bool display_fbdev_configure(U32 width, U32 height) {
	if (width <= 0 || height <= 0) {
		logger_printf("display_fbdev_configure(): Bad arguments!\n");
		goto fail;
	}

	if (device.vinfo.xres < width || device.vinfo.yres < height) {
		logger_printf("display_fbdev_configure(): Given resulution is bigger than frame buffer resolution!\n");
		goto fail;
	}

	device.dst_x = (device.vinfo.xres - width) / 2;
	device.dst_y = (device.vinfo.yres - height) / 2;
	device.dst_width = width;
	device.dst_height = height;

	return true;

fail:
	return false;
}

bool display_fbdev_putimage(U8 *src[], U32 src_stride[], FORMAT_VIDEO format) {
	int y;

	if (src[0] == NULL || src_stride[0] <= 0) {
		logger_printf("display_fbdev_configure(): Bad arguments!\n");
		goto fail;
	}

	for (y = 0; y < device.dst_height; y++) {
		memcpy(device.fb_ptr + (device.fb_stride * (device.dst_y + y)) + device.dst_x * 4, src[y], src_stride[y]);
	}

	return true;

fail:
	return false;
}

bool display_fbdev_flip() {

	// nothing

	return true;
}
