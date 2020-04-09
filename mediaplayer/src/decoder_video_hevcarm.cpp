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
#include "decoder_video_hevcarm.h"

namespace MediaPLayer {

DecoderVideoHEVCArm::DecoderVideoHEVCArm() {
	_bpp = 2;
}

DecoderVideoHEVCArm::~DecoderVideoHEVCArm() {
	deinit();
}

bool DecoderVideoHEVCArm::isCapable(Demuxer *demuxer) {
	if (demuxer == nullptr) {
		log->printf("DecoderVideoHEVCArm::isCapable(): demuxer is NULL\n");
		return false;
	}

	return false;
}

STATUS DecoderVideoHEVCArm::init(Demuxer *demuxer, Display *display) {
	return S_OK;
}

STATUS DecoderVideoHEVCArm::deinit() {
	return S_OK;
}

STATUS DecoderVideoHEVCArm::decodeFrame(bool &frameReady, StreamFrame *streamFrame) {
	return S_OK;
}

STATUS DecoderVideoHEVCArm::getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame) {
	return S_OK;
}

} // namespace
