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
#include "decoder_video_base.h"
#include "decoder_video_libav.h"
#include "demuxer_libav.h"

namespace MediaPLayer {

DecoderVideoLibAV::DecoderVideoLibAV() :
		_avc(nullptr), _avcodec(nullptr), _avframe(nullptr), _bsfc(nullptr) {
	_avframe = av_frame_alloc();
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
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
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

STATUS DecoderVideoLibAV::init(Demuxer *demuxer, Display *display) {
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
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibAV::init(): demuxer->getVideoStreamInfo() failed\n");
		goto fail;
	}
	_avc = static_cast<AVCodecContext *>(info.priv);
	if (_avc == nullptr) {
		log->printf("DecoderVideoLibAV::init(): avcodec context NULL\n");
		goto fail;
	}

	_avcodec = avcodec_find_decoder(_avc->codec_id);
	if (_avcodec == nullptr) {
		log->printf("DecoderVideoLibAV::init(): avcodec_find_decoder() failed\n");
		goto fail;
	}

	err = avcodec_open2(_avc, _avcodec, nullptr);
	if (err != 0) {
		log->printf("DecoderVideoLibAV::init(): avcodec_open2() failed: %d\n", err);
		goto fail;
	}

	_initialized = true;

	return S_OK;
fail:
	if (_avc) {
		avcodec_close(_avc);
		_avc = nullptr;
	}
	return S_FAIL;
}

STATUS DecoderVideoLibAV::deinit() {
	if (_initialized == false) {
		return S_OK;
	}
	av_frame_free(&_avframe);
	_avframe = nullptr;

	if (_bsfc) {
		 av_bsf_free(&_bsfc);
		_bsfc = nullptr;
	}

	avcodec_close(_avc);
	_avc = nullptr;

	return S_OK;
}

void DecoderVideoLibAV::getDemuxerBuffer(StreamFrame *streamFrame) {
	if (streamFrame == nullptr) {
		return;
	}
	streamFrame->videoFrame.data = nullptr;
	streamFrame->videoFrame.dataSize = 0;
	streamFrame->videoFrame.externalDataSize = 0;
}

STATUS DecoderVideoLibAV::decodeFrame(bool &frameReady, StreamFrame *streamFrame) {
	if (_initialized == false) {
		log->printf("DecoderVideoLibAV::decodeFrame(): not initialized!\n");
		return S_FAIL;
	}

	if (streamFrame == nullptr) {
		log->printf("DecoderVideoLibAV::decodeFrame(): null streamFrame!\n");
		return S_FAIL;
	}

	int status;
	status = avcodec_send_packet(_avc, static_cast<AVPacket *>(streamFrame->priv)) != 0;
	if (status != 0) {
		log->printf("DecoderVideoLibAV::decodeFrame(): avcodec_send_packet failed, status: %d\n", status);
		return S_FAIL;
	}
	status = avcodec_receive_frame(_avc, _avframe);
	if (status < 0 && status != AVERROR(EAGAIN) && status != AVERROR_EOF) {
		log->printf("DecoderVideoLibAV::decodeFrame(): avcodec_receive_frame failed, status: %d\n", status);
		return S_FAIL;
	} else if (status == 0) {
		frameReady = true;
	}

	return S_OK;
}

STATUS DecoderVideoLibAV::getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame) {
	if (!_initialized) {
		log->printf("DecoderVideoLibAV::getVideoStreamOutputFrame(): not initialized!\n");
		return S_FAIL;
	}

	if (demuxer == nullptr || videoFrame == nullptr) {
		log->printf("DecoderVideoLibAV::getVideoStreamOutputFrame(): wrong arguments\n");
		return S_FAIL;
	}

	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibAV::init(): demuxer->getVideoStreamInfo() failed\n");
		return S_FAIL;
	}

	videoFrame->pixelfmt = info.pixelfmt;
	videoFrame->data[0] = _avframe->data[0];
	videoFrame->data[1] = _avframe->data[1];
	videoFrame->data[2] = _avframe->data[2];
	videoFrame->data[3] = _avframe->data[3];
	videoFrame->stride[0] = _avframe->linesize[0];
	videoFrame->stride[1] = _avframe->linesize[1];
	videoFrame->stride[2] = _avframe->linesize[2];
	videoFrame->stride[3] = _avframe->linesize[3];
	videoFrame->width = _avframe->width;
	videoFrame->height = _avframe->height;
	videoFrame->dx = 0;
	videoFrame->dy = 0;
	videoFrame->dw = info.width;
	videoFrame->dh = info.height;
	videoFrame->hw = false;

	return S_OK;
}

FORMAT_VIDEO DecoderVideoLibAV::getVideoFmt(Demuxer *demuxer)
{
	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibAV::getVideoFmt(): demuxer->getVideoStreamInfo() failed\n");
		return FMT_NONE;
	}

	return info.pixelfmt;
}

int DecoderVideoLibAV::getVideoWidth(Demuxer *demuxer)
{
	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibAV::getVideoWidth(): demuxer->getVideoStreamInfo() failed\n");
		return 0;
	}

	return info.width;
}

int DecoderVideoLibAV::getVideoHeight(Demuxer *demuxer)
{
	StreamVideoInfo info;
	if (demuxer->getVideoStreamInfo(&info) != S_OK) {
		log->printf("DecoderVideoLibAV::getVideoHeight(): demuxer->getVideoStreamInfo() failed\n");
		return 0;
	}

	return info.height;
}

} // namespace
