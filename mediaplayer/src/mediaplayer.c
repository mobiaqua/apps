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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "typedefs.h"
#include "logger.h"
#include "display.h"

static display_t *display = NULL;

int main(int argc, char *argv[]) {
	int option;
	char *filename;
	unsigned char *fb;
	unsigned char *fb_src[480];
	int fb_stride[480];
	int y;

	logger_init();

	while ((option = getopt(argc, argv, ":")) != -1) {
		switch (option) {
		default:
			break;
		}
	}

	if (optind < argc) {
		filename = argv[optind];
		logger_printf("\ninput file: %s\n\n", filename);
	} else {
		logger_printf("Missing filename param!\n");
		goto end;
	}

	display = display_get("fbdev");
	if (display == NULL) {
		logger_printf("Failed get handle to display!\n");
		goto end;
	}
	if (display->init() == false) {
		logger_printf("Failed init display!\n");
		goto end;
	}

	if (display->configure(640, 480) == false) {
		logger_printf("Failed configure display!\n");
		goto end;
	}

	fb = calloc(640 * 480, 4);
	memset(fb, 0x55, 640*480*4);

	for (y = 0; y < 480; y++) {
		fb_stride[y] = 640 * 4;
		fb_src[y] = fb + (fb_stride[y] * y);
	}

	if (display->putimage(fb_src, fb_stride, ARGB32) == false) {
		logger_printf("Failed configure display!\n");
	}

	if (display->flip() == false) {
		logger_printf("Failed flip display!\n");
	}

end:
	if (display != NULL) {
		display->deinit();
		display_release(display);
		display = NULL;
	}

	logger_deinit();

	return 0;
}
