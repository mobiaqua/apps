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

#ifndef DECODER_VIDEO_BASE_H
#define DECODER_VIDEO_BASE_H

#include "basetypes.h"
#include "avtypes.h"
#include "demuxer_base.h"

namespace MediaPLayer {

typedef struct {
	U8 *data[4];
	U32 stride[4];
	FORMAT_VIDEO pixelfmt;
	U32 width, height; // target aligned width and height
	U32 dx, dy, dw, dh; // border of decoded frame data
} VideoFrame;

class DecoderVideo {
protected:

	bool _initialized;
	U32 _bpp;

public:

	DecoderVideo();
	virtual ~DecoderVideo() {}

	virtual bool isCapable(Demuxer *demuxer) = 0;
	virtual STATUS init(Demuxer *demuxer) = 0;
	virtual STATUS deinit() = 0;
	virtual STATUS decodeFrame(bool &frameReady, U8 *data, U32 dataSize) = 0;
	virtual STATUS getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *frame) = 0;
	U32 getBPP() { return _bpp; }
};

DecoderVideo *CreateDecoderVideo(DECODER_TYPE decoderType);

} // namespace

#endif
