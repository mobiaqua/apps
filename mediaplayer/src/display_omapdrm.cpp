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

namespace MediaPLayer {

DisplayOmapDrm::DisplayOmapDrm() :
		_fd(-1), _omapDevice(nullptr),
		_drmBuffers{}, _drmBufIdx(0),
		_drmResources(nullptr),
		_oldCrtc(nullptr),  _drmPlaneResources(nullptr), _connectorId(-1),
		_crtcId(-1), _planeId(-1),
		_fbStride(0), _fbWidth(0), _fbHeight(0),
		_scaleCtx (0){
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
			connector->connector_type == DRM_MODE_CONNECTOR_HDMIB) {
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

void DisplayOmapDrm::internalDeinit() {
	if (_initialized == false)
		return;

	for (int i = 0; i < NUM_DRM_BUFFERS; i++) {
		if (_drmBuffers[i].bo) {
			omap_bo_cpu_prep(_drmBuffers[i].bo, OMAP_GEM_WRITE);
			memset(_drmBuffers[i].ptr, 0, omap_bo_size(_drmBuffers[i].bo));
			omap_bo_cpu_fini(_drmBuffers[i].bo, OMAP_GEM_WRITE);
		}
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

	switch (videoFmt)
	{
	case FMT_YUV420P:
	case FMT_NV12:
		break;
	default:
		log->printf("DisplayOmapDrm::configure(): Unsupported input format!\n");
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
		log->printf("DisplayOmapDrm::configure(): Failed to find suitable display output!\n");
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
		log->printf("DisplayOmapDrm::configure(): Failed to find plane!\n");
		return S_FAIL;
	}

	uint32_t handles[4] = {}, pitches[4] = {}, offsets[4] = {};

	for (int i = 0; i < NUM_DRM_BUFFERS; i++) {
		_drmBuffers[i].bo = omap_bo_new(_omapDevice, _modeInfo.hdisplay * _modeInfo.vdisplay * 4, OMAP_BO_WC | OMAP_BO_SCANOUT);
		if (!_drmBuffers[i].bo) {
			log->printf("DisplayOmapDrm::configure(): Failed allocate buffer!\n");
			return S_FAIL;
		}
		_drmBuffers[i].ptr = (U8 *)omap_bo_map(_drmBuffers[i].bo);
		if (_drmBuffers[i].ptr == nullptr) {
			log->printf("DisplayOmapDrm::configure(): Failed get OSD frame buffer!");
			return S_FAIL;
		}
		handles[i] = omap_bo_handle(_drmBuffers[i].bo);
		pitches[i] = _modeInfo.hdisplay * 4;
		int ret = drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
			                	DRM_FORMAT_ARGB8888,
								handles, pitches, offsets, &_drmBuffers[i].fbId, 0);
		if (ret < 0) {
			log->printf("DisplayOmapDrm::configure(): failed add video buffer: %s\n", strerror(errno));
			return S_FAIL;
		}
	}

	_oldCrtc = drmModeGetCrtc(_fd, _crtcId);
	int ret = drmModeSetCrtc(_fd, _crtcId, _drmBuffers[_drmBufIdx].fbId, 0, 0, &_connectorId, 1, &_modeInfo);
	if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed set crtc: %s\n", strerror(errno));
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

	_fbWidth = _modeInfo.hdisplay;
	_fbHeight = _modeInfo.vdisplay;
	_fbStride = pitches[0];

	for (int i = 0; i < NUM_DRM_BUFFERS; i++) {
		omap_bo_cpu_prep(_drmBuffers[i].bo, OMAP_GEM_WRITE);
		memset(_drmBuffers[i].ptr, 0, omap_bo_size(_drmBuffers[i].bo));
		omap_bo_cpu_fini(_drmBuffers[i].bo, OMAP_GEM_WRITE);
	}

	return S_OK;
}

STATUS DisplayOmapDrm::putImage(VideoFrame *frame) {
	U32 dstX = (_fbWidth - frame->width) / 2;
	U32 dstY = (_fbHeight - frame->height) / 2;

	if (frame == nullptr || frame->data[0] == nullptr) {
		log->printf("DisplayOmapDrm::putImage(): Bad arguments!\n");
		goto fail;
	}

	_drmBufIdx = ++_drmBufIdx & 1;
	if (frame->pixelfmt == FMT_YUV420P) {
		uint8_t *srcPtr[4] = { frame->data[0], frame->data[1], frame->data[2], NULL };
		int srcStride[4] = {
				static_cast<int>(frame->stride[0]),
				static_cast<int>(frame->stride[1]),
				static_cast<int>(frame->stride[2]),
				0
		};
		uint8_t *dstPtr[4] = {
				_drmBuffers[_drmBufIdx].ptr + _fbStride * dstY + dstX * 4,
				NULL,
				NULL,
				NULL
		};
		int dstStride[4] = {
				static_cast<int>(_fbStride),
				0,
				0,
				0
		};

		if (!_scaleCtx) {
			_scaleCtx = sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P, frame->width, frame->height,
										AV_PIX_FMT_RGB32, SWS_POINT, NULL, NULL, NULL);
			if (!_scaleCtx) {
				log->printf("DisplayOmapDrm::putImage(): Can not create scale context!\n");
				goto fail;
			}
		}
		omap_bo_cpu_prep(_drmBuffers[_drmBufIdx].bo, OMAP_GEM_WRITE);
		sws_scale(_scaleCtx, srcPtr, srcStride, 0, frame->height, dstPtr, dstStride);
		omap_bo_cpu_fini(_drmBuffers[_drmBufIdx].bo, OMAP_GEM_WRITE);
	} else {
		log->printf("DisplayOmapDrm::putImage(): Can not handle pixel format!\n");
		goto fail;
	}

	return S_OK;

fail:
	return S_FAIL;
}

static int waiting_for_flip;

static void pageFlipHandler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	DisplayOmapDrm *ctx = (DisplayOmapDrm *)data;
	waiting_for_flip = 0;
}

STATUS DisplayOmapDrm::flip() {
	int result = drmModePageFlip(_fd, _crtcId, _drmBuffers[_drmBufIdx].fbId, DRM_MODE_PAGE_FLIP_EVENT, this);
	if (result) {
		log->printf("DisplayOmapDrm::flip(): Can not flip buffer! %s\n", strerror(errno));
		goto fail;
	}

	waiting_for_flip = 1;
	while (waiting_for_flip) {
		drmEventContext drmEvent{};
		drmEventContext drmEventContext{};
		drmEventContext.version = DRM_EVENT_CONTEXT_VERSION;
		drmEventContext.page_flip_handler = pageFlipHandler;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(_fd, &fds);
		struct timeval timeout = {
			.tv_sec = 3,
			.tv_usec = 0,
			};

		result = select(_fd + 1, &fds, NULL, NULL, &timeout);
		if (result <= 0) {
			if (errno == EAGAIN) {
				continue;
			} else {
				log->printf("DisplayOmapDrm::putImage(): Timeout on flip buffer!\n");
				goto fail;
			}
		}
		drmHandleEvent(_fd, &drmEventContext);
	}

	return S_OK;

fail:
	return S_FAIL;
}

} // namespace
