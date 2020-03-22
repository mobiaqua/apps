/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013-2019 Pawel Kolodziejski
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

struct OmapDrmOSD {
	struct omap_bo             *bo;
	U8                         *ptr;
	uint32_t				   fbId;
};

#define NUM_DRM_BUFFERS 2

class DisplayOmapDrmEgl : public Display {
private:

	int                         _fd;
	omap_device                 *_omapDevice;
	OmapDrmOSD	                _drmBuffers[NUM_DRM_BUFFERS];
	int                         _drmBufIdx;

	drmModeResPtr               _drmResources;
	drmModePlaneResPtr          _drmPlaneResources;
	drmModeCrtcPtr              _oldCrtc;
	drmModeModeInfo             _modeInfo;
	uint32_t                    _connectorId;
	uint32_t                    _crtcId;
	int                         _planeId;

	U32                         _fbStride;
	U32                         _fbWidth, _fbHeight;
	SwsContext                  *_scaleCtx;

public:

	DisplayOmapDrmEgl();
	~DisplayOmapDrmEgl();

	STATUS init();
	STATUS deinit();
	STATUS configure(FORMAT_VIDEO videoFmt, int videoFps, int videoWidth, int videoHeight);
	STATUS putImage(VideoFrame *frame);
	STATUS flip();

private:

	STATUS internalInit();
	void internalDeinit();
};

} // namespace

#endif
