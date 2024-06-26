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

#ifndef DISPLAY_OMAPDRM_EGL_H
#define DISPLAY_OMAPDRM_EGL_H

#include "display_base.h"
#include "basetypes.h"
#ifdef __cplusplus
extern "C" {
	#include <libswscale/swscale.h>
}
#endif
#include <cstdint>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace MediaPLayer {

class DisplayOmapDrmEgl : public Display {
private:

	typedef struct {
		int            fd;
		gbm_bo         *gbmBo;
		uint32_t       fbId;
	} DrmFb;

	typedef struct {
		uint32_t       handle;
		int            dmabuf;
		void           *mapPtr;
		uint32_t       mapSize;
		EGLImageKHR    image;
		GLuint         glTexture;
		DisplayVideoBuffer *db;
	} RenderTexture;

	int                         _fd;
	gbm_device                  *_gbmDevice;
	gbm_surface                 *_gbmSurface;

	drmModeResPtr               _drmResources;
	drmModePlaneResPtr          _drmPlaneResources;
	drmModeCrtcPtr              _oldCrtc;
	drmModeModeInfo             _modeInfo;
	uint32_t                    _connectorId;
	uint32_t                    _crtcId;
	int                         _planeId;

	uint32_t                    _primaryHandle;
	uint32_t                    _primaryFbId;
	void                        *_primaryPtr;
	U32                         _primarySize;

	EGLDisplay                  _eglDisplay;
	EGLSurface                  _eglSurface;
	EGLConfig                   _eglConfig;
	EGLContext                  _eglContext;
	PFNEGLCREATEIMAGEKHRPROC    eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC   eglDestroyImageKHR;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
	GLuint                      _vertexShader;
	GLuint                      _fragmentShader;
	GLuint                      _glProgram;
	RenderTexture               *_renderTexture;
	U32                         _fbWidth, _fbHeight;

	SwsContext                  *_scaleCtx;

public:

	DisplayOmapDrmEgl();
	~DisplayOmapDrmEgl();

	STATUS init(bool hwAccelDecode);
	STATUS deinit();
	STATUS configure(FORMAT_VIDEO videoFmt, int videoFps, int videoWidth, int videoHeight);
	STATUS putImage(VideoFrame *frame, bool skip);
	STATUS flip(bool skip);
	STATUS getHandle(DisplayHandle *handle);
	STATUS getDisplayVideoBuffer(DisplayVideoBuffer *handle, FORMAT_VIDEO pixelfmt, int width, int height);
	STATUS releaseDisplayVideoBuffer(DisplayVideoBuffer *handle);

private:

	STATUS internalInit();
	void internalDeinit();
	static void drmFbDestroyCallback(gbm_bo *gbmBo, void *data);
	DrmFb *getDrmFb(gbm_bo *gbmBo);
	const char* eglGetErrorStr(EGLint error);
	RenderTexture *getVideoBuffer(FORMAT_VIDEO pixelfmt, int width, int height);
	STATUS releaseVideoBuffer(RenderTexture *texture);
	static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
};

} // namespace

#endif
