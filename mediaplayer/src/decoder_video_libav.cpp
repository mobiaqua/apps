/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013-2016 Pawel Kolodziejski <aquadran at users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "basetypes.h"
#include "logs.h"
#include "decoder_video_base.h"
#include "decoder_video_libav.h"
#include "demuxer_libav.h"

namespace MediaPLayer {

DecoderVideoLibAV::DecoderVideoLibAV() :
		_avc(nullptr), _codec(nullptr), _frame(nullptr) {
	_frame = av_frame_alloc();
}

DecoderVideoLibAV::~DecoderVideoLibAV() {
	deinit();
}

bool DecoderVideoLibAV::isCapable(Demuxer *demuxer) {
	if (demuxer == nullptr) {
		log->printf("DecoderVideoLibAV::isCapable(): demuxer is NULL\n");
		return false;
	}

	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(info) != S_OK) {
		log->printf("DecoderVideoLibAV::isCapable(): demuxer->getVideoStreamInfo() failed\n");
		return false;
	}

	switch (info.codecId) {
	case CODEC_ID_MPEG1VIDEO:
	case CODEC_ID_MPEG2VIDEO:
	case CODEC_ID_H261:
	case CODEC_ID_H263:
	case CODEC_ID_MPEG4:
	case CODEC_ID_MSMPEG4V1:
	case CODEC_ID_MSMPEG4V2:
	case CODEC_ID_MSMPEG4V3:
	case CODEC_ID_H263P:
	case CODEC_ID_H263I:
	case CODEC_ID_FLV1:
	case CODEC_ID_SVQ1:
	case CODEC_ID_SVQ3:
	case CODEC_ID_AIC:
	case CODEC_ID_DVVIDEO:
	case CODEC_ID_VP3:
	case CODEC_ID_VP5:
	case CODEC_ID_VP6:
	case CODEC_ID_VP6A:
	case CODEC_ID_VP6F:
	case CODEC_ID_VP7:
	case CODEC_ID_VP8:
	case CODEC_ID_VP9:
	case CODEC_ID_WEBP:
	case CODEC_ID_THEORA:
	case CODEC_ID_RV10:
	case CODEC_ID_RV20:
	case CODEC_ID_RV30:
	case CODEC_ID_RV40:
	case CODEC_ID_WMV1:
	case CODEC_ID_WMV2:
	case CODEC_ID_WMV3:
	case CODEC_ID_VC1:
	case CODEC_ID_H264:
	case CODEC_ID_HEVC:
		return true;
	default:
		return false;
	}

	return false;
}

STATUS DecoderVideoLibAV::init(Demuxer *demuxer) {
	int err;

	if (_initialized) {
		log->printf("DecoderVideoLibAV::init(): already initialized!\n");
		goto fail;
	}

	if (demuxer == nullptr) {
		log->printf("DecoderVideoLibAV::init(): demuxer is NULL\n");
		goto fail;
	}

	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(info) != S_OK) {
		log->printf("DecoderVideoLibAV::init(): demuxer->getVideoStreamInfo() failed\n");
		goto fail;
	}

	_bpp = 2; // default to 2

	AVCodecID codecIdAV;
	switch (info.codecId) {
	case CODEC_ID_MPEG1VIDEO:
		codecIdAV = AV_CODEC_ID_MPEG1VIDEO;
		break;
	case CODEC_ID_MPEG2VIDEO:
		codecIdAV = AV_CODEC_ID_MPEG2VIDEO;
		break;
	case CODEC_ID_H261:
		codecIdAV = AV_CODEC_ID_H261;
		break;
	case CODEC_ID_H263:
		codecIdAV = AV_CODEC_ID_H263;
		break;
	case CODEC_ID_MPEG4:
		codecIdAV = AV_CODEC_ID_MPEG4;
		break;
	case CODEC_ID_MSMPEG4V1:
		codecIdAV = AV_CODEC_ID_MSMPEG4V1;
		break;
	case CODEC_ID_MSMPEG4V2:
		codecIdAV = AV_CODEC_ID_MSMPEG4V2;
		break;
	case CODEC_ID_MSMPEG4V3:
		codecIdAV = AV_CODEC_ID_MSMPEG4V3;
		break;
	case CODEC_ID_H263P:
		codecIdAV = AV_CODEC_ID_H263P;
		break;
	case CODEC_ID_H263I:
		codecIdAV = AV_CODEC_ID_H263I;
		break;
	case CODEC_ID_FLV1:
		codecIdAV = AV_CODEC_ID_FLV1;
		break;
	case CODEC_ID_SVQ1:
		codecIdAV = AV_CODEC_ID_SVQ1;
		break;
	case CODEC_ID_SVQ3:
		codecIdAV = AV_CODEC_ID_SVQ3;
		break;
	case CODEC_ID_AIC:
		codecIdAV = AV_CODEC_ID_AIC;
		break;
	case CODEC_ID_DVVIDEO:
		codecIdAV = AV_CODEC_ID_DVVIDEO;
		break;
	case CODEC_ID_VP3:
		codecIdAV = AV_CODEC_ID_VP3;
		break;
	case CODEC_ID_VP5:
		codecIdAV = AV_CODEC_ID_VP5;
		break;
	case CODEC_ID_VP6:
		codecIdAV = AV_CODEC_ID_VP6;
		break;
	case CODEC_ID_VP6A:
		codecIdAV = AV_CODEC_ID_VP6A;
		break;
	case CODEC_ID_VP6F:
		codecIdAV = AV_CODEC_ID_VP6F;
		break;
	case CODEC_ID_VP7:
		codecIdAV = AV_CODEC_ID_VP7;
		break;
	case CODEC_ID_VP8:
		codecIdAV = AV_CODEC_ID_VP8;
		break;
	case CODEC_ID_VP9:
		codecIdAV = AV_CODEC_ID_VP9;
		break;
	case CODEC_ID_WEBP:
		codecIdAV = AV_CODEC_ID_WEBP;
		break;
	case CODEC_ID_THEORA:
		codecIdAV = AV_CODEC_ID_THEORA;
		break;
	case CODEC_ID_RV10:
		codecIdAV = AV_CODEC_ID_RV10;
		break;
	case CODEC_ID_RV20:
		codecIdAV = AV_CODEC_ID_RV20;
		break;
	case CODEC_ID_RV30:
		codecIdAV = AV_CODEC_ID_RV30;
		break;
	case CODEC_ID_RV40:
		codecIdAV = AV_CODEC_ID_RV40;
		break;
	case CODEC_ID_WMV1:
		codecIdAV = AV_CODEC_ID_WMV1;
		break;
	case CODEC_ID_WMV2:
		codecIdAV = AV_CODEC_ID_WMV2;
		break;
	case CODEC_ID_WMV3:
		codecIdAV = AV_CODEC_ID_WMV3;
		break;
	case CODEC_ID_VC1:
		codecIdAV = AV_CODEC_ID_VC1;
		break;
	case CODEC_ID_H264:
		codecIdAV = AV_CODEC_ID_H264;
		break;
	case CODEC_ID_HEVC:
		codecIdAV = AV_CODEC_ID_HEVC;
		break;
	default:
		log->printf("DecoderVideoLibAV::init(): not supported codec: 0x%08h\n", info.codecId);
		goto fail;
	}

	_codec = avcodec_find_decoder(codecIdAV);
	if (_codec == NULL) {
		log->printf("DecoderVideoLibAV::init(): avcodec_find_decoder() failed\n");
		goto fail;
	}
	_avc = avcodec_alloc_context3(NULL);
	if (_avc == NULL) {
		log->printf("DecoderVideoLibAV::init(): avcodec_alloc_context() failed\n");
		goto fail;
	}

	_avc->width = info.width;
	_avc->height = info.height;
	_avc->time_base.num = info.timeBaseRate;
	_avc->time_base.den = info.timeBaseScale;

	if (demuxer->getVideoStreamExtraData((U32 &)_avc->extradata_size, (U8 **)&_avc->extradata) != S_OK) {
		log->printf("DecoderVideoLibAV::init(): demuxer->getVideoStreamExtraData() failed\n");
		goto fail;
	}

	err = avcodec_open2(_avc, _codec, NULL);
	if (err != 0) {
		fprintf(stderr, "DecoderVideoLibAV::init(): avcodec_open2() failed: %d\n", err);
		goto fail;
	}

	switch (_avc->pix_fmt) {
	case AV_PIX_FMT_RGB24:
		_pixelFormat = FMT_RGB24;
		break;
	case AV_PIX_FMT_ARGB:
		_pixelFormat = FMT_ARGB;
		break;
	case AV_PIX_FMT_YUV422P:
		_pixelFormat = FMT_YUV422;
		break;
	case AV_PIX_FMT_YUV420P:
		_pixelFormat = FMT_YUV420P;
		break;
	case AV_PIX_FMT_NV12:
		_pixelFormat = FMT_NV12;
		break;
	default:
		_pixelFormat = FMT_NONE;
		log->printf("DecoderVideoLibAV::init(): Unknown pixel format: 0x%08h!\n",
				_avc->pix_fmt);
		goto fail;
	}

	_initialized = true;

	return S_OK;
fail:
	if (_avc) {
		avcodec_close(_avc);
		av_freep(&_avc);
		_avc = nullptr;
	}
	return S_FAIL;
}

STATUS DecoderVideoLibAV::deinit() {
	if (_initialized == false) {
		log->printf("DecoderVideoLibAV::deinit(): already deinitialized!\n");
		return S_FAIL;
	}
	av_frame_free(&_frame);
	_frame = nullptr;
	avcodec_close(_avc);
	av_freep(&_avc);
	_avc = nullptr;

	return S_OK;
}

STATUS DecoderVideoLibAV::decodeFrame(bool &frameReady, U8 *data, U32 dataSize) {
	if (_initialized == false) {
		log->printf("DecoderVideoLibAV::decodeFrame(): not initialized!\n");
		return S_FAIL;
	}

	int frameFinished = 0, readed;
	av_init_packet(&_packet);
	_packet.data = data;
	_packet.size = dataSize;
	readed = avcodec_decode_video2(_avc, _frame, &frameFinished, &_packet);
	if (readed < 0) {
		log->printf("DecoderVideoLibAV::decodeFrame(): decode frame failed!\n");
		return S_FAIL;
	}
	if (frameFinished) {
		frameReady = true;
	} else {
		frameReady = false;
	}

	return S_OK;
}

STATUS DecoderVideoLibAV::getVideoStreamOutputFrame(VideoFrame *frame) {
	if (!_initialized) {
		log->printf("DecoderVideoLibAV::getVideoStreamOutputFrame(): not initialized!\n");
		return S_FAIL;
	}

	frame->data[0] = _frame->data[0];
	frame->data[1] = _frame->data[1];
	frame->data[2] = _frame->data[2];
	frame->data[3] = _frame->data[3];
	frame->stride[0] = _frame->linesize[0];
	frame->stride[1] = _frame->linesize[1];
	frame->stride[2] = _frame->linesize[2];
	frame->stride[3] = _frame->linesize[3];
	frame->pixelfmt = _pixelFormat;

	return S_OK;
}

} // namespace
