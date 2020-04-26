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

#include "display_omapdrm_egl.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include "display_base.h"
#include "logs.h"

static struct frame_info {
	unsigned int w;
	unsigned int h;
	unsigned int dx;
	unsigned int dy;
	unsigned int dw;
	unsigned int dh;
	unsigned int y_stride;
	unsigned int uv_stride;
} yuv420_frame_info, nv12_frame_info;

extern "C" int yuv420_to_nv12_convert(unsigned char *vdst[3], unsigned char *vsrc[3], unsigned char *, unsigned char *);
extern "C" void yuv420_to_nv12_open(struct frame_info *dst, struct frame_info *src);

namespace MediaPLayer {

DisplayOmapDrmEgl::DisplayOmapDrmEgl() :
		_fd(-1), _omapDevice(nullptr),
		_drmResources(nullptr), _drmPlaneResources(nullptr),
		_connectorId(-1), _oldCrtc(nullptr), _crtcId(-1), _planeId(-1),
		_primaryFbBo(nullptr), _primaryFbId(0),
		_gbmDevice(nullptr), _gbmSurface(nullptr),
		_eglDisplay(nullptr), _eglSurface(nullptr), _eglConfig(nullptr), _eglContext(nullptr),
		eglCreateImageKHR(nullptr), eglDestroyImageKHR(nullptr), glEGLImageTargetTexture2DOES(nullptr),
		_vertexShader(0), _fragmentShader(0), _glProgram(0), _renderTexture(nullptr),
		_fbWidth(0), _fbHeight(0), _scaleCtx(nullptr) {
}

DisplayOmapDrmEgl::~DisplayOmapDrmEgl() {
	deinit();
}

STATUS DisplayOmapDrmEgl::init(bool hwAccelDecode) {
	if (_initialized)
		return S_FAIL;

	_hwAccelDecode = hwAccelDecode;

	if (internalInit() == S_FAIL)
		return S_FAIL;

	return S_OK;
}

STATUS DisplayOmapDrmEgl::deinit() {
	if (!_initialized)
		return S_FAIL;

	internalDeinit();

	return S_OK;
}

#define EGL_STR_ERROR(value) case value: return #value;
const char* DisplayOmapDrmEgl::eglGetErrorStr(EGLint error) {
	switch (error) {
		EGL_STR_ERROR(EGL_SUCCESS)
		EGL_STR_ERROR(EGL_NOT_INITIALIZED)
		EGL_STR_ERROR(EGL_BAD_ACCESS)
		EGL_STR_ERROR(EGL_BAD_ALLOC)
		EGL_STR_ERROR(EGL_BAD_ATTRIBUTE)
		EGL_STR_ERROR(EGL_BAD_CONFIG)
		EGL_STR_ERROR(EGL_BAD_CONTEXT)
		EGL_STR_ERROR(EGL_BAD_CURRENT_SURFACE)
		EGL_STR_ERROR(EGL_BAD_DISPLAY)
		EGL_STR_ERROR(EGL_BAD_MATCH)
		EGL_STR_ERROR(EGL_BAD_NATIVE_PIXMAP)
		EGL_STR_ERROR(EGL_BAD_NATIVE_WINDOW)
		EGL_STR_ERROR(EGL_BAD_PARAMETER)
		EGL_STR_ERROR(EGL_BAD_SURFACE)
		EGL_STR_ERROR(EGL_CONTEXT_LOST)
		default: return "Unknown";
	}
}
#undef EGL_STR_ERROR

STATUS DisplayOmapDrmEgl::internalInit() {
	_fd = drmOpen("omapdrm", NULL);
	if (_fd < 0) {
		log->printf("DisplayOmapDrmEgl::internalInit(): Failed open omapdrm, %s\n", strerror(errno));
		goto fail;
	}

	_omapDevice = omap_device_new(_fd);
	if (!_omapDevice) {
		log->printf("DisplayOmapDrmEgl::internalInit(): Failed create omap device\n");
		goto fail;
	}

	_drmResources = drmModeGetResources(_fd);
	if (!_drmResources) {
		log->printf("DisplayOmapDrmEgl::internalInit(): Failed get DRM resources, %s\n", strerror(errno));
		goto fail;
	}

	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmResources) {
		log->printf("DisplayOmapDrmEgl::internalInit(): Failed get DRM plane resources, %s\n", strerror(errno));
		goto fail;
	}

	drmModeConnectorPtr connector;
	for (int i = 0; i < _drmResources->count_connectors; i++) {
		connector = drmModeGetConnector(_fd, _drmResources->connectors[i]);
		if (connector == nullptr)
			continue;
		if (connector->connection != DRM_MODE_CONNECTED ||
			connector->count_modes == 0) {
			drmModeFreeConnector(connector);
			continue;
		}
		if (connector->connector_type == DRM_MODE_CONNECTOR_HDMIA ||
			connector->connector_type == DRM_MODE_CONNECTOR_HDMIB) {
			_connectorId = connector->connector_id;
			drmModeFreeConnector(connector);
			break;
		}
	}

	if (_connectorId == -1) {
		log->printf("DisplayOmapDrmEgl::internalInit(): Failed to find active HDMI connector!\n");
		goto fail;
	}

	_gbmDevice = gbm_create_device(_fd);
	if (!_gbmDevice) {
		log->printf("DisplayOmapDrmEgl::internalInit(): Failed to create gbm device!\n");
		goto fail;
	}

	_scaleCtx = nullptr;

	_initialized = true;
	return S_OK;

fail:

	if (_gbmDevice != nullptr) {
		gbm_device_destroy(_gbmDevice);
		_gbmDevice = nullptr;
	}

	if (_drmPlaneResources != nullptr) {
		drmModeFreePlaneResources(_drmPlaneResources);
		_drmPlaneResources = nullptr;
	}

	if (_drmResources != nullptr) {
		drmModeFreeResources(_drmResources);
		_drmResources = nullptr;
	}

	if (_omapDevice != nullptr) {
		omap_device_del(_omapDevice);
		_omapDevice = nullptr;
	}

	if (_fd != -1) {
		drmClose(_fd);
		_fd = -1;
	}

	return S_FAIL;
}

void DisplayOmapDrmEgl::internalDeinit() {
	if (_initialized == false)
		return;

	if (_scaleCtx) {
		sws_freeContext(_scaleCtx);
		_scaleCtx = nullptr;
	}

	if (_vertexShader) {
	    glDeleteShader(_vertexShader);
	    _vertexShader = 0;
	}

	if (_fragmentShader) {
	    glDeleteShader(_fragmentShader);
	    _fragmentShader = 0;
	}

	if (_glProgram) {
		glDeleteProgram(_glProgram);
		_glProgram = 0;
	}

	if (_renderTexture) {
		releaseVideoBuffer(_renderTexture);
		_renderTexture = nullptr;
	}

	if (_eglDisplay) {
		glFinish();
		eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(_eglDisplay, _eglContext);
		eglDestroySurface(_eglDisplay, _eglSurface);
		eglTerminate(_eglDisplay);
		_eglDisplay = nullptr;
	}

	if (_gbmSurface) {
		gbm_surface_destroy(_gbmSurface);
		_gbmSurface = nullptr;
	}

	if (_gbmDevice != nullptr) {
		gbm_device_destroy(_gbmDevice);
		_gbmDevice = nullptr;
	}

	if (_oldCrtc) {
		drmModeSetCrtc(_fd, _oldCrtc->crtc_id, _oldCrtc->buffer_id,
				_oldCrtc->x, _oldCrtc->y, &_connectorId, 1, &_oldCrtc->mode);
		drmModeFreeCrtc(_oldCrtc);
		_oldCrtc = nullptr;
	}

	if (_primaryFbId) {
		drmModeRmFB(_fd, _primaryFbId);
		_primaryFbId = 0;
	}
	if (_primaryFbBo) {
		omap_bo_del(_primaryFbBo);
		_primaryFbBo = nullptr;
	}

	if (_drmPlaneResources != nullptr)
		drmModeFreePlaneResources(_drmPlaneResources);

	if (_drmResources != nullptr)
		drmModeFreeResources(_drmResources);

	if (_omapDevice != nullptr)
		omap_device_del(_omapDevice);

	if (_fd != -1)
		drmClose(_fd);

	_initialized = false;
}

STATUS DisplayOmapDrmEgl::configure(FORMAT_VIDEO videoFmt, int videoFps, int videoWidth, int videoHeight) {
	if (!_initialized)
		return S_FAIL;

	const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const GLchar *vertexShaderSource =
			"attribute vec4 position;                      \n"
			"attribute vec2 texCoord;                      \n"
			"varying   vec2 textureCoords;                 \n"
			"void main()                                   \n"
			"{                                             \n"
			"    textureCoords = texCoord;                 \n"
			"    gl_Position = position;                   \n"
			"}                                             \n";

	static const GLchar *fragmentShaderSource =
			"#extension GL_OES_EGL_image_external : require               \n"
			"precision mediump float;                                     \n"
			"varying vec2               textureCoords;                    \n"
			"uniform samplerExternalOES textureSampler;                   \n"
			"void main()                                                  \n"
			"{                                                            \n"
			"    gl_FragColor = texture2D(textureSampler, textureCoords); \n"
			"}                                                            \n";

	int modeId = -1;
	_crtcId = -1;
	const char *extensions;
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	int ret;

	switch (videoFmt) {
		case FMT_YUV420P:
		case FMT_NV12:
			break;
		default:
			log->printf("DisplayOmapDrmEgl::configure(): Unsupported input format!\n");
			return S_FAIL;
	}

	drmModeConnectorPtr connector = nullptr;
	for (int i = 0; i < _drmResources->count_connectors; i++) {
		connector = drmModeGetConnector(_fd, _drmResources->connectors[i]);
		if (connector == nullptr)
			continue;
		if (connector->connector_id == _connectorId)
			break;
		drmModeFreeConnector(connector);
	}
	if (connector == nullptr) {
		log->printf("DisplayOmapDrmEgl::configure(): Failed to find connector!\n");
		return S_FAIL;
	}

	for (int j = 0; j < connector->count_modes; j++) {
		auto mode = &connector->modes[j];
		if ((mode->vrefresh >= videoFps) && (mode->type & DRM_MODE_TYPE_PREFERRED)) {
			modeId = j;
			break;
		}
	}

	if (modeId == -1) {
		modeId = 0;
		U64 highestArea = 0;
		for (int j = 0; j < connector->count_modes; j++) {
			auto mode = &connector->modes[j];
			const U64 area = mode->hdisplay * mode->vdisplay;
			if ((mode->vrefresh >= videoFps) && (area > highestArea))
			{
				highestArea = area;
				modeId = j;
			}
		}
	}

	_modeInfo = connector->modes[modeId];

	for (int i = 0; i < connector->count_encoders; i++) {
		auto encoder = drmModeGetEncoder(_fd, connector->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id) {
			_crtcId = encoder->crtc_id;
			drmModeFreeEncoder(encoder);
			break;
		}
		drmModeFreeEncoder(encoder);
	}
	drmModeFreeConnector(connector);

	if (modeId == -1 || _crtcId == -1) {
		log->printf("DisplayOmapDrmEgl::configure(): Failed to find suitable display output!\n");
		return S_FAIL;
	}

	_planeId = -1;
	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	for (int i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (plane == nullptr)
			continue;
		if (plane->crtc_id == 0) {
			_planeId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}
		drmModeFreePlane(plane);
	}
	if (_planeId == -1) {
		log->printf("DisplayOmapDrmEgl::configure(): Failed to find plane!\n");
		return S_FAIL;
	}

	drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(_fd, _planeId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		log->printf("DisplayOmapDrmEgl::configure(): Failed to find properties for plane!\n");
		return S_FAIL;
	}
	for (int i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop != nullptr && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _planeId, DRM_MODE_OBJECT_PLANE, props->props[i], 0)) {
				log->printf("DisplayOmapDrmEgl::configure(): Failed to set zorder property for plane!\n");
				return S_FAIL;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

	_fbWidth = _modeInfo.hdisplay;
	_fbHeight = _modeInfo.vdisplay;

	log->printf("Using display HDMI output: %dx%d@%d\n", _fbWidth, _fbHeight, _modeInfo.vrefresh);

	_gbmSurface = gbm_surface_create(
			_gbmDevice,
			_fbWidth,
			_fbHeight,
			GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
			);
	if (!_gbmSurface) {
		log->printf("DisplayOmapDrmEgl::configure(): failed to create gbm surface!\n");
		goto fail;
	}

	_eglDisplay = eglGetDisplay((EGLNativeDisplayType)_gbmDevice);
	if (_eglDisplay == EGL_NO_DISPLAY) {
		log->printf("DisplayOmapDrmEgl::configure(): failed to create display!\n");
		goto fail;
	}

	EGLint major, minor;
	if (!eglInitialize(_eglDisplay, &major, &minor)) {
		log->printf("DisplayOmapDrmEgl::configure(): failed to initialize egl, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	log->printf("EGL vendor version: \"%s\"\n", eglQueryString(_eglDisplay, EGL_VERSION));

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		log->printf("DisplayOmapDrmEgl::configure(): failed to bind EGL_OPENGL_ES_API, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	EGLint numConfig;
	if (!eglChooseConfig(_eglDisplay, configAttribs, &_eglConfig, 1, &numConfig)) {
		log->printf("DisplayOmapDrmEgl::configure(): failed to choose config, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}
	if (numConfig != 1) {
		log->printf("DisplayOmapDrmEgl::configure(): more than 1 config: %d\n", numConfig);
		goto fail;
	}

	_eglContext = eglCreateContext(_eglDisplay, _eglConfig, EGL_NO_CONTEXT, contextAttribs);
	if (_eglContext == EGL_NO_CONTEXT) {
		log->printf("DisplayOmapDrmEgl::configure(): failed to create context, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	_eglSurface = eglCreateWindowSurface(_eglDisplay, _eglConfig, _gbmSurface, NULL);
	if (_eglSurface == EGL_NO_SURFACE) {
		log->printf("DisplayOmapDrmEgl::configure(): failed to create egl surface, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	if (!eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext)) {
		log->printf("DisplayOmapDrmEgl::configure(): failed attach rendering context to egl surface, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	if (!(eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR"))) {
		log->printf("DisplayOmapDrmEgl::configure(): No eglCreateImageKHR!\n");
		goto fail;
	}

	if (!(eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR"))) {
		log->printf("DisplayOmapDrmEgl::configure(): No eglDestroyImageKHR!\n");
		goto fail;
	}

	if (!(glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES"))) {
		log->printf("DisplayOmapDrmEgl::configure(): No glEGLImageTargetTexture2DOES!\n");
		goto fail;
	}

	extensions = (char *)glGetString(GL_EXTENSIONS);
	if (!strstr(extensions, "GL_TI_image_external_raw_video")) {
		log->printf("DisplayOmapDrmEgl::configure(): No GL_TI_image_external_raw_video extension!\n");
		goto fail;
	}

	GLint shaderStatus;
	_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(_vertexShader, 1, &vertexShaderSource, nullptr);
	glCompileShader(_vertexShader);
	glGetShaderiv(_vertexShader, GL_COMPILE_STATUS, &shaderStatus);
	if (!shaderStatus) {
		log->printf("DisplayOmapDrmEgl::configure(): vertex shader compilation failed!\n");
		glGetShaderiv(_vertexShader, GL_INFO_LOG_LENGTH, &shaderStatus);
		char logStr[shaderStatus];
		if (shaderStatus > 1) {
			glGetShaderInfoLog(_vertexShader, shaderStatus, nullptr, logStr);
			log->printf(logStr);
		}
		goto fail;
	}

	_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(_fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(_fragmentShader);
	glGetShaderiv(_fragmentShader, GL_COMPILE_STATUS, &shaderStatus);
	if (!shaderStatus) {
		log->printf("DisplayOmapDrmEgl::configure(): fragment shader compilation failed!\n");
		glGetShaderiv(_fragmentShader, GL_INFO_LOG_LENGTH, &shaderStatus);
		char logStr[shaderStatus];
		if (shaderStatus > 1) {
			glGetShaderInfoLog(_fragmentShader, shaderStatus, nullptr, logStr);
			log->printf(logStr);
		}
		goto fail;
	}

	_glProgram = glCreateProgram();

	glAttachShader(_glProgram, _vertexShader);
	glAttachShader(_glProgram, _fragmentShader);

	glBindAttribLocation(_glProgram, 0, "position");
	glBindAttribLocation(_glProgram, 1, "texCoord");

	glLinkProgram(_glProgram);
	glGetProgramiv(_glProgram, GL_LINK_STATUS, &shaderStatus);
	if (!shaderStatus) {
		char logStr[shaderStatus];
		log->printf("DisplayOmapDrmEgl::configure(): program linking failed!\n");
		glGetProgramiv(_glProgram, GL_INFO_LOG_LENGTH, &shaderStatus);
		if (shaderStatus > 1) {
			glGetProgramInfoLog(_glProgram, shaderStatus, NULL, logStr);
			log->printf(logStr);
		}
		goto fail;
	}

	glUseProgram(_glProgram);

	glViewport(0, 0, _fbWidth, _fbHeight);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);


	_primaryFbBo = omap_bo_new(_omapDevice, _modeInfo.hdisplay * _modeInfo.vdisplay * 4, OMAP_BO_WC | OMAP_BO_SCANOUT);
	if (!_primaryFbBo) {
		log->printf("DisplayOmapDrm::configure(): Failed allocate buffer!\n");
		return S_FAIL;
	}
	handles[0] = omap_bo_handle(_primaryFbBo);
	pitches[0] = _modeInfo.hdisplay * 4;
	ret = drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
	                	DRM_FORMAT_ARGB8888,
						handles, pitches, offsets, &_primaryFbId, 0);
	if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed add video buffer: %s\n", strerror(errno));
		return S_FAIL;
	}
	omap_bo_cpu_prep(_primaryFbBo, OMAP_GEM_WRITE);
	memset(omap_bo_map(_primaryFbBo), 0, omap_bo_size(_primaryFbBo));
	omap_bo_cpu_fini(_primaryFbBo, OMAP_GEM_WRITE);

	_oldCrtc = drmModeGetCrtc(_fd, _crtcId);
	ret = drmModeSetCrtc(_fd, _crtcId, _primaryFbId, 0, 0, &_connectorId, 1, &_modeInfo);
	if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed set crtc: %s\n", strerror(errno));
		return S_FAIL;
	}

	return S_OK;

fail:

	if (_vertexShader) {
		glDeleteShader(_vertexShader);
		_vertexShader = 0;
	}
	if (_fragmentShader) {
		glDeleteShader(_fragmentShader);
		_fragmentShader = 0;
	}
	if (_glProgram) {
		glDeleteProgram(_glProgram);
		_glProgram = 0;
	}
	if (_eglSurface) {
		eglDestroySurface(_eglDisplay, _eglSurface);
		_eglSurface = nullptr;
	}
	if (_eglContext) {
		eglDestroyContext(_eglDisplay, _eglContext);
		_eglContext = nullptr;
	}
	if (_eglDisplay) {
		eglTerminate(_eglDisplay);
		_eglDisplay = nullptr;
	}
	if (_gbmSurface) {
		gbm_surface_destroy(_gbmSurface);
		_gbmSurface = nullptr;
	}

	return S_FAIL;
}

void DisplayOmapDrmEgl::drmFbDestroyCallback(gbm_bo *gbmBo, void *data) {
	DisplayOmapDrmEgl::DrmFb *drmFb = (DisplayOmapDrmEgl::DrmFb *)data;

	if (drmFb->fbId)
		drmModeRmFB(drmFb->fd, drmFb->fbId);

	delete drmFb;
}

DisplayOmapDrmEgl::DrmFb *DisplayOmapDrmEgl::getDrmFb(gbm_bo *gbmBo) {
	if (!_initialized)
		return nullptr;

	uint32_t handles[4] = {}, pitches[4] = {}, offsets[4] = {};
	DisplayOmapDrmEgl::DrmFb *drmFb = (DisplayOmapDrmEgl::DrmFb *)gbm_bo_get_user_data(gbmBo);

	if (drmFb)
		return drmFb;

	drmFb = new DisplayOmapDrmEgl::DrmFb;
	drmFb->fd = _fd;
	drmFb->gbmBo = gbmBo;

	pitches[0] = gbm_bo_get_stride(gbmBo);
	handles[0] = gbm_bo_get_handle(gbmBo).u32;
	int ret = drmModeAddFB2(
			drmFb->fd,
			gbm_bo_get_width(gbmBo),
			gbm_bo_get_height(gbmBo),
			gbm_bo_get_format(gbmBo),
			handles,
			pitches,
			offsets,
			&drmFb->fbId,
			0
			);
	if (ret < 0) {
		log->printf("DisplayOmapDrmEgl::getDrmBuffer(): failed add video buffer: %s\n", strerror(errno));
		delete drmFb;
		return nullptr;
	}

	gbm_bo_set_user_data(gbmBo, drmFb, drmFbDestroyCallback);

	return drmFb;
}

STATUS DisplayOmapDrmEgl::putImage(VideoFrame *frame, bool skip) {
	if (!_initialized)
		return S_FAIL;

	RenderTexture *renderTexture;
	uint32_t fourcc;
	uint32_t stride;
	uint32_t fbSize;
	float x, y;
	float cropLeft, cropRight, cropTop, cropBottom;
	GLfloat coords[] = {
		0.0f,  1.0f,
		1.0f,  1.0f,
		0.0f,  0.0f,
		1.0f,  0.0f,
	};

	if (frame == nullptr || frame->data[0] == nullptr) {
		log->printf("DisplayOmapDrmEgl::putImage(): Bad arguments!\n");
		goto fail;
	}

	if (frame->anistropicDVD) {
		x = 1;
		y = 1;
	} else {
		x = (float)(frame->dw) / _fbWidth;
		y = (float)(frame->dh) / _fbHeight;
		if (x >= y) {
			y /= x;
			x = 1;
		} else {
			x /= y;
			y = 1;
		}
	}

	GLfloat position[8];
	position[0] = -x;
	position[1] = -y;
	position[2] =  x;
	position[3] = -y;
	position[4] = -x;
	position[5] =  y;
	position[6] =  x;
	position[7] =  y;

	cropLeft = (float)(frame->dx) / frame->width;
	cropRight = (float)(frame->dw + frame->dx) / frame->width;
	cropTop = (float)(frame->dy) / frame->height;
	cropBottom = (float)(frame->dh + frame->dy) / frame->height;

	coords[0] = coords[4] = cropLeft;
	coords[2] = coords[6] = cropRight;
	coords[5] = coords[7] = cropTop;
	coords[1] = coords[3] = cropBottom;

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, position);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, coords);
	glEnableVertexAttribArray(1);

	if (_hwAccelDecode) {
		DisplayVideoBuffer *db = (DisplayVideoBuffer *)(frame->data[0]);
		renderTexture = (RenderTexture *)db->priv;
	} else if (!_renderTexture) {
		_renderTexture = renderTexture = getVideoBuffer(frame->pixelfmt, frame->width, frame->height);
		if (!_renderTexture) {
			goto fail;
		}
	} else {
		renderTexture = _renderTexture;
	}

	if (!_hwAccelDecode) {
		uint8_t *srcPtr[4] = {};
		uint8_t *dstPtr[4] = {};
		int srcStride[4] = {};
		int dstStride[4] = {};

		uint8_t *dst = (uint8_t *)renderTexture->mapPtr;
		if (frame->pixelfmt == FMT_YUV420P && (ALIGN2(frame->width, 5) == frame->width)) {
			srcPtr[0] = frame->data[0];
			srcPtr[1] = frame->data[1];
			srcPtr[2] = frame->data[2];
			dstPtr[0] = dst;
			dstPtr[1] = dst + frame->width * frame->height;
			dstPtr[2] = 0;

			yuv420_frame_info.w = frame->width;
			yuv420_frame_info.h = frame->height;
			yuv420_frame_info.dx = 0;
			yuv420_frame_info.dy = 0;
			yuv420_frame_info.dw = frame->width;
			yuv420_frame_info.dh = frame->height;
			yuv420_frame_info.y_stride = frame->stride[0];
			yuv420_frame_info.uv_stride = frame->stride[1];

			nv12_frame_info.w = frame->width;
			nv12_frame_info.h = frame->height;
			nv12_frame_info.dx = 0;
			nv12_frame_info.dy = 0;
			nv12_frame_info.dw = frame->width;
			nv12_frame_info.dh = frame->height;
			nv12_frame_info.y_stride = frame->width;
			nv12_frame_info.uv_stride = frame->width;

			yuv420_to_nv12_open(&yuv420_frame_info, &nv12_frame_info);

			omap_bo_cpu_prep(renderTexture->bo, OMAP_GEM_WRITE);
			yuv420_to_nv12_convert(dstPtr, srcPtr, NULL, NULL);
			omap_bo_cpu_fini(renderTexture->bo, OMAP_GEM_WRITE);
		} else if (frame->pixelfmt == FMT_YUV420P) {
			srcPtr[0] = frame->data[0];
			srcPtr[1] = frame->data[1];
			srcPtr[2] = frame->data[2];
			srcPtr[3] = frame->data[3];
			srcStride[0] = frame->stride[0];
			srcStride[1] = frame->stride[1];
			srcStride[2] = frame->stride[2];
			srcStride[3] = frame->stride[3];
			dstPtr[0] = dst;
			dstPtr[1] = dst + frame->width * frame->height;
			dstPtr[2] = nullptr;
			dstPtr[3] = nullptr;
			dstStride[0] = frame->width;
			dstStride[1] = frame->width;
			dstStride[2] = 0;
			dstStride[3] = 0;

			if (!_scaleCtx) {
				_scaleCtx = sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P, frame->width, frame->height,
						AV_PIX_FMT_NV12, SWS_POINT, NULL, NULL, NULL);
				if (!_scaleCtx) {
					log->printf("DisplayOmapDrm::putImage(): Can not create scale context!\n");
					goto fail;
				}
			}
			omap_bo_cpu_prep(renderTexture->bo, OMAP_GEM_WRITE);
			sws_scale(_scaleCtx, srcPtr, srcStride, 0, frame->height, dstPtr, dstStride);
			omap_bo_cpu_fini(renderTexture->bo, OMAP_GEM_WRITE);
		} else {
			log->printf("DisplayOmapDrm::putImage(): Not supported format!\n");
			goto fail;
		}
	}

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, renderTexture->glTexture);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glFlush();

	return S_OK;

fail:
	return S_FAIL;
}

STATUS DisplayOmapDrmEgl::flip(bool skip) {
	gbm_bo *gbmBo;
	DrmFb *drmFb;

	if (!_initialized)
		return S_FAIL;

	if (skip)
		return S_OK;

	eglSwapBuffers(_eglDisplay, _eglSurface);

	gbmBo = gbm_surface_lock_front_buffer(_gbmSurface);
	drmFb = getDrmFb(gbmBo);

	if (drmModeSetPlane(_fd, _planeId, _crtcId,
				drmFb->fbId, 0,
				0, 0, _modeInfo.hdisplay, _modeInfo.vdisplay,
				0, 0, _modeInfo.hdisplay << 16, _modeInfo.vdisplay << 16
				)) {
		log->printf("DisplayOmapDrmEgl::flip(): failed set plane: %s\n", strerror(errno));
		goto fail;
	}

	gbm_surface_release_buffer(_gbmSurface, gbmBo);

	return S_OK;

fail:

	gbm_surface_release_buffer(_gbmSurface, gbmBo);

	return S_FAIL;
}

STATUS DisplayOmapDrmEgl::getHandle(DisplayHandle *handle) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	handle->handle = _fd;

	return S_OK;
};

DisplayOmapDrmEgl::RenderTexture *DisplayOmapDrmEgl::getVideoBuffer(FORMAT_VIDEO pixelfmt, int width, int height) {
	DisplayVideoBuffer buffer;

	if (getVideoBuffer(&buffer, pixelfmt, width, height) != S_OK) {
		return nullptr;
	}

	RenderTexture *renderTexture = (RenderTexture *)buffer.priv;
	renderTexture->db = nullptr;

	return renderTexture;
}

STATUS DisplayOmapDrmEgl::getVideoBuffer(DisplayVideoBuffer *handle, FORMAT_VIDEO pixelfmt, int width, int height) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	uint32_t fourcc;
	uint32_t stride;
	uint32_t fbSize;

	RenderTexture *renderTexture = new RenderTexture;
	memset(renderTexture, 0, sizeof (RenderTexture));

	if (pixelfmt == FMT_YUV420P || pixelfmt == FMT_NV12) {
		fourcc = FOURCC_TI_NV12;
		stride = width;
		fbSize = width * height * 3 / 2;
	} else {
		log->printf("DisplayOmapDrmEgl::getVideoBuffer(): Can not handle pixel format!\n");
		return S_FAIL;
	}

	handle->locked = false;
	handle->bo = renderTexture->bo = omap_bo_new(_omapDevice, fbSize, OMAP_BO_WC);
	handle->boHandle = omap_bo_handle(handle->bo);
	renderTexture->mapPtr = omap_bo_map(handle->bo);

	EGLint attr[] = {
		EGL_GL_VIDEO_FOURCC_TI,      (EGLint)fourcc,
		EGL_GL_VIDEO_WIDTH_TI,       (EGLint)width,
		EGL_GL_VIDEO_HEIGHT_TI,      (EGLint)height,
		EGL_GL_VIDEO_BYTE_SIZE_TI,   (EGLint)omap_bo_size(renderTexture->bo),
		EGL_GL_VIDEO_BYTE_STRIDE_TI, (EGLint)stride,
		EGL_GL_VIDEO_YUV_FLAGS_TI,   EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE | EGLIMAGE_FLAGS_YUV_BT601,
		EGL_NONE
	};

	renderTexture->dmabuf = omap_bo_dmabuf(renderTexture->bo);
	renderTexture->image = eglCreateImageKHR(_eglDisplay, EGL_NO_CONTEXT, EGL_RAW_VIDEO_TI_DMABUF, (EGLClientBuffer)renderTexture->dmabuf, attr);
	if (renderTexture->image == EGL_NO_IMAGE_KHR) {
		log->printf("DisplayOmapDrmEgl::getVideoBuffer(): failed to bind texture, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	glGenTextures(1, &renderTexture->glTexture);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, renderTexture->glTexture);
	if (glGetError() != GL_NO_ERROR) {
		log->printf("DisplayOmapDrmEgl::getVideoBuffer(): failed to bind texture, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, renderTexture->image);
	if (glGetError() != GL_NO_ERROR) {
		log->printf("DisplayOmapDrmEgl::getVideoBuffer(): failed update texture, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	renderTexture->db = handle;
	handle->priv = renderTexture;

	return S_OK;

fail:

	if (renderTexture)
		releaseVideoBuffer(renderTexture);

	return S_FAIL;
};

STATUS DisplayOmapDrmEgl::releaseVideoBuffer(DisplayVideoBuffer *handle) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	RenderTexture *renderTexture = (RenderTexture *)handle->priv;
	if (renderTexture == nullptr || _eglDisplay == nullptr)
		return S_FAIL;

	if (releaseVideoBuffer(renderTexture) != S_OK)
		return S_FAIL;

	handle->bo = nullptr;
	handle->priv = nullptr;

	return S_OK;
};

STATUS DisplayOmapDrmEgl::releaseVideoBuffer(RenderTexture *texture) {
	if (!_initialized || texture == nullptr || _eglDisplay == nullptr)
		return S_FAIL;

	if (texture->image) {
		eglDestroyImageKHR(_eglDisplay, texture->image);
		glDeleteTextures(1, &texture->glTexture);
	}

	if (texture->dmabuf)
		close(texture->dmabuf);

	if (texture->bo)
		omap_bo_del(texture->bo);

	delete texture;

	return S_OK;
};

} // namespace
