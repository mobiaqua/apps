/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013-2014 Pawel Kolodziejski <aquadran at users.sourceforge.net>
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

#ifndef DISPLAY_BASE_H
#define DISPLAY_BASE_H

#include "basetypes.h"
#include "avtypes.h"
#include "decoder_video_base.h"

namespace MediaPLayer {

class Display {
protected:

	bool	_initialized;

public:

	Display();
	virtual ~Display();

	virtual STATUS init() = 0;
	virtual STATUS deinit() = 0;
	virtual STATUS configure(U32 width, U32 height) = 0;
	virtual STATUS putImage(VideoFrame *frame) = 0;
	virtual STATUS flip() = 0;
};

Display *CreateDisplay(DISPLAY_TYPE displayType);

} // namespace

#endif
