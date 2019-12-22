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

#ifndef DEMUXER_LIBAV_H
#define DEMUXER_LIBAV_H

#include "basetypes.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace MediaPLayer {

class DemuxerLibAV : public Demuxer {
private:

	AVFormatContext            *_afc;
	AVStream                   *_videoStream;
	AVStream                   *_audioStream;
	S64                         _pts;
	AVPacket                    _packedFrame;
	StreamVideoInfo 			_videoStreamInfo;
	AVBSFContext               *_bsf;

public:
	DemuxerLibAV();
	virtual ~DemuxerLibAV();

	virtual STATUS init();
	virtual STATUS deinit();
	virtual STATUS openFile(const char *filename);
	virtual void closeFile();
	virtual STATUS selectVideoStream();
	virtual STATUS selectAudioStream(S32 index_audio);
	virtual STATUS seekFrame(float seek, U32 flags);
	virtual STATUS readNextFrame(StreamFrame *frame);
	virtual STATUS getVideoStreamInfo(StreamVideoInfo *info);
};

} // namespace

#endif
