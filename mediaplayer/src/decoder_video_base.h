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

#ifndef DECODER_VIDEO_BASE_H
#define DECODER_VIDEO_BASE_H

#include "basetypes.h"
#include "avtypes.h"
#include "demuxer_base.h"

namespace MediaPLayer {

#pragma pack(1)

typedef struct {
	U8 *data[4]; // array of pointers for video planes
	U32 stride[4]; // array of widths of video planes in bytes
	FORMAT_VIDEO pixelfmt; // pixel format of decoded video frame
	U32 width, height; // target aligned width and height
	U32 dx, dy, dw, dh; // border of decoded frame data
	bool hw;
} VideoFrame;

#pragma pack()

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
	virtual STATUS decodeFrame(bool &frameReady, StreamFrame *streamFrame) = 0;
	virtual STATUS getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame) = 0;
	U32 getBPP() { return _bpp; }
	virtual FORMAT_VIDEO getVideoFmt(Demuxer *demuxer) = 0;
	virtual int getVideoWidth(Demuxer *demuxer) = 0;
	virtual int getVideoHeight(Demuxer *demuxer) = 0;
};

DecoderVideo *CreateDecoderVideo(DECODER_TYPE decoderType);

} // namespace

#endif
