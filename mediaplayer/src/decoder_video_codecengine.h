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

#ifndef DECODER_VIDEO_CODECENGINE_H
#define DECODER_VIDEO_CODECENGINE_H

#include "basetypes.h"
#include "demuxer_base.h"
#include "decoder_video_base.h"

namespace MediaPLayer {

class DecoderVideoCodecEngine : public DecoderVideo {
public:
	DecoderVideoCodecEngine();
	~DecoderVideoCodecEngine();

	bool isCapable(Demuxer *demuxer);
	STATUS init(Demuxer *demuxer, Display *display);
	STATUS deinit();
	void getDemuxerBuffer(StreamFrame *streamFrame);
	STATUS decodeFrame(bool &frameReady, StreamFrame *streamFrame);
	STATUS flush() { return S_OK; }
	STATUS getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame);
	FORMAT_VIDEO getVideoFmt(Demuxer * /*demuxer*/) { return FMT_YUV420P; }
	int getVideoWidth(Demuxer *demuxer) { return 0; }
	int getVideoHeight(Demuxer *demuxer) { return 0; }
};

} // namespace

#endif
