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

#ifndef DISPLAY_OMAPDRM_H
#define DISPLAY_OMAPDRM_H

#include "display_base.h"
#include "basetypes.h"
#ifdef __cplusplus
extern "C"
{
	#include <libdrm/omap_drmif.h>
	#include <libswscale/swscale.h>
}
#endif
#include <xf86drmMode.h>
#include <drm_fourcc.h>

namespace MediaPLayer {

#define NUM_OSD_FB   2
#define NUM_VIDEO_FB 3

class DisplayOmapDrm : public Display {
private:

	typedef struct {
		struct omap_bo  *bo;
		uint32_t        fbId;
		void            *ptr;
		U32             width, height;
		U32             stride;
		U32             size;
	} OSDBuffer;

	typedef struct {
		struct omap_bo  *bo;
		uint32_t        fbId;
		void            *ptr;
		U32             width, height;
		U32             stride;
		U32             size;
		U32             dstX, dstY;
		U32             dstWidth, dstHeight;
	} VideoBuffer;

	int                         _fd;
	struct omap_device          *_omapDevice;
	drmModeResPtr               _drmResources;
	drmModePlaneResPtr          _drmPlaneResources;
	drmModeCrtcPtr              _oldCrtc;
	drmModeModeInfo             _modeInfo;
	uint32_t                    _connectorId;
	uint32_t                    _crtcId;
	int                         _planeId;

	OSDBuffer                   _osdBuffers[NUM_OSD_FB]{};
	VideoBuffer                 _videoBuffers[NUM_VIDEO_FB]{};

	int                         _currentOSDBuffer;
	int                         _currentVideoBuffer;
	SwsContext                  *_scaleCtx;

public:

	DisplayOmapDrm();
	~DisplayOmapDrm();

	STATUS init();
	STATUS deinit();
	STATUS configure(FORMAT_VIDEO videoFmt, int videoFps, int videoWidth, int videoHeight);
	STATUS putImage(VideoFrame *frame);
	STATUS flip();
	STATUS getHandle(DisplayHandle *handle);
	STATUS getVideoBuffer(DisplayVideoBuffer *handle, FORMAT_VIDEO pixelfmt, int width, int height);
	STATUS releaseVideoBuffer(DisplayVideoBuffer *handle);

private:

	STATUS internalInit();
	void internalDeinit();
	static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
};

} // namespace

#endif
