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

#include "display_omapdrm.h"

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
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
		_fd(-1), _drmResources(nullptr),
		_oldCrtc(nullptr),  _drmPlaneResources(nullptr), _connectorId(-1),
		_crtcId(-1), _osdPlaneId(-1), _videoPlaneId(-1),
		_primaryHandle(0), _primaryFbId(0), _primarySize(0), _primaryPtr(nullptr),
		_currentOSDBuffer(), _currentVideoBuffer(0),
		_scaleCtx(nullptr) {
}

DisplayOmapDrm::~DisplayOmapDrm() {
	deinit();
}

STATUS DisplayOmapDrm::init(bool hwAccelDecode) {
	if (_initialized)
		return S_FAIL;

	_hwAccelDecode = hwAccelDecode;

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
	drmDevice *devices[DRM_MAX_MINOR] = { 0 };

	int card_count = drmGetDevices2(0, devices, SIZE_OF_ARRAY(devices));
	for (int i = 0; i < card_count; i++) {
		drmDevice *dev = devices[i];
		if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY))) {
			continue;
		}
		_fd = open(dev->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
		_drmResources = drmModeGetResources(_fd);
		if (!_drmResources) {
			close(_fd);
			_fd = -1;
			continue;
		}
		break;
	}
	if (_fd < 0) {
		log->printf("DisplayOmapDrm::internalInit(): Failed open, %s\n", strerror(errno));
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

	if (_fd != -1) {
		drmClose(_fd);
		_fd = -1;
	}

	return S_FAIL;
}

void DisplayOmapDrm::internalDeinit() {
	if (_initialized == false)
		return;

	for (int i = 0; i < NUM_OSD_FB; i++) {
		if (_osdBuffers[i].fbId) {
			drmModeRmFB(_fd, _osdBuffers[i].fbId);
		}
		if (_osdBuffers[i].ptr) {
			munmap(_osdBuffers[i].ptr, _osdBuffers[i].size);
		}
		if (_osdBuffers[i].handle > 0) {
			struct drm_mode_destroy_dumb dreq = {
				.handle = _osdBuffers[i].handle,
			};
			drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		}
		_osdBuffers[i] = {};
	}

	if (!_hwAccelDecode) {
		for (int i = 0; i < NUM_VIDEO_FB; i++) {
			if (_videoBuffers[i] && _videoBuffers[i]->fbId) {
				drmModeRmFB(_fd, _videoBuffers[i]->fbId);
			}
			if (_videoBuffers[i]->ptr) {
				munmap(_videoBuffers[i]->ptr, _videoBuffers[i]->size);
			}
			if (_videoBuffers[i]->handle > 0) {
				struct drm_mode_destroy_dumb dreq = {
					.handle = _videoBuffers[i]->handle,
				};
				drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
			}
			_videoBuffers[i] = {};
		}
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
	if (_primaryPtr) {
		munmap(_primaryPtr, _primarySize);
	}
	if (_primaryHandle > 0) {
		struct drm_mode_destroy_dumb dreq = {
			.handle = _primaryHandle,
		};
		drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	}

	if (_drmPlaneResources != nullptr)
		drmModeFreePlaneResources(_drmPlaneResources);

	if (_drmResources != nullptr)
		drmModeFreeResources(_drmResources);

	if (_fd != -1)
		drmClose(_fd);

	_initialized = false;
}

STATUS DisplayOmapDrm::configure(FORMAT_VIDEO videoFmt, int videoFps, int videoWidth, int videoHeight) {
	int modeId = -1;
	_crtcId = -1;

	if (_initialized == false)
		return S_FAIL;

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

	for (int i = 0; i < connector->count_encoders; i++) {
		auto encoder = drmModeGetEncoder(_fd, connector->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id) {
			_crtcId = encoder->crtc_id;
			drmModeFreeEncoder(encoder);
			break;
		}
		drmModeFreeEncoder(encoder);
	}

	if (modeId == -1 || _crtcId == -1) {
		log->printf("DisplayOmapDrm::configure(): Failed to find suitable display output!\n");
		drmModeFreeConnector(connector);
		return S_FAIL;
	}

	_modeInfo = connector->modes[modeId];

	drmModeFreeConnector(connector);

	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmPlaneResources) {
		log->printf("DisplayOmapDrm::configure(): Failed to plane resources!\n");
		return S_FAIL;
	}

	_osdPlaneId = -1;
	for (int i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (plane == nullptr)
			continue;
		if (plane->crtc_id == 0) {
			_osdPlaneId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}
		drmModeFreePlane(plane);
	}
	if (_osdPlaneId == -1) {
		log->printf("DisplayOmapDrm::configure(): Failed to find plane!\n");
		return S_FAIL;
	}
	_videoPlaneId = -1;
	for (int i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (plane == nullptr)
			continue;
		if (plane->crtc_id == 0 && plane->plane_id != _osdPlaneId) {
			_videoPlaneId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}
		drmModeFreePlane(plane);
	}
	if (_videoPlaneId == -1) {
		log->printf("DisplayOmapDrm::configure(): Failed to find plane!\n");
		return S_FAIL;
	}

	drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(_fd, _osdPlaneId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		log->printf("DisplayOmapDrm::configure(): Failed to find properties for plane!\n");
		return S_FAIL;
	}
	for (int i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop != nullptr && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _osdPlaneId, DRM_MODE_OBJECT_PLANE, props->props[i], 1)) {
				log->printf("DisplayOmapDrm::configure(): Failed to set zorder property for plane!\n");
				return S_FAIL;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

	props = drmModeObjectGetProperties(_fd, _videoPlaneId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		log->printf("DisplayOmapDrm::configure(): Failed to find properties for plane!\n");
		return S_FAIL;
	}
	for (int i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop != nullptr && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _videoPlaneId, DRM_MODE_OBJECT_PLANE, props->props[i], 0)) {
				log->printf("DisplayOmapDrm::configure(): Failed to set zorder property for plane!\n");
				return S_FAIL;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

	log->printf("Using display HDMI output: %dx%d@%d\n", _modeInfo.hdisplay, _modeInfo.vdisplay, _modeInfo.vrefresh);

	uint32_t fourcc = 0;
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	U32 videoBufferSize = 0;
	int ret;

	struct drm_mode_create_dumb creq = {
		.height = _modeInfo.vdisplay,
		.width = _modeInfo.hdisplay,
		.bpp = 32,
	};

	if (drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		log->printf("DisplayOmapDrm::configure(): Cannot create dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	_primaryHandle = handles[0] = creq.handle;
	pitches[0] = creq.pitch;
	ret = drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
	                	DRM_FORMAT_ARGB8888,
						handles, pitches, offsets, &_primaryFbId, 0);
	if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed add video buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	struct drm_mode_map_dumb mreq = {
		.handle = creq.handle,
	};
	if (drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
		log->printf("DisplayOmapDrm::configure(): Cannot map dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	_primaryPtr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, mreq.offset);
	if (_primaryPtr == MAP_FAILED) {
		log->printf("DisplayOmapDrm::configure(): Cannot map dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	memset(_primaryPtr, 0, creq.size);

	_oldCrtc = drmModeGetCrtc(_fd, _crtcId);
	ret = drmModeSetCrtc(_fd, _crtcId, _primaryFbId, 0, 0, &_connectorId, 1, &_modeInfo);
	if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed set crtc: %s\n", strerror(errno));
		return S_FAIL;
	}

	for (int i = 0; i < NUM_OSD_FB; i++) {
		if (drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
			log->printf("DisplayOmapDrm::configure(): Cannot create dumb buffer: %s\n", strerror(errno));
			return S_FAIL;
		}
		_osdBuffers[i].handle = handles[0] = creq.handle;
		pitches[0] = creq.pitch;

		ret = drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
		                    DRM_FORMAT_ARGB8888,
		                    handles, pitches, offsets, &_osdBuffers[i].fbId, 0);
		if (ret < 0) {
			log->printf("DisplayOmapDrm::configure(): failed add video buffer: %s\n", strerror(errno));
			return S_FAIL;
		}

		_osdBuffers[i].ptr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, mreq.offset);
		if (_osdBuffers[i].ptr == MAP_FAILED) {
			log->printf("DisplayOmapDrm::configure(): Cannot map dumb buffer: %s\n", strerror(errno));
			return S_FAIL;
		}

		_osdBuffers[i].width = _modeInfo.hdisplay;
		_osdBuffers[i].height = _modeInfo.vdisplay;
		_osdBuffers[i].stride = pitches[0];
		_osdBuffers[i].size = creq.size;
		if (_osdBuffers[i].ptr == nullptr) {
			log->printf("DisplayOmapDrm::internalInit(): Failed get primary frame buffer!");
			return S_FAIL;
		}
		memset(_osdBuffers[i].ptr, 0, _osdBuffers[i].size);
	}

	if (!_hwAccelDecode) {
		for (int i = 0; i < NUM_VIDEO_FB; i++) {
			_videoBuffers[i] = getVideoBuffer(FMT_NV12, videoWidth, videoHeight);
		}
	}

	_scaleCtx = nullptr;

	_currentOSDBuffer = 0;
	_currentVideoBuffer = 0;

	return S_OK;

fail:

	for (int i = 0; i < NUM_OSD_FB; i++) {
		if (_osdBuffers[i].fbId) {
			drmModeRmFB(_fd, _osdBuffers[i].fbId);
		}
		if (_osdBuffers[i].ptr) {
			munmap(_osdBuffers[i].ptr, _osdBuffers[i].size);
		}
		if (_osdBuffers[i].handle > 0) {
			drmIoctl(_fd, DRM_IOCTL_GEM_CLOSE, &_osdBuffers[i].handle);
		}
		_osdBuffers[i] = {};
	}

	for (int i = 0; i < NUM_VIDEO_FB; i++) {
		if (_videoBuffers[i] && _videoBuffers[i]->fbId) {
			drmModeRmFB(_fd, _videoBuffers[i]->fbId);
		}
		if (_videoBuffers[i]->ptr) {
			munmap(_videoBuffers[i]->ptr, _videoBuffers[i]->size);
		}
		if (_videoBuffers[i]->handle > 0) {
			struct drm_mode_destroy_dumb dreq = {
				.handle = _videoBuffers[i]->handle,
			};
			drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		}
		_videoBuffers[i] = {};
	}

	if (_primaryFbId) {
		drmModeRmFB(_fd, _primaryFbId);
	}
	if (_primaryPtr) {
		munmap(_primaryPtr, _primarySize);
	}
	if (_primaryHandle > 0) {
		struct drm_mode_destroy_dumb dreq = {
			.handle = _primaryHandle,
		};
		drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	}

	return S_FAIL;
}

STATUS DisplayOmapDrm::putImage(VideoFrame *frame, bool skip) {
	if (frame == nullptr || frame->data[0] == nullptr) {
		log->printf("DisplayOmapDrm::putImage(): Bad arguments!\n");
		goto fail;
	}

	if (_hwAccelDecode) {
		DisplayVideoBuffer *db = (DisplayVideoBuffer *)(frame->data[0]);
		db->locked = true;
		_videoBuffers[_currentVideoBuffer] = (VideoBuffer *)db->priv;
	} else {
		uint8_t *srcPtr[4] = {};
		uint8_t *dstPtr[4] = {};
		int srcStride[4] = {};
		int dstStride[4] = {};

		uint8_t *dst = (uint8_t *)_videoBuffers[_currentVideoBuffer]->ptr;
		if (0 && frame->pixelfmt == FMT_YUV420P && (ALIGN2(frame->width, 5) == frame->width)) {
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

//			yuv420_to_nv12_open(&yuv420_frame_info, &nv12_frame_info);

//			yuv420_to_nv12_convert(dstPtr, srcPtr, NULL, NULL);
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
			sws_scale(_scaleCtx, srcPtr, srcStride, 0, frame->height, dstPtr, dstStride);
		} else {
			log->printf("DisplayOmapDrm::putImage(): Not supported format!\n");
			goto fail;
		}
	}

	float x, y, w, h;
	if (frame->anistropicDVD) {
		x = 0;
		y = 0;
		w = _modeInfo.hdisplay;
		h = _modeInfo.vdisplay;
	} else {
		float rw = (float)(frame->dw) / _modeInfo.hdisplay;
		float rh = (float)(frame->dh) / _modeInfo.vdisplay;
		if (rw >= rh) {
			w = _modeInfo.hdisplay;
			h = _modeInfo.vdisplay * (rh / rw);
			x = 0;
			y = (_modeInfo.vdisplay - h) / 2;
		} else {
			w = _modeInfo.hdisplay * (rw / rh);
			h = _modeInfo.vdisplay;
			x = (_modeInfo.hdisplay - w) / 2;
			y = 0;
		}
	}

	_videoBuffers[_currentVideoBuffer]->srcX = frame->dx;
	_videoBuffers[_currentVideoBuffer]->srcY = frame->dy;
	_videoBuffers[_currentVideoBuffer]->srcWidth = frame->dw;
	_videoBuffers[_currentVideoBuffer]->srcHeight = frame->dh;

	_videoBuffers[_currentVideoBuffer]->dstX = x;
	_videoBuffers[_currentVideoBuffer]->dstY = y;
	_videoBuffers[_currentVideoBuffer]->dstWidth = w;
	_videoBuffers[_currentVideoBuffer]->dstHeight = h;

	return S_OK;

fail:
	return S_FAIL;
}

STATUS DisplayOmapDrm::flip(bool skip) {
	if (!_initialized)
		return S_FAIL;

	if (!skip && drmModeSetPlane(_fd, _videoPlaneId, _crtcId,
	                             _videoBuffers[_currentVideoBuffer]->fbId, 0,
	                             _videoBuffers[_currentVideoBuffer]->dstX,
	                             _videoBuffers[_currentVideoBuffer]->dstY,
	                             _videoBuffers[_currentVideoBuffer]->dstWidth,
	                             _videoBuffers[_currentVideoBuffer]->dstHeight,
	                             _videoBuffers[_currentVideoBuffer]->srcX << 16,
	                             _videoBuffers[_currentVideoBuffer]->srcY << 16,
	                             _videoBuffers[_currentVideoBuffer]->srcWidth << 16,
	                             _videoBuffers[_currentVideoBuffer]->srcHeight << 16
	                            )) {
		log->printf("DisplayOmapDrm::flip(): failed set plane: %s\n", strerror(errno));
		goto fail;
	}
	if (++_currentVideoBuffer >= NUM_VIDEO_FB)
		_currentVideoBuffer = 0;
	if (_videoBuffers[_currentVideoBuffer] &&
		_videoBuffers[_currentVideoBuffer]->db) {
		_videoBuffers[_currentVideoBuffer]->db->locked = false;
	}


	if (drmModeSetPlane(_fd, _osdPlaneId, _crtcId,
	                    _osdBuffers[_currentOSDBuffer].fbId, 0,
	                    0, 0, _modeInfo.hdisplay, _modeInfo.vdisplay,
	                    0, 0, _modeInfo.hdisplay << 16, _modeInfo.vdisplay << 16
	                   )) {
		log->printf("DisplayOmapDrm::flip(): failed set plane: %s\n", strerror(errno));
		goto fail;
	}
	if (++_currentOSDBuffer >= NUM_OSD_FB)
		_currentOSDBuffer = 0;

	return S_OK;

fail:

	return S_FAIL;
}

STATUS DisplayOmapDrm::getHandle(DisplayHandle *handle) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	handle->handle = _fd;

	return S_OK;
};

DisplayOmapDrm::VideoBuffer *DisplayOmapDrm::getVideoBuffer(FORMAT_VIDEO pixelfmt, int width, int height) {
	DisplayVideoBuffer buffer;

	if (getDisplayVideoBuffer(&buffer, pixelfmt, width, height) != S_OK) {
		return nullptr;
	}

	VideoBuffer *videoBuffer = (VideoBuffer *)buffer.priv;
	videoBuffer->db = nullptr;

	return videoBuffer;
}

STATUS DisplayOmapDrm::getDisplayVideoBuffer(DisplayVideoBuffer *handle, FORMAT_VIDEO pixelfmt, int width, int height) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	uint32_t fbSize;

	VideoBuffer *videoBuffer = new VideoBuffer;
	memset(videoBuffer, 0, sizeof(VideoBuffer));

	fbSize = width * height * 3 / 2;
	handle->locked = 0;

	struct drm_mode_create_dumb creq = {
		.height = fbSize,
		.width = 1,
		.bpp = 8,
	};

	if (drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		log->printf("DisplayOmapDrm::getVideoBuffer(): Cannot create dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	struct drm_mode_map_dumb mreq = {
		.handle = creq.handle,
	};
	if (drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
		log->printf("DisplayOmapDrm::getVideoBuffer(): Cannot map dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	void *map = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, mreq.offset);
	if (map == MAP_FAILED) {
		log->printf("DisplayOmapDrm::getVideoBuffer(): Cannot map dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	if (drmPrimeHandleToFD(_fd, creq.handle, DRM_CLOEXEC, &handle->dmaBuf)) {
		log->printf("DisplayOmapDrm::getVideoBuffer(): Cannot exports a dma-buf: %s\n", strerror(errno));
		return S_FAIL;
	}

	handles[0] = creq.handle;
	pitches[0] = width;
	handles[1] = handles[0];
	pitches[1] = pitches[0];
	offsets[1] = width * height;
	if (drmModeAddFB2(_fd, width, height,
	                  DRM_FORMAT_NV12, handles, pitches, offsets, &videoBuffer->fbId, 0) < 0) {
		log->printf("DisplayOmapDrm::getVideoBuffer(): failed add video buffer: %s\n", strerror(errno));
		return S_FAIL;
	}

	videoBuffer->handle = creq.handle;
	videoBuffer->srcX = 0;
	videoBuffer->srcY = 0;
	videoBuffer->srcWidth = width;
	videoBuffer->srcHeight = height;
	videoBuffer->stride = pitches[0];
	videoBuffer->width = width;
	videoBuffer->height = height;
	videoBuffer->dstX = 0;
	videoBuffer->dstY = 0;
	videoBuffer->dstWidth = width;
	videoBuffer->dstHeight = height;
	videoBuffer->dmaBuf = handle->dmaBuf;
	videoBuffer->size = fbSize;
	videoBuffer->ptr = map;
	videoBuffer->db = handle;
	handle->priv = videoBuffer;

	return S_OK;

fail:

	return S_FAIL;
};

STATUS DisplayOmapDrm::releaseDisplayVideoBuffer(DisplayVideoBuffer *handle) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	VideoBuffer *videoBuffer = (VideoBuffer *)handle->priv;
	if (videoBuffer == nullptr)
		return S_FAIL;

	if (releaseVideoBuffer(videoBuffer) != S_OK)
		return S_FAIL;

	handle->handle = 0;
	handle->priv = nullptr;

	return S_OK;
};

STATUS DisplayOmapDrm::releaseVideoBuffer(VideoBuffer *buffer) {
	if (!_initialized || buffer == nullptr)
		return S_FAIL;

	drmModeRmFB(_fd, buffer->fbId);

	munmap(buffer->ptr, buffer->size);

	close(buffer->dmaBuf);

	struct drm_gem_close req = {
		.handle = buffer->handle,
	};
	drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);

	delete buffer;

	return S_OK;
};

} // namespace
