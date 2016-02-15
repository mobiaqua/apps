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

#include <assert.h>

#include "basetypes.h"
#include "avtypes.h"
#include "decoder_video_base.h"
#include "decoder_video_libav.h"
#include "decoder_video_libdce.h"
#include "decoder_video_codecengine.h"

namespace MediaPLayer {

DecoderVideo::DecoderVideo() :
		_initialized(false), _bpp(0), _pixelFormat(FMT_NONE) {
	assert(false);
};

DecoderVideo::~DecoderVideo() {
	assert(false);
}

DecoderVideo *CreateDecoderVideo(DECODER_TYPE decoderType) {
	switch (decoderType) {
	case DECODER_LIBAV:
		return new DecoderVideoLibAV();
	case DECODER_LIBDCE:
		return new DecoderVideoLibDCE();
	case DECODER_CODEC_ENGINE:
		return new DecoderVideoCodecEngine();
	default:
		return NULL;
	}
}

} // namespace
