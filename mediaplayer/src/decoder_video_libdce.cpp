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

#include "basetypes.h"
#include "logs.h"
#include "decoder_video_base.h"
#include "decoder_video_libdce.h"

namespace MediaPLayer {

DecoderVideoLibDCE::DecoderVideoLibDCE() {
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
		return false/*true*/;
	default:
		return false;
	}

	return false;
}

STATUS DecoderVideoLibDCE::init(Demuxer *demuxer) {
	return S_OK;
}

STATUS DecoderVideoLibDCE::deinit() {
	return S_OK;
}

STATUS DecoderVideoLibDCE::decodeFrame(bool &frameReady, StreamFrame *streamFrame) {
	return S_OK;
}

STATUS DecoderVideoLibDCE::getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame) {
	return S_OK;
}

} // namespace
