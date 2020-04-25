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
		_fd(-1), _omapDevice(nullptr), _drmResources(nullptr),
		_oldCrtc(nullptr),  _drmPlaneResources(nullptr), _connectorId(-1),
		_crtcId(-1), _planeId(-1),
		_currentOSDBuffer(), _currentVideoBuffer(0),
		_scaleCtx(nullptr) {
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

	for (int i = 0; i < NUM_OSD_FB; i++) {
		if (_osdBuffers[i].fbId) {
			drmModeRmFB(_fd, _osdBuffers[i].fbId);
		}
		if (_osdBuffers[i].bo) {
			omap_bo_del(_osdBuffers[i].bo);
		}
		_osdBuffers[i] = {};
	}

	for (int i = 0; i < NUM_VIDEO_FB; i++) {
		if (_videoBuffers[i].fbId) {
			drmModeRmFB(_fd, _videoBuffers[i].fbId);
		}
		if (_videoBuffers[i].bo) {
			omap_bo_del(_videoBuffers[i].bo);
		}
		_videoBuffers[i] = {};
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

	drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(_fd, _planeId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		log->printf("DisplayOmapDrm::configure(): Failed to find properties for plane!\n");
		return S_FAIL;
	}
	for (int i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop != nullptr && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _planeId, DRM_MODE_OBJECT_PLANE, props->props[i], 0)) {
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
	int ret;

	for (int i = 0; i < NUM_OSD_FB; i++) {
		_osdBuffers[i].bo = omap_bo_new(_omapDevice, _modeInfo.hdisplay * _modeInfo.vdisplay * 4, OMAP_BO_WC | OMAP_BO_SCANOUT);
		if (!_osdBuffers[i].bo) {
			log->printf("DisplayOmapDrm::configure(): Failed allocate buffer!\n");
			return S_FAIL;
		}
		handles[0] = omap_bo_handle(_osdBuffers[i].bo);
		pitches[0] = _modeInfo.hdisplay * 4;
		ret = drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
		                	DRM_FORMAT_ARGB8888,
							handles, pitches, offsets, &_osdBuffers[i].fbId, 0);
		if (ret < 0) {
			log->printf("DisplayOmapDrm::configure(): failed add video buffer: %s\n", strerror(errno));
			return S_FAIL;
		}
		_osdBuffers[i].width = _modeInfo.hdisplay;
		_osdBuffers[i].height = _modeInfo.vdisplay;
		_osdBuffers[i].stride = pitches[0];
		_osdBuffers[i].size = omap_bo_size(_osdBuffers[i].bo);
		_osdBuffers[i].ptr = (U8 *)omap_bo_map(_osdBuffers[i].bo);
		if (_osdBuffers[i].ptr == nullptr) {
			log->printf("DisplayOmapDrm::internalInit(): Failed get primary frame buffer!");
			return S_FAIL;
		}
		omap_bo_cpu_prep(_osdBuffers[i].bo, OMAP_GEM_WRITE);
		memset(_osdBuffers[i].ptr, 0, _osdBuffers[i].size);
		omap_bo_cpu_fini(_osdBuffers[i].bo, OMAP_GEM_WRITE);
	}

	_oldCrtc = drmModeGetCrtc(_fd, _crtcId);

	ret = drmModeSetCrtc(_fd, _crtcId, _osdBuffers[0].fbId, 0, 0, &_connectorId, 1, &_modeInfo);
	if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed set crtc: %s\n", strerror(errno));
		return S_FAIL;
	}

	uint16_t stride;
	switch (videoFmt) {
		case FMT_YUV420P:
		case FMT_NV12:
			fourcc = DRM_FORMAT_NV12;
			break;
		default:
			log->printf("DisplayOmapDrm::configure(): Unsupported input format!\n");
			return S_FAIL;
	}

	U32 videoBufferSize = 0;
	if (fourcc == DRM_FORMAT_NV12) {
		videoBufferSize = videoWidth * videoHeight * 3 / 2;
	}
	for (int i = 0; i < NUM_VIDEO_FB; i++) {
		_videoBuffers[i].bo = omap_bo_new(_omapDevice, videoBufferSize, OMAP_BO_WC | OMAP_BO_SCANOUT);
		if (!_videoBuffers[i].bo) {
			log->printf("DisplayOmapDrm::configure(): Failed allocate primary buffer!\n");
			return S_FAIL;
		}
		if (fourcc == DRM_FORMAT_NV12) {
			handles[0] = omap_bo_handle(_videoBuffers[i].bo);
			pitches[0] = videoWidth;
			handles[1] = handles[0];
			pitches[1] = pitches[0];
			offsets[1] = videoWidth * videoHeight;
		}
		ret = drmModeAddFB2(_fd, videoWidth, videoHeight,
						fourcc, handles, pitches, offsets, &_videoBuffers[i].fbId, 0);
		if (ret < 0) {
			log->printf("DisplayOmapDrm::configure(): failed add buffer: %s\n", strerror(errno));
			return S_FAIL;
		}
		_videoBuffers[i].dstX = 0;
		_videoBuffers[i].dstY = 0;
		_videoBuffers[i].dstWidth = videoWidth;
		_videoBuffers[i].dstHeight = videoHeight;
		_videoBuffers[i].stride = pitches[0];
		_videoBuffers[i].width = videoWidth;
		_videoBuffers[i].height = videoHeight;
		_videoBuffers[i].size = omap_bo_size(_videoBuffers[i].bo);
		_videoBuffers[i].ptr = omap_bo_map(_videoBuffers[i].bo);
		if (_videoBuffers[i].ptr == nullptr) {
			log->printf("DisplayOmapDrm::internalInit(): Failed get video frame buffer!");
			return S_FAIL;
		}
		omap_bo_cpu_prep(_videoBuffers[i].bo, OMAP_GEM_WRITE);
		memset(_videoBuffers[i].ptr, 0, _videoBuffers[i].size);
		omap_bo_cpu_fini(_videoBuffers[i].bo, OMAP_GEM_WRITE);
	}

	_scaleCtx = nullptr;

	_currentOSDBuffer = NUM_OSD_FB - 1;
	_currentVideoBuffer = 0;

	return S_OK;

fail:

	for (int i = 0; i < NUM_OSD_FB; i++) {
		if (_osdBuffers[i].fbId) {
			drmModeRmFB(_fd, _osdBuffers[i].fbId);
		}
		if (_osdBuffers[i].bo) {
			omap_bo_del(_osdBuffers[i].bo);
		}
		_osdBuffers[i] = {};
	}

	for (int i = 0; i < NUM_VIDEO_FB; i++) {
		if (_videoBuffers[i].fbId) {
			drmModeRmFB(_fd, _videoBuffers[i].fbId);
		}
		if (_videoBuffers[i].bo) {
			omap_bo_del(_videoBuffers[i].bo);
		}
		_videoBuffers[i] = {};
	}

	return S_FAIL;
}

STATUS DisplayOmapDrm::putImage(VideoFrame *frame) {
	if (frame == nullptr || frame->data[0] == nullptr) {
		log->printf("DisplayOmapDrm::putImage(): Bad arguments!\n");
		goto fail;
	}

	if (!frame->hw) {
		uint8_t *srcPtr[4] = {};
		uint8_t *dstPtr[4] = {};
		int srcStride[4] = {};
		int dstStride[4] = {};

		uint8_t *dst = (uint8_t *)_videoBuffers[_currentVideoBuffer].ptr;
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

			omap_bo_cpu_prep(_videoBuffers[_currentVideoBuffer].bo, OMAP_GEM_WRITE);
			yuv420_to_nv12_convert(dstPtr, srcPtr, NULL, NULL);
			omap_bo_cpu_fini(_videoBuffers[_currentVideoBuffer].bo, OMAP_GEM_WRITE);
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
			omap_bo_cpu_prep(_videoBuffers[_currentVideoBuffer].bo, OMAP_GEM_WRITE);
			sws_scale(_scaleCtx, srcPtr, srcStride, 0, frame->height, dstPtr, dstStride);
			omap_bo_cpu_fini(_videoBuffers[_currentVideoBuffer].bo, OMAP_GEM_WRITE);
		} else {
			log->printf("DisplayOmapDrm::putImage(): Not supported format!\n");
			goto fail;
		}
	}

	return S_OK;

fail:
	return S_FAIL;
}

void DisplayOmapDrm::pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
	DisplayOmapDrm *context = (DisplayOmapDrm *)data;
}

STATUS DisplayOmapDrm::flip() {
	drmEventContext drmEvent{};
	drmEventContext drmEventContext{};

	if (!_initialized)
		return S_FAIL;

	if (drmModeSetPlane(_fd, _planeId, _crtcId,
				_videoBuffers[_currentVideoBuffer].fbId, 0,
				0, 0, _modeInfo.hdisplay, _modeInfo.vdisplay,
				_videoBuffers[_currentVideoBuffer].dstX << 16,
				_videoBuffers[_currentVideoBuffer].dstY << 16,
				_videoBuffers[_currentVideoBuffer].dstWidth << 16,
				_videoBuffers[_currentVideoBuffer].dstHeight << 16
				)) {
		log->printf("DisplayOmapDrm::flip(): failed set plane: %s\n", strerror(errno));
		goto fail;
	}
	if (++_currentVideoBuffer >= NUM_VIDEO_FB)
		_currentVideoBuffer = 0;

	if (++_currentOSDBuffer >= NUM_OSD_FB)
		_currentOSDBuffer = 0;
	if (drmModePageFlip(_fd, _crtcId, _osdBuffers[_currentOSDBuffer++].fbId, DRM_MODE_PAGE_FLIP_EVENT, this)) {
		log->printf("DisplayOmapDrm::flip(): Can not flip buffer! %s\n", strerror(errno));
		goto fail;
	}

	drmEventContext.version = 2;
	drmEventContext.page_flip_handler = pageFlipHandler;
	drmHandleEvent(_fd, &drmEventContext);

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

STATUS DisplayOmapDrm::getVideoBuffer(DisplayVideoBuffer *handle, FORMAT_VIDEO pixelfmt, int width, int height) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	uint32_t fourcc;
	uint32_t stride;
	uint32_t fbSize;

	handle->bo = omap_bo_new(_omapDevice, fbSize, OMAP_BO_WC);
	handle->boHandle = omap_bo_handle(handle->bo);

	handle->priv = nullptr;

	return S_OK;

fail:

	return S_FAIL;
};

STATUS DisplayOmapDrm::releaseVideoBuffer(DisplayVideoBuffer *handle) {
	if (!_initialized || handle == nullptr)
		return S_FAIL;

	close(handle->boHandle);
	omap_bo_del(handle->bo);

	handle->bo = nullptr;
	handle->priv = nullptr;

	return S_OK;
};

} // namespace
