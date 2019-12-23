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
		_fd(-1), _omapDevice(nullptr), _drmResources(nullptr), _oldCrtc(nullptr),
		 _drmPlaneResources(nullptr), _connectorId(-1), _crtcId(-1),
		 _boFlags(0),
		 _fbPtr(nullptr), _fbSize(0), _fbStride(0),
		_fbWidth(0), _fbHeight(0), _dstX(0), _dstY(0),
		_dstWidth(0), _dstHeight(0) {
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

STATUS DisplayOmapDrm::configure(FORMAT_VIDEO videoFmt, int videoFps) {
	int modeId = -1;
	_crtcId = -1;

	_boFlags |= OMAP_BO_TILED;

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


	struct drm_mode_create_dumb creq;
	creq.width = _modeInfo.hdisplay;
	creq.height = _modeInfo.vdisplay;
	creq.bpp = 32;

    uint64_t has_dumb = 0;
    if (drmGetCap(_fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 ||
        has_dumb == 0) {
		log->printf("DisplayOmapDrm::configure(): lack of dumb buffer support\n");
		return S_FAIL;
    }
    if (drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		log->printf("DisplayOmapDrm::configure(): failed to create dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
    }

    uint32_t fbId = 0;
    uint32_t handles[4] = { creq.handle, 0, 0, 0 };
    uint32_t pitches[4] = { creq.pitch, 0, 0, 0 };
    uint32_t offsets[4] = { 0, 0, 0, 0 };
    int ret = drmModeAddFB2(_fd, creq.width, creq.height,
    						DRM_FORMAT_XRGB8888,
							handles, pitches, offsets, &fbId, 0);
    if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed add dumb buffer: %s\n", strerror(errno));
		return S_FAIL;
    }

    _oldCrtc = drmModeGetCrtc(_fd, _crtcId);

	ret = drmModeSetCrtc(_fd, _crtcId, fbId, 0, 0, &_connectorId, 1, &_modeInfo);
    if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed set crtc: %s\n", strerror(errno));
		return S_FAIL;
    }

	_dstWidth = _modeInfo.hdisplay;
	_dstHeight = _modeInfo.vdisplay;

	drm_mode_map_dumb mreq;
	mreq.handle = creq.handle;
	ret = drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret < 0) {
		log->printf("DisplayOmapDrm::configure(): failed map dump buffer: %s\n", strerror(errno));
		return S_FAIL;
    }

	_fbStride = creq.pitch;
	_fbWidth = creq.width;
	_fbHeight = creq.height;
	_fbSize = creq.size;

	_fbPtr = (U8 *)mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, mreq.offset);
	if (_fbPtr == reinterpret_cast<void *>(-1)) {
		log->printf("DisplayOmapDrm::internalInit(): Failed get frame buffer! %s\n", strerror(errno));
		return S_FAIL;
	}

	memset(_fbPtr, 0, _fbSize);
	scaleCtx = NULL;

	return S_OK;
}

STATUS DisplayOmapDrm::putImage(VideoFrame *frame) {
	U32 dstX = (_dstWidth - frame->width) / 2;
	U32 dstY = (_dstHeight - frame->height) / 2;

	if (frame == nullptr || frame->data[0] == nullptr) {
		log->printf("DisplayOmapDrm::putImage(): Bad arguments!\n");
		goto fail;
	}

	if (frame->pixelfmt == FMT_YUV420P) {
		uint8_t *srcPtr[4] = { frame->data[0], frame->data[1], frame->data[2], NULL };
		int srcStride[4] = { static_cast<int>(frame->stride[0]),
				static_cast<int>(frame->stride[1]),
				static_cast<int>(frame->stride[2]), 0 };
		uint8_t *dstPtr[4] = { _fbPtr + _fbStride * dstY + dstX * 4, NULL, NULL, NULL };
		int dstStride[4] = { static_cast<int>(_fbStride), 0, 0, 0 };

		if (!scaleCtx) {
			scaleCtx = sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P, frame->width, frame->height,
										AV_PIX_FMT_RGB32, SWS_POINT, NULL, NULL, NULL);
			if (!scaleCtx) {
				log->printf("DisplayOmapDrm::putImage(): Can not create scale context!\n");
				goto fail;
			}
		}
		sws_scale(scaleCtx, srcPtr, srcStride, 0, frame->height, dstPtr, dstStride);
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
