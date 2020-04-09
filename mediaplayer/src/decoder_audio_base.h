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

#ifndef DECODER_AUDIO_BASE_H
#define DECODER_AUDIO_BASE_H

#include "basetypes.h"
#include "avtypes.h"
#include "demuxer_base.h"

namespace MediaPLayer {

class DecoderAudio {
protected:
	bool _initialized;

public:

	DecoderAudio();
	virtual ~DecoderAudio() {}

	virtual bool isCapable(Demuxer *demuxer) = 0;
	virtual STATUS init(Demuxer *demuxer) = 0;
	virtual STATUS deinit() = 0;
	virtual STATUS decodeFrame() = 0;
};

DecoderAudio *CreateDecoderAudio(DECODER_TYPE decoderType);

} // namespace

#endif
