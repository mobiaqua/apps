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

#ifndef DEMUXER_BASE_H
#define DEMUXER_BASE_H

#include "basetypes.h"
#include "avtypes.h"

namespace MediaPLayer {

#define SEEK_BY_PERCENT  1
#define SEEK_BY_TIME     2

#pragma pack(1)

typedef struct {
	U8      *data;
	U32      dataSize;
	U32	     externalDataSize;
	S64      pts;
	bool     keyFrame;
} StreamVideoFrame;

typedef struct {
	U8      *data;
	U32      dataSize;
	U32      externalDataSize;
	void    *priv; // used for non API purposes
} StreamAudioFrame;

typedef struct {
	StreamVideoFrame      videoFrame;
	StreamAudioFrame      audioFrame;
	void                 *priv; // used for non API purposes
} StreamFrame;

typedef struct {
	CODEC_ID     codecId;
	U32          codecTag;
	FORMAT_VIDEO pixelfmt;
	U32          width;
	U32          height;
	U32          timeBaseScale;
	U32          timeBaseRate;
	U32          profileLevel;
	float        fps;
	void        *priv; // used for non API purposes
} StreamVideoInfo;

typedef struct {
	CODEC_ID     codecId;
	U32          codecTag;
	void        *priv; // used for non API purposes
} StreamAudioInfo;

#pragma pack()

class Demuxer {
protected:

	bool          _initialized;
	StreamFrame   _streamFrame;

public:

	Demuxer();
	virtual ~Demuxer() {}

	virtual STATUS init() = 0;
	virtual STATUS deinit() = 0;
	virtual STATUS openFile(const char *filename) = 0;
	virtual void closeFile() = 0;
	virtual STATUS selectVideoStream() = 0;
	virtual STATUS selectAudioStream(S32 index_audio) = 0;
	virtual STATUS seekFrame(float seek, U32 flags) = 0;
	virtual STATUS readNextFrame(StreamFrame *frame) = 0;
	virtual STATUS getVideoStreamInfo(StreamVideoInfo *info) = 0;
};

Demuxer *CreateDemuxer(DEMUXER_TYPE demuxerType);

} // namespace

#endif
