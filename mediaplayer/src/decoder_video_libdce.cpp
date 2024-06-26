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
#include <stdlib.h>
#include <cerrno>

namespace MediaPLayer {

DecoderVideoLibDCE::DecoderVideoLibDCE() :
		_display(nullptr), _codecEngine(nullptr), _codecHandle(nullptr), _codecParams(nullptr), _codecDynParams(nullptr),
		_codecStatus(0), _codecInputBufs(nullptr), _codecOutputBufs(nullptr),
		_codecInputArgs(nullptr), _codecOutputArgs(nullptr), _drmFd(0),
		_frameWidth(0), _frameHeight(0),_inputBufPtr(nullptr), _inputBufSize(0), _inputBufHandle(0),
		_numFrameBuffers(0), _frameBuffers(nullptr),
		_codecId(CODEC_ID_NONE) {
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

	switch (info.codecTag) {
	case 0: {
		switch (info.codecId) {
		case CODEC_ID_MPEG2VIDEO:
		case CODEC_ID_MPEG4:
		case CODEC_ID_WMV3:
//		case CODEC_ID_VC1:
		case CODEC_ID_H264:
			return true;
		default:
			return false;
		}
	}
	// H264:
	case 0x10000005:
	case 0x00000005:
	case MK_FOURCC('H','2','6','4'):
	case MK_FOURCC('h','2','6','4'):
	case MK_FOURCC('X','2','6','4'):
	case MK_FOURCC('x','2','6','4'):
	case MK_FOURCC('A','V','C','1'):
	case MK_FOURCC('a','v','c','1'):
	// MPEG4:
	case 0x10000004:
	case 0x00000004:
	case MK_FOURCC('F','M','P','4'):
	case MK_FOURCC('f','m','p','4'):
	case MK_FOURCC('X','V','I','D'):
	case MK_FOURCC('D','X','5','0'):
	case MK_FOURCC('D','X','G','M'):
	// MPEG2:
	case 0x10000002:
	case 0x00000002:
	case MK_FOURCC('m','p','g','2'):
	case MK_FOURCC('M','P','G','2'):
	case MK_FOURCC('M','7','0','1'):
	case MK_FOURCC('m','2','v','1'):
	case MK_FOURCC('m','2','2','v'):
	case MK_FOURCC('m','p','g','v'):
	// VC1:
/*	case MK_FOURCC('W','V','C','1'):
	case MK_FOURCC('w','v','c','1'):
	case MK_FOURCC('V','C','-','1'):
	case MK_FOURCC('v','c','-','1'):*/
	// WMV3:
	case MK_FOURCC('W','M','V','3'):
		return true;
	default:
		return false;
	}

	return false;
}

STATUS DecoderVideoLibDCE::init(Demuxer *demuxer, Display *display) {
	Engine_Error engineError;
	DisplayHandle displayHandle;
	Int32 codecError;
	int dpbSizeInFrames = 0;
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;
	struct drm_prime_handle dreq;

	_display = display;
	if (display->getHandle(&displayHandle) != S_OK) {
		log->printf("DecoderVideoLibDCE::init(): failed get display handle!\n");
		goto fail;
	}

	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibDCE::init(): demuxer->getVideoStreamInfo() failed\n");
		goto fail;
	}

	_numFrameBuffers = 3;
	switch (info.codecId) {
	case CODEC_ID_MPEG1VIDEO:
		_codecId = CODEC_ID_MPEG1VIDEO;
		break;
	case CODEC_ID_MPEG2VIDEO:
		_codecId = CODEC_ID_MPEG2VIDEO;
		break;
	case CODEC_ID_MPEG4:
		_codecId = CODEC_ID_MPEG4;
		break;
	case CODEC_ID_WMV3:
		_codecId = CODEC_ID_WMV3;
		_numFrameBuffers = 4;
		break;
	case CODEC_ID_VC1:
		_codecId = CODEC_ID_VC1;
		_numFrameBuffers = 4;
		break;
	case CODEC_ID_H264:
		_codecId = CODEC_ID_H264;
		int maxDpb;
		switch (info.profileLevel) {
		case 30:
			maxDpb = 8100;
			break;
		case 31:
			maxDpb = 18100;
			break;
		case 32:
			maxDpb = 20480;
			break;
		case 40:
		case 41:
			maxDpb = 32768;
			break;
		case 42:
			maxDpb = 34816;
			break;
		case 50:
			maxDpb = 110400;
			break;
		case 51:
		case 52:
			maxDpb = 184320;
			break;
		default:
			log->printf("DecoderVideoLibDCE::init(): not supported profile level: %d\n", info.profileLevel);
			goto fail;
		}
		dpbSizeInFrames = MIN(16, maxDpb / ((info.width / 16) * (info.height / 16)));
		_numFrameBuffers = IVIDEO2_MAX_IO_BUFFERS;
		break;
	default:
		log->printf("DecoderVideoLibDCE::init(): not supported codec!\n");
		goto fail;
	}

	_numFrameBuffers += 2; // for display buffering

	_frameWidth  = ALIGN2(info.width, 4);
	_frameHeight = ALIGN2(info.height, 4);

	dce_init(displayHandle.handle);

	_codecEngine = Engine_open((String)"ivahd_vidsvr", nullptr, &engineError);
	if (!_codecEngine) {
		log->printf("DecoderVideoLibDCE::init(): failed open codec engine!\n");
		goto fail;
	}

	switch (_codecId) {
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
		log->printf("DecoderVideoLibDCE::init(): Unsupported codec %08x\n", _codecId);
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

	switch (_codecId) {
	case CODEC_ID_H264:
		_frameWidth = ALIGN2(_frameWidth + (32 * 2), 7);
		_frameHeight = _frameHeight + 4 * 24;
		_codecParams->size = sizeof(IH264VDEC_Params);
		((IH264VDEC_Params *)_codecParams)->dpbSizeInFrames = dpbSizeInFrames;//IH264VDEC_DPB_NUMFRAMES_AUTO;
		((IH264VDEC_Params *)_codecParams)->pConstantMemory = 0;
		((IH264VDEC_Params *)_codecParams)->bitStreamFormat = IH264VDEC_BYTE_STREAM_FORMAT;
		((IH264VDEC_Params *)_codecParams)->errConcealmentMode = IH264VDEC_APPLY_CONCEALMENT;
		((IH264VDEC_Params *)_codecParams)->temporalDirModePred = IH264VDEC_ENABLE_TEMPORALDIRECT;
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
		((IMPEG4VDEC_Params *)_codecParams)->outloopDeBlocking = IMPEG4VDEC_ENHANCED_DEBLOCK_ENABLE;
		((IMPEG4VDEC_Params *)_codecParams)->errorConcealmentEnable = IMPEG4VDEC_EC_ENABLE;
		((IMPEG4VDEC_Params *)_codecParams)->sorensonSparkStream = FALSE;
		((IMPEG4VDEC_Params *)_codecParams)->debugTraceLevel = IMPEG4VDEC_DEBUGTRACE_LEVEL0; // 0 - 2
		((IMPEG4VDEC_Params *)_codecParams)->lastNFramesToLog = IMPEG4VDEC_MINNUM_OF_FRAME_LOGS;
		((IMPEG4VDEC_Params *)_codecParams)->paddingMode = IMPEG4VDEC_MPEG4_MODE_PADDING;//IMPEG4VDEC_DIVX_MODE_PADDING;
		((IMPEG4VDEC_Params *)_codecParams)->enhancedDeBlockingQp = 15; // 1 - 31
		((IMPEG4VDEC_Params *)_codecParams)->decodeOnlyIntraFrames = IMPEG4VDEC_DECODE_ONLY_I_FRAMES_DISABLE;
		_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_mpeg4dec", _codecParams);
		log->printf("DecoderVideoLibDCE::init(): using ivahd_mpeg4dec\n");
		break;
	case CODEC_ID_MPEG1VIDEO:
	case CODEC_ID_MPEG2VIDEO:
		_codecParams->size = sizeof(IMPEG2VDEC_Params);
		((IMPEG2VDEC_Params *)_codecParams)->ErrorConcealmentON = IMPEG2VDEC_EC_DISABLE; // IMPEG2VDEC_EC_ENABLE
		((IMPEG2VDEC_Params *)_codecParams)->outloopDeBlocking =  IMPEG2VDEC_DEBLOCK_ENABLE;
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
		((IVC1VDEC_Params *)_codecParams)->errorConcealmentON = TRUE;
		((IVC1VDEC_Params *)_codecParams)->frameLayerDataPresentFlag = FALSE;
		((IVC1VDEC_Params *)_codecParams)->debugTraceLevel = 0; // 0 - 4
		((IVC1VDEC_Params *)_codecParams)->lastNFramesToLog = 0;
		_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_vc1vdec", _codecParams);
		log->printf("DecoderVideoLibDCE::init(): using ivahd_vc1dec\n");
		break;
	default:
		log->printf("DecoderVideoLibDCE::init(): Unsupported codec %08x\n", _codecId);
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
	if (_codecId == CODEC_ID_H264) {
		((IH264VDEC_DynamicParams *)_codecDynParams)->deblockFilterMode = IH264VDEC_DEBLOCK_DEFAULT;
	}

	_codecDynParams->size = sizeof(VIDDEC3_DynamicParams);
	_codecDynParams->decodeHeader = XDM_DECODE_AU;
	_codecDynParams->displayWidth = 0;
	_codecDynParams->frameSkipMode = IVIDEO_NO_SKIP;
	_codecDynParams->newFrameFlag = XDAS_TRUE;
	_codecDynParams->lateAcquireArg = 0;
	if (_codecId == CODEC_ID_MPEG4 || _codecId == CODEC_ID_VC1 || _codecId == CODEC_ID_WMV3) {
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

	creq.height = (uint32_t)_frameHeight;
	creq.width = (uint32_t)_frameWidth;
	creq.bpp = 8;
	if (drmIoctl(displayHandle.handle, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		log->printf("DecoderVideoLibDCE::init(): Failed create input buffer\n");
		goto fail;
	}

	_inputBufHandle = creq.handle;

	mreq.handle = creq.handle;
	if (drmIoctl(displayHandle.handle, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
		log->printf("DisplayOmapDrm::init(): Cannot map dumb buffer: %s\n", strerror(errno));
		goto fail;
	}

	_inputBufPtr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, displayHandle.handle, mreq.offset);
	if (_inputBufPtr == MAP_FAILED) {
		log->printf("DisplayOmapDrm::init(): Cannot map dumb buffer: %s\n", strerror(errno));
		goto fail;
	}

	_inputBufSize = creq.size;

	dreq.handle = _inputBufHandle;
	dreq.flags = DRM_CLOEXEC;
	if (drmIoctl(displayHandle.handle, DRM_IOCTL_PRIME_HANDLE_TO_FD, &dreq) < 0) {
		log->printf("DisplayOmapDrm::init(): Cannot DMA buffer: %s\n", strerror(errno));
		goto fail;
	}

	_codecInputBufs->numBufs = 1;
	_codecInputBufs->descs[0].memType = XDM_MEMTYPE_RAW;
	_codecInputBufs->descs[0].buf = (XDAS_Int8 *)dreq.fd;
	_codecInputBufs->descs[0].bufSize.bytes = creq.size;
	dce_buf_lock(1, (size_t *)&(_codecInputBufs->descs[0].buf));

	_codecOutputBufs->numBufs = 2;
	_codecOutputBufs->descs[0].memType = XDM_MEMTYPE_RAW;
	_codecOutputBufs->descs[0].bufSize.bytes = _frameWidth * _frameHeight;
	_codecOutputBufs->descs[1].memType = XDM_MEMTYPE_RAW;
	_codecOutputBufs->descs[1].bufSize.bytes = _frameWidth * (_frameHeight / 2);

	_frameBuffers = (FrameBuffer **)calloc(_numFrameBuffers, sizeof(FrameBuffer *));
	for (int i = 0; i < _numFrameBuffers; i++) {
		_frameBuffers[i] = (FrameBuffer *)calloc(1, sizeof(FrameBuffer));
		if (_display->getDisplayVideoBuffer(&_frameBuffers[i]->buffer, FMT_NV12, _frameWidth, _frameHeight) != S_OK) {
			log->printf("DecoderVideoLibDCE::getBuffer(): Failed create output buffer\n");
			goto fail;
		}
		_frameBuffers[i]->index = i;
		_frameBuffers[i]->locked = false;
	}
	_initialized = true;

	return S_OK;

fail:

	if (_frameBuffers) {
		for (int i = 0; i < _numFrameBuffers; i++) {
			if (_frameBuffers[i]) {
				if (_frameBuffers[i]->buffer.priv) {
					_display->releaseDisplayVideoBuffer(&_frameBuffers[i]->buffer);
				}
				free(_frameBuffers[i]);
			}
		}
		free(_frameBuffers);
		_frameBuffers = nullptr;
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
		dce_buf_unlock(1, (size_t *)&(_codecInputBufs->descs[0].buf));
		munmap(_inputBufPtr, _inputBufSize);
		close((int)_codecInputBufs->descs[0].buf);
		dce_free(_codecInputBufs);
		_codecInputBufs = nullptr;
	}
	if (_inputBufHandle > 0) {
		struct drm_gem_close req = {
			.handle = _inputBufHandle,
		};
		drmIoctl(_drmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
		_inputBufHandle = 0;
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

	dce_deinit();

	return S_FAIL;
}

STATUS DecoderVideoLibDCE::deinit() {
	if (!_initialized) {
		return S_OK;
	}

	if (_frameBuffers) {
		for (int i = 0; i < _numFrameBuffers; i++) {
			if (_frameBuffers[i]) {
				if (_frameBuffers[i]->buffer.priv) {
					_display->releaseDisplayVideoBuffer(&_frameBuffers[i]->buffer);
				}
				free(_frameBuffers[i]);
			}
		}
		free(_frameBuffers);
		_frameBuffers = nullptr;
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
		dce_buf_unlock(1, (size_t *)&(_codecInputBufs->descs[0].buf));
        munmap(_inputBufPtr, _inputBufSize);
		close((int)_codecInputBufs->descs[0].buf);
		dce_free(_codecInputBufs);
		_codecInputBufs = nullptr;
	}
	if (_inputBufHandle > 0) {
		struct drm_gem_close req = {
			.handle = _inputBufHandle,
		};
		drmIoctl(_drmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
		_inputBufHandle = 0;
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

	dce_deinit();

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

	frameReady = false;

	_codecInputArgs->inputID = (XDAS_Int32)fb;
	_codecInputArgs->numBytes = streamFrame->videoFrame.dataSize;

	_codecInputBufs->numBufs = 1;
	_codecInputBufs->descs[0].bufSize.bytes = streamFrame->videoFrame.dataSize;

	_codecOutputBufs->numBufs = 2;
	_codecOutputBufs->descs[0].buf = (XDAS_Int8 *)fb->buffer.dmaBuf;
	_codecOutputBufs->descs[1].buf = (XDAS_Int8 *)fb->buffer.dmaBuf;

	memset(_codecOutputArgs->outputID, 0, sizeof(_codecOutputArgs->outputID));
	memset(_codecOutputArgs->freeBufID, 0, sizeof(_codecOutputArgs->freeBufID));

	Int32 codecError = VIDDEC3_process(_codecHandle, _codecInputBufs, _codecOutputBufs, _codecInputArgs, _codecOutputArgs);
	if (codecError != VIDDEC3_EOK) {
		log->printf("DecoderVideoLibDCE::decodeFrame(): VIDDEC3_process() status: %d, extendedError: %08x\n",
				codecError, _codecOutputArgs->extendedError);
		if (XDM_ISFATALERROR(_codecOutputArgs->extendedError) ||
			(codecError == DCE_EXDM_UNSUPPORTED) ||
			(codecError == DCE_EIPC_CALL_FAIL) ||
			(codecError == DCE_EINVALID_INPUT)) {
			unlockBuffer(fb);
			return S_FAIL;
		}
	}

	if (_codecOutputArgs->outBufsInUseFlag) {
		log->printf("DecoderVideoLibDCE::decodeFrame(): VIDDEC3_process() status: outBufsInUseFlag\n");
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

STATUS DecoderVideoLibDCE::flush() {
	Int32 codecError = VIDDEC3_control(_codecHandle, XDM_FLUSH, _codecDynParams, _codecStatus);
	if (codecError != VIDDEC3_EOK) {
		log->printf("DecoderVideoLibDCE::flush(): VIDDEC3_control(XDM_FLUSH) status: %d\n", codecError);
		return S_FAIL;
	}

	_codecInputArgs->inputID = 0;
	_codecInputArgs->numBytes = 0;
	_codecInputBufs->numBufs = 0;
	_codecInputBufs->descs[0].bufSize.bytes = 0;

	_codecOutputBufs->numBufs = 0;

	memset(_codecOutputArgs->outputID, 0, sizeof(_codecOutputArgs->outputID));
	memset(_codecOutputArgs->freeBufID, 0, sizeof(_codecOutputArgs->freeBufID));

	do {
		codecError = VIDDEC3_process(_codecHandle, _codecInputBufs, _codecOutputBufs, _codecInputArgs, _codecOutputArgs);
		if (codecError != VIDDEC3_EOK) {
			log->printf("DecoderVideoLibDCE::flush(): VIDDEC3_process() status: %d, extendedError: %08x\n",
					codecError, _codecOutputArgs->extendedError);
			if (XDM_ISFATALERROR(_codecOutputArgs->extendedError) ||
				(codecError == DCE_EXDM_UNSUPPORTED) ||
				(codecError == DCE_EIPC_CALL_FAIL) ||
				(codecError == DCE_EINVALID_INPUT)) {
				return S_FAIL;
			}
		}
		for (int i = 0; _codecOutputArgs->freeBufID[i]; i++) {
			unlockBuffer((FrameBuffer *)_codecOutputArgs->freeBufID[i]);
		}
	} while (codecError != XDM_EFAIL);

	for (int i = 0; i < _numFrameBuffers; i++) {
		if (_frameBuffers[i]->buffer.priv && _frameBuffers[i]->locked) {
			dce_buf_unlock(1, (size_t *)&(_frameBuffers[i]->buffer.dmaBuf));
			_frameBuffers[i]->locked = 0;
		}
	}

	memset(_codecOutputArgs->outputID, 0, sizeof(_codecOutputArgs->outputID));
	memset(_codecOutputArgs->freeBufID, 0, sizeof(_codecOutputArgs->freeBufID));

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

	FrameBuffer *fb = (FrameBuffer *)_codecOutputArgs->outputID[foundIndex];

	videoFrame->pixelfmt = FMT_NV12;
	videoFrame->data[0] = (U8 *)&fb->buffer;
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

	if (_codecId == CODEC_ID_MPEG2VIDEO && _frameWidth == 720 && (_frameHeight == 576 || _frameHeight == 480)) {
		videoFrame->anistropicDVD = true;
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].contentType == IVIDEO_INTERLACED) {
		videoFrame->interlaced = true;
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].contentType == IVIDEO_INTERLACED_TOPFIELD) {
		log->printf("DecoderVideoLibDCE::getVideoStreamOutputFrame(): IVIDEO_INTERLACED_TOPFIELD\n");
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].contentType == IVIDEO_INTERLACED_BOTTOMFIELD) {
		log->printf("DecoderVideoLibDCE::getVideoStreamOutputFrame(): IVIDEO_INTERLACED_BOTTOMFIELD\n");
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].topFieldFirstFlag) {
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].repeatFirstFieldFlag) {
		log->printf("DecoderVideoLibDCE::getVideoStreamOutputFrame(): repeatFirstFieldFlag\n");
	}

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

	for (int i = 0; i < _numFrameBuffers; i++) {
		if (_frameBuffers[i]->buffer.priv && !_frameBuffers[i]->locked) {
			if (!_frameBuffers[i]->buffer.locked) {
				dce_buf_lock(1, (size_t *)&(_frameBuffers[i]->buffer.dmaBuf));
				_frameBuffers[i]->locked = true;
				return _frameBuffers[i];
			}
		}
	}

	log->printf("DecoderVideoLibDCE::getBuffer(): No free slots for output buffer\n");
	return nullptr;
}

void DecoderVideoLibDCE::lockBuffer(FrameBuffer *fb) {
	if (!_initialized || !fb) {
		return;
	}

	if (_frameBuffers[fb->index]->locked) {
		log->printf("DecoderVideoLibDCE::lockBuffer(): Already locked frame buffer at index: %d\n", fb->index);
		return;
	}

	if (!_frameBuffers[fb->index]->buffer.priv) {
		log->printf("DecoderVideoLibDCE::lockBuffer(): Missing frame buffer at index: %d\n", fb->index);
		return;
	}

	dce_buf_lock(1, (size_t *)&(_frameBuffers[fb->index]->buffer.dmaBuf));

	_frameBuffers[fb->index]->locked = true;
}

void DecoderVideoLibDCE::unlockBuffer(FrameBuffer *fb) {
	if (!_initialized || !fb) {
		return;
	}

	if (!_frameBuffers[fb->index]->locked) {
		log->printf("DecoderVideoLibDCE::unlockBuffer(): Already unlocked frame buffer at index: %d\n", fb->index);
		return;
	}

	if (!_frameBuffers[fb->index]->buffer.priv) {
		log->printf("DecoderVideoLibDCE::unlockBuffer(): Missing frame buffer at index: %d\n", fb->index);
		return;
	}

	dce_buf_unlock(1, (size_t *)&(_frameBuffers[fb->index]->buffer.dmaBuf));

	_frameBuffers[fb->index]->locked = false;
}

} // namespace
