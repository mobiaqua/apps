/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013-2014 Pawel Kolodziejski <aquadran at users.sourceforge.net>
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
#include "decoder_audio_base.h"
#include "decoder_audio_libav.h"
#include "demuxer_base.h"

namespace MediaPLayer {

DecoderAudioLibAV::DecoderAudioLibAV() :
		_avc(NULL), _codec(NULL), _stream(NULL) {
	_initialized = false;
}

DecoderAudioLibAV::~DecoderAudioLibAV() {
	deinit();
}

bool DecoderAudioLibAV::isCapable(Demuxer *demuxer) {
	if (demuxer == NULL) {
		log->printf("DecoderAudioLibAV::isCapable(): demuxer is NULL\n");
		return false;
	}

	return false;
}

STATUS DecoderAudioLibAV::init(Demuxer *demuxer) {
	return S_OK;
}

STATUS DecoderAudioLibAV::deinit() {
	return S_OK;
}

STATUS DecoderAudioLibAV::decodeFrame() {
	return S_OK;
}

} // namespace
