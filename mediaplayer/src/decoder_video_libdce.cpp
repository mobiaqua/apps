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

#include "basetypes.h"
#include "logs.h"
#include "decoder_video_libdce.h"
#include "display_base.h"
#include <string.h>

namespace MediaPLayer {

DecoderVideoLibDCE::DecoderVideoLibDCE() :
		_display(nullptr), _codecEngine(nullptr), _codecHandle(nullptr), _codecParams(nullptr), _codecDynParams(nullptr),
		_codecStatus(0), _codecInputBufs(nullptr), _codecOutputBufs(nullptr),
		_codecInputArgs(nullptr), _codecOutputArgs(nullptr), _omapDev(nullptr),
		_frameWidth(0), _frameHeight(0),_inputBufPtr(nullptr), _inputBufSize(0), _inputBufBo(nullptr) {
	_bpp = 2;
}

DecoderVideoLibDCE::~DecoderVideoLibDCE() {
	deinit();
}

bool DecoderVideoLibDCE::isCapable(Demuxer *demuxer) {
	if (demuxer == nullptr) {
		log->printf("DecoderVideoLibDCE::isCapable(): demuxer is NULL\n");
		return false;
	}

	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibDCE::isCapable(): demuxer->getVideoStreamInfo() failed\n");
		return false;
	}

	switch (info.codecId) {
	case CODEC_ID_MPEG1VIDEO:
	case CODEC_ID_MPEG2VIDEO:
	case CODEC_ID_MPEG4:
	case CODEC_ID_WMV3:
	case CODEC_ID_VC1:
	case CODEC_ID_H264:
		return true;
	default:
		return false;
	}

	return false;
}

STATUS DecoderVideoLibDCE::init(Demuxer *demuxer, Display *display) {
	unsigned int codecId = CODEC_ID_NONE;
	Engine_Error engineError;
	DisplayHandle displayHandle;
	Int32 codecError;

	_display = display;
	if (display->getHandle(&displayHandle) != S_OK) {
		log->printf("DecoderVideoLibDCE::init(): failed get display handle!\n");
        goto fail;
	}

	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibDCE::init(): demuxer->getVideoStreamInfo() failed\n");
		return false;
	}

	switch (info.codecId) {
	case CODEC_ID_MPEG1VIDEO:
		codecId = CODEC_ID_MPEG1VIDEO;
		break;
	case CODEC_ID_MPEG2VIDEO:
		codecId = CODEC_ID_MPEG2VIDEO;
		break;
	case CODEC_ID_MPEG4:
		codecId = CODEC_ID_MPEG4;
		break;
	case CODEC_ID_WMV3:
		codecId = CODEC_ID_WMV3;
		break;
	case CODEC_ID_VC1:
		codecId = CODEC_ID_VC1;
		break;
	case CODEC_ID_H264:
		codecId = CODEC_ID_H264;
		break;
	default:
		log->printf("DecoderVideoLibDCE::init(): not supported codec!\n");
		return S_FAIL;
	}

	_frameWidth  = ALIGN2(info.width, 4);
	_frameHeight = ALIGN2(info.height, 4);

	dce_set_fd(displayHandle.handle);
	_omapDev = dce_init();
	if (_omapDev == nullptr) {
		log->printf("DecoderVideoLibDCE::init(): failed init dce!\n");
        goto fail;
	}

	_codecEngine = Engine_open((String)"ivahd_vidsvr", nullptr, &engineError);
	if (!_codecEngine) {
		log->printf("DecoderVideoLibDCE::init(): failed open codec engine!\n");
        goto fail;
	}

	switch (codecId) {
    case CODEC_ID_H264:
        _codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IH264VDEC_Params));
        break;
    case CODEC_ID_MPEG4:
    	_codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IMPEG4VDEC_Params));
        break;
    case CODEC_ID_MPEG1VIDEO:
    case CODEC_ID_MPEG2VIDEO:
    	_codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IMPEG2VDEC_Params));
        break;
    case CODEC_ID_WMV3:
    case CODEC_ID_VC1:
    	_codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IVC1VDEC_Params));
        break;
    default:
		log->printf("DecoderVideoLibDCE::init(): Unsupported codec %08x\n", codecId);
        goto fail;
    }

    if (!_codecParams) {
		log->printf("DecoderVideoLibDCE::init(): Error allocation with dce_alloc()\n");
        goto fail;
    }

    _codecParams->maxWidth = _frameWidth;
    _codecParams->maxHeight = _frameHeight;
    _codecParams->maxFrameRate = 30000;
    _codecParams->maxBitRate = 10000000;
    _codecParams->dataEndianness = XDM_BYTE;
    _codecParams->forceChromaFormat = XDM_YUV_420SP;
    _codecParams->operatingMode = IVIDEO_DECODE_ONLY;
    _codecParams->displayDelay = IVIDDEC3_DISPLAY_DELAY_AUTO;
    _codecParams->displayBufsMode = IVIDDEC3_DISPLAYBUFS_EMBEDDED;
    _codecParams->inputDataMode = IVIDEO_ENTIREFRAME;
    _codecParams->outputDataMode = IVIDEO_ENTIREFRAME;
    _codecParams->numInputDataUnits = 0;
    _codecParams->numOutputDataUnits = 0;
    _codecParams->errorInfoMode = IVIDEO_ERRORINFO_OFF;
    _codecParams->metadataType[0] = IVIDEO_METADATAPLANE_NONE;
    _codecParams->metadataType[1] = IVIDEO_METADATAPLANE_NONE;
    _codecParams->metadataType[2] = IVIDEO_METADATAPLANE_NONE;

    switch (codecId) {
    case CODEC_ID_H264:
    	_frameWidth = ALIGN2(_frameWidth + (32 * 2), 7);
    	_frameHeight = _frameHeight + 4 * 24;
    	_codecParams->size = sizeof(IH264VDEC_Params);
        ((IH264VDEC_Params *)_codecParams)->dpbSizeInFrames = IH264VDEC_DPB_NUMFRAMES_AUTO;
        ((IH264VDEC_Params *)_codecParams)->pConstantMemory = 0;
        ((IH264VDEC_Params *)_codecParams)->bitStreamFormat = IH264VDEC_BYTE_STREAM_FORMAT;
        ((IH264VDEC_Params *)_codecParams)->errConcealmentMode = IH264VDEC_NO_CONCEALMENT; // IH264VDEC_APPLY_CONCEALMENT
        ((IH264VDEC_Params *)_codecParams)->temporalDirModePred = IH264VDEC_DISABLE_TEMPORALDIRECT;
        ((IH264VDEC_Params *)_codecParams)->svcExtensionFlag = IH264VDEC_DISABLE_SVCEXTENSION;
        ((IH264VDEC_Params *)_codecParams)->svcTargetLayerDID = IH264VDEC_TARGET_DID_DEFAULT;
        ((IH264VDEC_Params *)_codecParams)->svcTargetLayerTID = IH264VDEC_TARGET_TID_DEFAULT;
        ((IH264VDEC_Params *)_codecParams)->svcTargetLayerQID = IH264VDEC_TARGET_QID_DEFAULT;
        ((IH264VDEC_Params *)_codecParams)->presetLevelIdc = IH264VDEC_MAXLEVELID;
        ((IH264VDEC_Params *)_codecParams)->presetProfileIdc = IH264VDEC_PROFILE_ANY;
        ((IH264VDEC_Params *)_codecParams)->detectCabacAlignErr = IH264VDEC_DISABLE_CABACALIGNERR_DETECTION;
        ((IH264VDEC_Params *)_codecParams)->detectIPCMAlignErr = IH264VDEC_DISABLE_IPCMALIGNERR_DETECTION;
        ((IH264VDEC_Params *)_codecParams)->debugTraceLevel = IH264VDEC_DEBUGTRACE_LEVEL0; // 0 - 3
        ((IH264VDEC_Params *)_codecParams)->lastNFramesToLog = 0;
        ((IH264VDEC_Params *)_codecParams)->enableDualOutput = IH264VDEC_DUALOUTPUT_DISABLE;
        ((IH264VDEC_Params *)_codecParams)->processCallLevel = FALSE; // TRUE - for interlace
        ((IH264VDEC_Params *)_codecParams)->enableWatermark = IH264VDEC_WATERMARK_DISABLE;
        ((IH264VDEC_Params *)_codecParams)->decodeFrameType = IH264VDEC_DECODE_ALL;
        _codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_h264dec", _codecParams);
		log->printf("DecoderVideoLibDCE::init(): using ivahd_h264dec\n");
        break;
    case CODEC_ID_MPEG4:
    	_frameWidth = ALIGN2(_frameWidth + 32, 7);
    	_frameHeight = _frameHeight + 32;
    	_codecParams->size = sizeof(IMPEG4VDEC_Params);
        ((IMPEG4VDEC_Params *)_codecParams)->outloopDeBlocking = IMPEG4VDEC_DEBLOCK_DISABLE; // IMPEG4VDEC_ENHANCED_DEBLOCK_ENABLE
        ((IMPEG4VDEC_Params *)_codecParams)->errorConcealmentEnable = IMPEG4VDEC_EC_DISABLE; // IMPEG4VDEC_EC_ENABLE
        ((IMPEG4VDEC_Params *)_codecParams)->sorensonSparkStream = FALSE;
        ((IMPEG4VDEC_Params *)_codecParams)->debugTraceLevel = IMPEG4VDEC_DEBUGTRACE_LEVEL0; // 0 - 2
        ((IMPEG4VDEC_Params *)_codecParams)->lastNFramesToLog = IMPEG4VDEC_MINNUM_OF_FRAME_LOGS;
        ((IMPEG4VDEC_Params *)_codecParams)->paddingMode = IMPEG4VDEC_MPEG4_MODE_PADDING;//IMPEG4VDEC_DIVX_MODE_PADDING;
        ((IMPEG4VDEC_Params *)_codecParams)->enhancedDeBlockingQp = IMPEG4VDEC_DEBLOCK_QP_MAX; // 1 - 31
        ((IMPEG4VDEC_Params *)_codecParams)->decodeOnlyIntraFrames = IMPEG4VDEC_DECODE_ONLY_I_FRAMES_DISABLE;
        _codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_mpeg4dec", _codecParams);
		log->printf("DecoderVideoLibDCE::init(): using ivahd_mpeg4dec\n");
        break;
    case CODEC_ID_MPEG1VIDEO:
    case CODEC_ID_MPEG2VIDEO:
    	_codecParams->size = sizeof(IMPEG2VDEC_Params);
        ((IMPEG2VDEC_Params *)_codecParams)->ErrorConcealmentON = IMPEG2VDEC_EC_DISABLE; // IMPEG2VDEC_EC_ENABLE
        ((IMPEG2VDEC_Params *)_codecParams)->outloopDeBlocking =  IMPEG2VDEC_DEBLOCK_DISABLE; // IMPEG2VDEC_DEBLOCK_ENABLE
        ((IMPEG2VDEC_Params *)_codecParams)->debugTraceLevel = 0; // 0 - 4
        ((IMPEG2VDEC_Params *)_codecParams)->lastNFramesToLog = 0;
    	_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_mpeg2vdec", _codecParams);
		log->printf("DecoderVideoLibDCE::init(): using ivahd_mpeg2vdec\n");
		break;
    case CODEC_ID_WMV3:
    case CODEC_ID_VC1:
    	_frameWidth = ALIGN2(_frameWidth + (32 * 2), 7);
    	_frameHeight = (ALIGN2(_frameHeight / 2, 4) * 2) + 2 * 40;
		_codecParams->size = sizeof(IVC1VDEC_Params);
		((IVC1VDEC_Params *)_codecParams)->errorConcealmentON = FALSE;
		((IVC1VDEC_Params *)_codecParams)->frameLayerDataPresentFlag = FALSE;
        ((IVC1VDEC_Params *)_codecParams)->debugTraceLevel = 0; // 0 - 4
        ((IVC1VDEC_Params *)_codecParams)->lastNFramesToLog = 0;
		_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_vc1vdec", _codecParams);
		log->printf("DecoderVideoLibDCE::init(): using ivahd_vc1dec\n");
        break;
    default:
		log->printf("DecoderVideoLibDCE::init(): Unsupported codec %08x\n", codecId);
		goto fail;
    }

    if (!_codecHandle) {
		log->printf("DecoderVideoLibDCE::init(): Error: VIDDEC3_create() failed\n");
		goto fail;
    }

    _codecStatus = (VIDDEC3_Status *)dce_alloc(sizeof(VIDDEC3_Status));
    _codecDynParams = (VIDDEC3_DynamicParams *)dce_alloc(sizeof(VIDDEC3_DynamicParams));
    _codecInputBufs = (XDM2_BufDesc *)dce_alloc(sizeof(XDM2_BufDesc));
    _codecOutputBufs = (XDM2_BufDesc *)dce_alloc(sizeof(XDM2_BufDesc));
    _codecInputArgs = (VIDDEC3_InArgs *)dce_alloc(sizeof(VIDDEC3_InArgs));
    _codecOutputArgs = (VIDDEC3_OutArgs *)dce_alloc(sizeof(VIDDEC3_OutArgs));
    if (!_codecDynParams || !_codecStatus || !_codecInputBufs || !_codecOutputBufs || !_codecInputArgs || !_codecOutputArgs) {
		log->printf("DecoderVideoLibDCE::init(): Failed allocation with dce_alloc()\n");
		goto fail;
    }

    _codecDynParams->size = sizeof(VIDDEC3_DynamicParams);
    _codecDynParams->decodeHeader = XDM_DECODE_AU;
    _codecDynParams->displayWidth = 0;
    _codecDynParams->frameSkipMode = IVIDEO_NO_SKIP;
    _codecDynParams->newFrameFlag = XDAS_TRUE;
    _codecDynParams->lateAcquireArg = 0;
    if (codecId == CODEC_ID_MPEG4 || codecId == CODEC_ID_VC1 || codecId == CODEC_ID_WMV3) {
    	_codecDynParams->lateAcquireArg = -1;
    }

    _codecStatus->size = sizeof(VIDDEC3_Status);
    _codecInputArgs->size = sizeof(VIDDEC3_InArgs);
    _codecOutputArgs->size = sizeof(VIDDEC3_OutArgs);

    codecError = VIDDEC3_control(_codecHandle, XDM_SETPARAMS, _codecDynParams, _codecStatus);
    if (codecError != VIDDEC3_EOK) {
		log->printf("DecoderVideoLibDCE::init(): VIDDEC3_control(XDM_SETPARAMS) failed %d\n", codecError);
		goto fail;
    }
    codecError = VIDDEC3_control(_codecHandle, XDM_GETBUFINFO, _codecDynParams, _codecStatus);
    if (codecError != VIDDEC3_EOK) {
		log->printf("DecoderVideoLibDCE::init(): VIDDEC3_control(XDM_GETBUFINFO) failed %d\n", codecError);
		goto fail;
    }

    _inputBufBo = (omap_bo *)omap_bo_new(_omapDev, _frameWidth * _frameHeight, OMAP_BO_WC);
    if (!_inputBufBo) {
		log->printf("DecoderVideoLibDCE::init(): Failed create input buffer\n");
		goto fail;
    }
    _inputBufPtr = omap_bo_map(_inputBufBo);
    _inputBufSize = omap_bo_size(_inputBufBo);

    _codecInputBufs->numBufs = 1;
    _codecInputBufs->descs[0].memType = XDM_MEMTYPE_BO;
    _codecInputBufs->descs[0].buf = (XDAS_Int8 *)omap_bo_handle(_inputBufBo);
	_codecInputBufs->descs[0].bufSize.bytes = omap_bo_size(_inputBufBo);

    _codecOutputBufs->numBufs = 2;
    _codecOutputBufs->descs[0].memType = XDM_MEMTYPE_BO;
    _codecOutputBufs->descs[0].bufSize.bytes = _frameWidth * _frameHeight;
    _codecOutputBufs->descs[1].memType = XDM_MEMTYPE_BO_OFFSET;
    _codecOutputBufs->descs[1].buf = (XDAS_Int8 *)(_frameWidth * _frameHeight);
    _codecOutputBufs->descs[1].bufSize.bytes = _frameWidth * (_frameHeight / 2);

	_initialized = true;

	return S_OK;

fail:

	if (_inputBufBo) {
		omap_bo_del(_inputBufBo);
		_inputBufBo = nullptr;
	}
	for (int i = 0; i < IVIDEO2_MAX_IO_BUFFERS; i++) {
		if (_frameBuffers[i].buffer.priv) {
			_display->releaseVideoBuffer(&_frameBuffers[i].buffer);
			_frameBuffers[i].buffer = {};
		}
	}
	if (_codecHandle) {
		VIDDEC3_delete(_codecHandle);
		_codecHandle = nullptr;
	}

	if (_codecParams) {
		dce_free(_codecParams);
		_codecParams = nullptr;
	}
	if (_codecStatus) {
		dce_free(_codecStatus);
		_codecStatus = nullptr;
	}
	if (_codecDynParams) {
		dce_free(_codecDynParams);
		_codecDynParams = nullptr;
	}
	if (_codecInputBufs) {
		dce_free(_codecInputBufs);
		_codecInputBufs = nullptr;
	}
	if (_codecOutputBufs) {
		dce_free(_codecOutputBufs);
		_codecOutputBufs = nullptr;
	}
	if (_codecInputArgs) {
		dce_free(_codecInputArgs);
		_codecInputArgs = nullptr;
	}
	if (_codecOutputArgs) {
		dce_free(_codecOutputArgs);
		_codecOutputArgs = nullptr;
	}

	if (_codecEngine) {
		Engine_close(_codecEngine);
		_codecEngine = nullptr;
	}

	if (_omapDev) {
		dce_deinit(_omapDev);
		_omapDev = nullptr;
	}

	return S_FAIL;
}

STATUS DecoderVideoLibDCE::deinit() {
	if (!_initialized) {
		return S_OK;
	}

	if (_inputBufBo) {
		omap_bo_del(_inputBufBo);
		_inputBufBo = nullptr;
	}
	for (int i = 0; i < IVIDEO2_MAX_IO_BUFFERS; i++) {
		if (_frameBuffers[i].buffer.priv) {
			_display->releaseVideoBuffer(&_frameBuffers[i].buffer);
			_frameBuffers[i].buffer = {};
		}
	}

	if (_codecHandle && _codecDynParams && _codecParams) {
		VIDDEC3_control(_codecHandle, XDM_FLUSH, _codecDynParams, _codecStatus);
	}

	if (_codecHandle) {
		VIDDEC3_delete(_codecHandle);
		_codecHandle = nullptr;
	}

	if (_codecParams) {
		dce_free(_codecParams);
		_codecParams = nullptr;
	}
	if (_codecStatus) {
		dce_free(_codecStatus);
		_codecStatus = nullptr;
	}
	if (_codecDynParams) {
		dce_free(_codecDynParams);
		_codecDynParams = nullptr;
	}
	if (_codecInputBufs) {
		dce_free(_codecInputBufs);
		_codecInputBufs = nullptr;
	}
	if (_codecOutputBufs) {
		dce_free(_codecOutputBufs);
		_codecOutputBufs = nullptr;
	}
	if (_codecInputArgs) {
		dce_free(_codecInputArgs);
		_codecInputArgs = nullptr;
	}
	if (_codecOutputArgs) {
		dce_free(_codecOutputArgs);
		_codecOutputArgs = nullptr;
	}

	if (_codecEngine) {
		Engine_close(_codecEngine);
		_codecEngine = nullptr;
	}

	if (_omapDev) {
		dce_deinit(_omapDev);
		_omapDev = nullptr;
	}

	_initialized = false;

	return S_OK;
}

void DecoderVideoLibDCE::getDemuxerBuffer(StreamFrame *streamFrame) {
	if (!_initialized || !streamFrame) {
		streamFrame->videoFrame.data = nullptr;
		streamFrame->videoFrame.dataSize = 0;
		streamFrame->videoFrame.externalDataSize = 0;
	} else {
		streamFrame->videoFrame.data = (U8 *)_inputBufPtr;
		streamFrame->videoFrame.dataSize = 0;
		streamFrame->videoFrame.externalDataSize = _inputBufSize;
	}
}

STATUS DecoderVideoLibDCE::decodeFrame(bool &frameReady, StreamFrame *streamFrame) {
	if (!_initialized) {
		return S_FAIL;
	}

	FrameBuffer *fb = getBuffer();
	if (!fb) {
		return S_FAIL;
	}

	lockBuffer(fb);

	frameReady = false;

	omap_bo *bo = fb->buffer.bo;
	_codecInputArgs->inputID = (XDAS_Int32)fb;
	_codecInputArgs->numBytes = streamFrame->videoFrame.dataSize;

	_codecInputBufs->descs[0].bufSize.bytes = streamFrame->videoFrame.dataSize;

	_codecOutputBufs->descs[0].buf = (XDAS_Int8 *)omap_bo_handle(bo);

	memset(_codecOutputArgs->outputID, 0, sizeof(_codecOutputArgs->outputID));
	memset(_codecOutputArgs->freeBufID, 0, sizeof(_codecOutputArgs->freeBufID));

	Int32 codecError = VIDDEC3_process(_codecHandle, _codecInputBufs, _codecOutputBufs, _codecInputArgs, _codecOutputArgs);
	if (codecError != VIDDEC3_EOK) {
		log->printf("DecoderVideoLibDCE::decodeFrame(): VIDDEC3_process() status: %d, extendedError: %08x\n",
				codecError, _codecOutputArgs->extendedError);
		if (XDM_ISFATALERROR(_codecOutputArgs->extendedError)) {
			unlockBuffer(fb);
			return S_FAIL;
		}
	}

	if (_codecOutputArgs->outBufsInUseFlag)
	{
		return S_FAIL;
	}

	for (int i = 0; _codecOutputArgs->outputID[i]; i++) {
		frameReady = true;
		break;
	}

	for (int i = 0; _codecOutputArgs->freeBufID[i]; i++) {
		unlockBuffer((FrameBuffer *)_codecOutputArgs->freeBufID[i]);
	}

	return S_OK;
}

STATUS DecoderVideoLibDCE::getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame) {
	if (!_initialized) {
		return S_FAIL;
	}

	int foundIndex = -1;
	for (int i = 0; _codecOutputArgs->outputID[i]; i++) {
		foundIndex = i;
		break;
	}
	if (foundIndex == -1) {
		return S_FAIL;
	}

	XDM_Rect *r = &_codecOutputArgs->displayBufs.bufDesc[0].activeFrameRegion;

	videoFrame->pixelfmt = FMT_NV12;
	videoFrame->data[0] = (U8 *)_codecOutputArgs->outputID[foundIndex];
	videoFrame->data[1] = nullptr;
	videoFrame->data[2] = nullptr;
	videoFrame->data[3] = nullptr;
	videoFrame->stride[0] = _frameWidth;
	videoFrame->stride[1] = 0;
	videoFrame->stride[2] = 0;
	videoFrame->stride[3] = 0;
	videoFrame->width = _frameWidth;
	videoFrame->height = _frameHeight;
	videoFrame->dx = r->topLeft.x;
	videoFrame->dy = r->topLeft.y;
	videoFrame->dw = r->bottomRight.x - r->topLeft.x;
	videoFrame->dh = r->bottomRight.y - r->topLeft.y;
	videoFrame->hw = true;

	return S_OK;
}

int DecoderVideoLibDCE::getVideoWidth(Demuxer *demuxer) {
	if (!_initialized) {
		return 0;
	}

	return _frameWidth;
}

int DecoderVideoLibDCE::getVideoHeight(Demuxer *demuxer) {
	if (!_initialized) {
		return 0;
	}

	return _frameHeight;
}

DecoderVideoLibDCE::FrameBuffer *DecoderVideoLibDCE::getBuffer() {
	if (!_initialized) {
		return nullptr;
	}

	for (int i = 0; i < IVIDEO2_MAX_IO_BUFFERS; i++) {
		if (_frameBuffers[i].buffer.priv && !_frameBuffers[i].locked) {
			return &_frameBuffers[i];
		}
		if (!_frameBuffers[i].buffer.priv && !_frameBuffers[i].locked) {
			if (_display->getVideoBuffer(&_frameBuffers[i].buffer, FMT_NV12, _frameWidth, _frameHeight) != S_OK) {
				log->printf("DecoderVideoLibDCE::getBuffer(): Failed create output buffer\n");
				return nullptr;
		    }
		    _frameBuffers[i].index = i;
		    _frameBuffers[i].locked = false;
			return &_frameBuffers[i];
		}
	}

	log->printf("DecoderVideoLibDCE::init(): No free slots for output buffer\n");
	return nullptr;
}

void DecoderVideoLibDCE::lockBuffer(FrameBuffer *fb) {
	if (!_initialized || !fb) {
		return;
	}

	if (_frameBuffers[fb->index].locked) {
		log->printf("DecoderVideoLibDCE::getBuffer(): Already locked frame buffer at index: %d\n", fb->index);
		return;
	}

	if (!_frameBuffers[fb->index].buffer.priv) {
		log->printf("DecoderVideoLibDCE::getBuffer(): Missing frame buffer at index: %d\n", fb->index);
		return;
	}

	_frameBuffers[fb->index].locked = true;
}

void DecoderVideoLibDCE::unlockBuffer(FrameBuffer *fb) {
	if (!_initialized || !fb) {
		return;
	}

	if (!_frameBuffers[fb->index].locked) {
		log->printf("DecoderVideoLibDCE::getBuffer(): Already unlocked frame buffer at index: %d\n", fb->index);
		return;
	}

	if (!_frameBuffers[fb->index].buffer.priv) {
		log->printf("DecoderVideoLibDCE::getBuffer(): Missing frame buffer at index: %d\n", fb->index);
		return;
	}

	_frameBuffers[fb->index].locked = false;
}

} // namespace
