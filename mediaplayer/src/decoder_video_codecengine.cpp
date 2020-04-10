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
#include "decoder_video_codecengine.h"

namespace MediaPLayer {

DecoderVideoCodecEngine::DecoderVideoCodecEngine() {
}

DecoderVideoCodecEngine::~DecoderVideoCodecEngine() {
	deinit();
}

bool DecoderVideoCodecEngine::isCapable(Demuxer *demuxer) {
	if (demuxer == nullptr) {
		log->printf("DecoderVideoCodecEngine::isCapable(): demuxer is NULL\n");
		return false;
	}

	return false;
}

STATUS DecoderVideoCodecEngine::init(Demuxer *demuxer, Display *display) {
	return S_OK;
}

STATUS DecoderVideoCodecEngine::deinit() {
	return S_OK;
}

void DecoderVideoCodecEngine::getDemuxerBuffer(StreamFrame *streamFrame) {
	if (streamFrame == nullptr) {
		return;
	}
	streamFrame->videoFrame.data = nullptr;
	streamFrame->videoFrame.dataSize = 0;
	streamFrame->videoFrame.externalDataSize = 0;
}

STATUS DecoderVideoCodecEngine::decodeFrame(bool &frameReady, StreamFrame *streamFrame) {
	return S_OK;
}

STATUS DecoderVideoCodecEngine::getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame) {
	return S_OK;
}

} // namespace
