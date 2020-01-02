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

#include "display_omapdrm.h"

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

DisplayOmapDrm::DisplayOmapDrm() :
		_fd(-1), _omapDevice(nullptr), _boFB(nullptr), _boVideo(nullptr),
		_drmResources(nullptr),
		_oldCrtc(nullptr),  _drmPlaneResources(nullptr), _connectorId(-1),
		_crtcId(-1), _planeId(-1),
		_fbPtr(nullptr), _fbSize(0), _fbStride(0),
		_fbWidth(0), _fbHeight(0),
		_videoPtr(nullptr), _videoSize(0), _videoStride(0),
		_videoWidth(0), _videoHeight(0),
		_dstX(0), _dstY(0), _dstWidth(0), _dstHeight(0), scaleCtx(nullptr) {
}

DisplayOmapDrm::~DisplayOmapDrm() {
	deinit();
}

STATUS DisplayOmapDrm::init() {
	if (_initialized)
		return S_FAIL;

	if (internalInit() == S_FAIL)
		return S_FAIL;

	return S_OK;
}

STATUS DisplayOmapDrm::deinit() {
	if (!_initialized)
		return S_FAIL;

	internalDeinit();

	return S_OK;
}

STATUS DisplayOmapDrm::internalInit() {
	_fd = drmOpen("omapdrm", NULL);
	if (_fd < 0) {
		log->printf("DisplayOmapDrm::internalInit(): Failed open omapdrm, %s\n", strerror(errno));
		goto fail;
	}

	_omapDevice = omap_device_new(_fd);
	if (!_omapDevice) {
		log->printf("DisplayOmapDrm::internalInit(): Failed create omap device\n");
		goto fail;
	}

	_drmResources = drmModeGetResources(_fd);
	if (!_drmResources) {
		log->printf("DisplayOmapDrm::internalInit(): Failed get DRM resources, %s\n", strerror(errno));
		goto fail;
	}

	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmResources) {
		log->printf("DisplayOmapDrm::internalInit(): Failed get DRM plane resources, %s\n", strerror(errno));
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
	    	connector->connector_type == DRM_MODE_CONNECTOR_HDMIB)
	    {
	    	_connectorId = connector->connector_id;
			drmModeFreeConnector(connector);
	    	break;
	    }
	}

	if (_connectorId == -1) {
		log->printf("DisplayOmapDrm::internalInit(): Failed to find active HDMI connector!\n");
		goto fail;
	}

	_initialized = true;
	return S_OK;

fail:

	if (_drmPlaneResources != nullptr) {
		drmModeFreePlaneResources(_drmPlaneResources);
		_drmPlaneResources = nullptr;
	}

	if (_drmResources != nullptr) {
		drmModeFreeResources(_drmResources);
		_drmResources = nullptr;
	}

	if (_omapDevice != nullptr)
	{
		omap_device_del(_omapDevice);
		_omapDevice = nullptr;
	}
	if (_fd != -1)
	{
		drmClose(_fd);
		_fd = -1;
	}

	return S_FAIL;
}

void DisplayOmapDrm::internalDeinit() {
	if (_initialized == false)
		return;

	if (_fbPtr) {
		omap_bo_cpu_prep(_boFB, OMAP_GEM_WRITE);
		memset(_fbPtr, 0, _fbSize);
		omap_bo_cpu_fini(_boFB, OMAP_GEM_WRITE);
		_fbPtr = nullptr;
	}

	if (_videoPtr) {
		omap_bo_cpu_prep(_boVideo, OMAP_GEM_WRITE);
		memset(_videoPtr, 0, _videoSize);
		omap_bo_cpu_fini(_boVideo, OMAP_GEM_WRITE);
	}

	if (_oldCrtc) {
		drmModeSetCrtc(_fd, _oldCrtc->crtc_id, _oldCrtc->buffer_id,
				_oldCrtc->x, _oldCrtc->y, &_connectorId, 1, &_oldCrtc->mode);
		drmModeFreeCrtc(_oldCrtc);
		_oldCrtc = nullptr;
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

STATUS DisplayOmapDrm::configure(FORMAT_VIDEO videoFmt, int videoFps, int videoWidth, int videoHeight) {
	int modeId = -1;
	_crtcId = -1;

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
		log->printf("DisplayOmapDrm::configure(): Failed to find connector!\n");
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
		U64 hightestArea = 0;
		for (int j = 0; j < connector->count_modes; j++) {
			auto mode = &connector->modes[j];
			const U64 area = mode->hdisplay * mode->vdisplay;
			if ((mode->vrefresh >= videoFps) && (area > hightestArea))
			{
				hightestArea = area;
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
		log->printf("DisplayOmapDrm::configure(): Failed to find suitable display output!\n");
		return S_FAIL;
	}

	_planeId = -1;
	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	for (int i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (plane == nullptr)
			continue;
		if (plane->crtc_id == 0)
		{
			_planeId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}
		drmModeFreePlane(plane);
	}
	if (_planeId == -1) {
		log->printf("DisplayOmapDrm::configure(): Failed to find plane!\n");
		return S_FAIL;
	}

	uint32_t fourcc = 0;
    uint32_t fbId = 0;
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };

	_boFB = omap_bo_new(_omapDevice, _modeInfo.hdisplay * _modeInfo.vdisplay * 4, OMAP_BO_WC | OMAP_BO_SCANOUT);
	if (!_boFB) {
		log->printf("DisplayOmapDrm::configure(): Failed allocate buffer!\n");
		return S_FAIL;
	}

	handles[0] = omap_bo_handle(_boFB);
	pitches[0] = _modeInfo.hdisplay * 4;
    int ret = drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
    		                DRM_FORMAT_ARGB8888,
							handles, pitches, offsets, &fbId, 0);
    if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed add video buffer: %s\n", strerror(errno));
		return S_FAIL;
    }

    _oldCrtc = drmModeGetCrtc(_fd, _crtcId);

	ret = drmModeSetCrtc(_fd, _crtcId, fbId, 0, 0, &_connectorId, 1, &_modeInfo);
    if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed set crtc: %s\n", strerror(errno));
		return S_FAIL;
    }

    uint16_t stride;
	switch (videoFmt)
	{
	case FMT_YUV420P:
	case FMT_NV12:
		fourcc = DRM_FORMAT_NV12;
		break;
	default:
		log->printf("DisplayOmapDrm::configure(): Unsupported input format!\n");
		return S_FAIL;
	}

	U32 bufferSize = 0;
	if (fourcc == DRM_FORMAT_NV12) {
		bufferSize = videoWidth * videoHeight * 3 / 2;
	}
	_boVideo = omap_bo_new(_omapDevice, bufferSize, OMAP_BO_WC | OMAP_BO_SCANOUT);
	if (!_boVideo) {
		log->printf("DisplayOmapDrm::configure(): Failed allocate primary buffer!\n");
		return S_FAIL;
	}
	if (fourcc == DRM_FORMAT_NV12) {
		handles[0] = omap_bo_handle(_boVideo);
		pitches[0] = videoWidth;
		handles[1] = handles[0];
		pitches[1] = pitches[0];
		offsets[1] = videoWidth * videoHeight;
	}
    ret = drmModeAddFB2(_fd, videoWidth, videoHeight,
    					fourcc, handles, pitches, offsets, &fbId, 0);
    if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed add buffer: %s\n", strerror(errno));
		return S_FAIL;
    }
    if (drmModeSetPlane(_fd, _planeId, _crtcId, fbId,
    			    0, 0, 0, _modeInfo.hdisplay, _modeInfo.vdisplay,
    			    0, 0, videoWidth << 16, videoHeight << 16)) {
		log->printf("DisplayOmapDrm::configure(): failed set plane: %s\n", strerror(errno));
		return S_FAIL;
    }

    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(_fd, _planeId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		log->printf("DisplayOmapDrm::configure(): Failed to find properties for plane!\n");
		return S_FAIL;
	}
	for (int i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop != nullptr &&
			strcmp(prop->name, "zorder") == 0 &&
			drm_property_type_is(prop, DRM_MODE_PROP_RANGE))
		{
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _planeId, DRM_MODE_OBJECT_PLANE, props->props[i], 0))
			{
				log->printf("DisplayOmapDrm::configure(): Failed to set zorder property for plane!\n");
				return S_FAIL;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

    _dstWidth = _modeInfo.hdisplay;
	_dstHeight = _modeInfo.vdisplay;

	_fbStride = pitches[0];
	_fbWidth = _dstWidth;
	_fbHeight = _dstHeight;
	_fbSize = omap_bo_size(_boFB);
	_fbPtr = (U8 *)omap_bo_map(_boFB);
	if (_fbPtr == nullptr) {
		log->printf("DisplayOmapDrm::internalInit(): Failed get primary frame buffer!");
		return S_FAIL;
	}

	_videoStride = pitches[0];
	_videoWidth = videoWidth;
	_videoHeight = videoHeight;
	_videoSize = omap_bo_size(_boVideo);
	_videoPtr = (U8 *)omap_bo_map(_boVideo);
	if (_videoPtr == nullptr) {
		log->printf("DisplayOmapDrm::internalInit(): Failed get video frame buffer!");
		return S_FAIL;
	}

	omap_bo_cpu_prep(_boFB, OMAP_GEM_WRITE);
	memset(_fbPtr, 0, _fbSize);
	omap_bo_cpu_fini(_boFB, OMAP_GEM_WRITE);

	omap_bo_cpu_prep(_boVideo, OMAP_GEM_WRITE);
	memset(_videoPtr, 0, _videoSize);
	omap_bo_cpu_fini(_boVideo, OMAP_GEM_WRITE);

	scaleCtx = NULL;

	return S_OK;
}

STATUS DisplayOmapDrm::putImage(VideoFrame *frame) {
	U32 dstX = (_videoWidth - frame->width) / 2;
	U32 dstY = (_videoHeight - frame->height) / 2;

	if (frame == nullptr || frame->data[0] == nullptr) {
		log->printf("DisplayOmapDrm::putImage(): Bad arguments!\n");
		goto fail;
	}

	if (frame->pixelfmt == FMT_YUV420P) {
		uint8_t *srcPtr[3] = { frame->data[0], frame->data[1], frame->data[2] };
		uint8_t *dstPtr[3] = { _videoPtr, _videoPtr + _videoWidth * _videoHeight, NULL };

		yuv420_frame_info.w = ALIGN2(frame->width, 5);
		yuv420_frame_info.h = ALIGN2(frame->height, 5);
		yuv420_frame_info.dx = 0;
		yuv420_frame_info.dy = 0;
		yuv420_frame_info.dw = frame->width;
		yuv420_frame_info.dh = frame->height;
		yuv420_frame_info.y_stride = frame->stride[0];
		yuv420_frame_info.uv_stride = frame->stride[1];

		nv12_frame_info.w = _videoWidth;
		nv12_frame_info.h = _videoHeight;
		nv12_frame_info.dx = 0;
		nv12_frame_info.dy = 0;
		nv12_frame_info.dw = frame->width;
		nv12_frame_info.dh = frame->height;
		nv12_frame_info.y_stride = _videoWidth;
		nv12_frame_info.uv_stride = _videoWidth;

		yuv420_to_nv12_open(&yuv420_frame_info, &nv12_frame_info);

		omap_bo_cpu_prep(_boVideo, OMAP_GEM_WRITE);
		yuv420_to_nv12_convert(dstPtr, srcPtr, NULL, NULL);
		omap_bo_cpu_fini(_boVideo, OMAP_GEM_WRITE);
	} else {
		log->printf("DisplayOmapDrm::putImage(): Can not handle pixel format!\n");
		goto fail;
	}

	return S_OK;

fail:
	return S_FAIL;
}

STATUS DisplayOmapDrm::flip() {

	return S_OK;
}

} // namespace
