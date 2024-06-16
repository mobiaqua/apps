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

#ifndef DECODER_VIDEO_LIBDCE_H
#define DECODER_VIDEO_LIBDCE_H

#include "basetypes.h"
#include "decoder_video_base.h"
#include "display_base.h"

#define xdc_target_types__ gnu/targets/std.h
#include <xdc/std.h>
#include <ti/xdais/dm/xdm.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video3/viddec3.h>
#include <ti/sdo/codecs/h264vdec/ih264vdec.h>
#include <ti/sdo/codecs/mpeg4vdec/impeg4vdec.h>
#include <ti/sdo/codecs/mpeg2vdec/impeg2vdec.h>
#include <ti/sdo/codecs/vc1vdec/ivc1vdec.h>

#include <inttypes.h>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <drm/drm.h>
#include <xf86drm.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
	#include <libdce.h>
}
#endif

namespace MediaPLayer {

class DecoderVideoLibDCE : public DecoderVideo {

private:

	typedef struct {
		DisplayVideoBuffer      buffer;
		int                     index;
		bool                    locked;
	} FrameBuffer;

	Display                    *_display;
	Engine_Handle              _codecEngine;
	VIDDEC3_Handle             _codecHandle;
	VIDDEC3_Params             *_codecParams;
	VIDDEC3_DynamicParams      *_codecDynParams;
	VIDDEC3_Status             *_codecStatus;
	XDM2_BufDesc               *_codecInputBufs;
	XDM2_BufDesc               *_codecOutputBufs;
	VIDDEC3_InArgs             *_codecInputArgs;
	VIDDEC3_OutArgs            *_codecOutputArgs;
	int                        _drmFd;
	int                        _frameWidth;
	int                        _frameHeight;
	void                       *_inputBufPtr;
	int                        _inputBufSize;
	uint32_t                   _inputBufHandle;
	int                        _numFrameBuffers;
	FrameBuffer                **_frameBuffers;
	unsigned int               _codecId;

public:
	DecoderVideoLibDCE();
	~DecoderVideoLibDCE();

	bool isCapable(Demuxer *demuxer);
	STATUS init(Demuxer *demuxer, Display *display);
	STATUS deinit();
	void getDemuxerBuffer(StreamFrame *streamFrame);
	STATUS decodeFrame(bool &frameReady, StreamFrame *streamFrame);
	STATUS flush();
	STATUS getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame);
	FORMAT_VIDEO getVideoFmt(Demuxer * /*demuxer*/) { return FMT_NV12; }
	int getVideoWidth(Demuxer *demuxer);
	int getVideoHeight(Demuxer *demuxer);

private:
	FrameBuffer *getBuffer();
	void lockBuffer(FrameBuffer *fb);
	void unlockBuffer(FrameBuffer *fb);
};

} // namespace

#endif
