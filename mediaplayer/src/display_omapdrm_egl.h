/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2019-2020 Pawel Kolodziejski
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
extern "C" {
	#include <libdrm/omap_drmif.h>
}
#endif
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gbm/gbm.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

namespace MediaPLayer {

class DisplayOmapDrmEgl : public Display {
private:

	typedef struct {
		int         fd;
		gbm_bo 		*gbmBo;
		uint32_t	fbId;
	} DrmFb;

	int                         _fd;
	omap_device                 *_omapDevice;
	gbm_device                  *_gbmDevice;
	gbm_surface                 *_gbmSurface;

	drmModeResPtr               _drmResources;
	drmModePlaneResPtr          _drmPlaneResources;
	drmModeCrtcPtr              _oldCrtc;
	drmModeModeInfo             _modeInfo;
	uint32_t                    _connectorId;
	uint32_t                    _crtcId;
	int                         _planeId;

	EGLDisplay 					_eglDisplay;
	EGLSurface					_eglSurface;
	EGLConfig 					_eglConfig;
	EGLContext					_eglContext;

	U32                         _fbWidth, _fbHeight;

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
	static void drmFbDestroyCallback(gbm_bo *gbmBo, void *data);
	DrmFb *getDrmFb(gbm_bo *gbmBo);
	static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
	const char* eglGetErrorStr(EGLint error);
};

} // namespace

#endif
