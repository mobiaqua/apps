/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013-2020 Pawel Kolodziejski
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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

struct omap_bo;

namespace MediaPLayer {

typedef struct {
	int     handle;
} DisplayHandle;

typedef struct {
	void           *priv;
	struct omap_bo *bo;
	U32            boHandle;
	bool           locked;
} DisplayVideoBuffer;

class Display {
protected:

	bool	_initialized;
	bool    _hwAccelDecode;

public:

	Display();
	virtual ~Display() {}

	virtual STATUS init(bool hwAccelDecode) = 0;
	virtual STATUS deinit() = 0;
	virtual STATUS configure(FORMAT_VIDEO videoFmt, int videoFps, int videoWidth, int videoHeight) = 0;
	virtual STATUS putImage(VideoFrame *frame, bool skip) = 0;
	virtual STATUS flip(bool skip) = 0;
	virtual STATUS getHandle(DisplayHandle *handle) = 0;
	virtual STATUS getDisplayVideoBuffer(DisplayVideoBuffer *handle, FORMAT_VIDEO pixelfmt, int width, int height) = 0;
	virtual STATUS releaseDisplayVideoBuffer(DisplayVideoBuffer *handle) = 0;
};

Display *CreateDisplay(DISPLAY_TYPE displayType);

} // namespace

#endif
