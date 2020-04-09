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

#ifdef __cplusplus
extern "C" {
	#include <libdce.h>
	#include <libdrm/omap_drmif.h>
}
#endif
#include <xdc/std.h>
#include <ti/xdais/dm/xdm.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video3/viddec3.h>
#include <ti/sdo/codecs/h264dec/ih264vdec.h>
#include <ti/sdo/codecs/mpeg4dec/impeg4vdec.h>
#include <ti/sdo/codecs/mpeg2vdec/impeg2vdec.h>
#include <ti/sdo/codecs/vc1vdec/ivc1vdec.h>

namespace MediaPLayer {

class DecoderVideoLibDCE : public DecoderVideo {

private:
	Engine_Handle              	_codecEngine;
	VIDDEC3_Handle             	_codecHandle;
	VIDDEC3_Params 				*_codecParams;
	VIDDEC3_DynamicParams 		*_codecDynParams;
	VIDDEC3_Status				*_codecStatus;
	XDM2_BufDesc				*_codecInputBufs;
	XDM2_BufDesc				*_codecOutputBufs;
	VIDDEC3_InArgs				*_codecInputArgs;
	VIDDEC3_OutArgs				*_codecOutputArgs;
	omap_device                 *_dceDev;
	int           				_frameWidth;
	int 						_frameHeight;
	void                        *_inputBufPtr;
	int                         _inputBufSize;
	omap_bo                     *_inputBufBo;

public:
	DecoderVideoLibDCE();
	~DecoderVideoLibDCE();

	bool isCapable(Demuxer *demuxer);
	STATUS init(Demuxer *demuxer, Display *display);
	STATUS deinit();
	void getDemuxerBuffer(StreamFrame *streamFrame);
	STATUS decodeFrame(bool &frameReady, StreamFrame *streamFrame);
	STATUS getVideoStreamOutputFrame(Demuxer *demuxer, VideoFrame *videoFrame);
	FORMAT_VIDEO getVideoFmt(Demuxer * /*demuxer*/) { return FMT_NV12; }
	int getVideoWidth(Demuxer *demuxer) { return 0; }
	int getVideoHeight(Demuxer *demuxer) { return 0; }
};

} // namespace

#endif
