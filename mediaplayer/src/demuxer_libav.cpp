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

#include "basetypes.h"
#include "logs.h"
#include "demuxer_base.h"
#include "demuxer_libav.h"

namespace MediaPLayer {

DemuxerLibAV::DemuxerLibAV() :
		_afc(nullptr), _videoStream(nullptr), _audioStream(nullptr),
		_bsf(nullptr), _pts(0) {
	memset(&_packedFrame, 0, sizeof(AVPacket));
	memset(&_streamFrame, 0, sizeof(StreamFrame));
}

DemuxerLibAV::~DemuxerLibAV() {
	deinit();
}

STATUS DemuxerLibAV::init() {
	return S_OK;
}

STATUS DemuxerLibAV::deinit() {
	closeFile();

	return S_OK;
}

STATUS DemuxerLibAV::openFile(const char *filename) {
	int err;

	if (_initialized) {
		log->printf("DemuxerLibAV::openFile(): demuxer is allready open!\n");
		return S_FAIL;
	}

	av_register_all();

	err = avformat_open_input(&_afc, filename, nullptr, nullptr);
	if (err < 0) {
		log->printf("DemuxerLibAV::openFile(): avformat_open_input error %d\n", err);
		return S_FAIL;
	}

	err = avformat_find_stream_info(_afc, nullptr);
	if (err < 0) {
		log->printf("DemuxerLibAV::openFile(): avformat_find_stream_info error %d\n", err);
		return S_FAIL;
	}

	av_dump_format(_afc, 0, filename, 0);

	av_init_packet(&_packedFrame);

	_initialized = true;
	return S_OK;
}

void DemuxerLibAV::closeFile() {
	if (_initialized) {
		log->printf("DemuxerLibAV::closeFile(): demuxer allready closed!\n");
		return;
	}

	if (_bsf && _streamFrame.videoFrame.data) {
		av_free(_streamFrame.videoFrame.data);
		_streamFrame.videoFrame.data = nullptr;
	}

	av_packet_unref(&_packedFrame);
	avformat_close_input(&_afc);

	if (_bsf) {
		av_bitstream_filter_close(_bsf);
		_bsf = nullptr;
	}
}

STATUS DemuxerLibAV::selectVideoStream() {
	if (!_initialized) {
		log->printf("DemuxerLibAV::selectVideoStream(): demuxer not opened!\n");
		return S_FAIL;
	}

	for (U32 i = 0; i < _afc->nb_streams; i++) {
		if (_afc->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			_videoStream = _afc->streams[i];
			AVCodecContext *cc = _videoStream->codec;
			if (cc->codec_id == AV_CODEC_ID_H264) {
				if (cc->extradata && cc->extradata_size > 0 && cc->extradata[0] == 1) {
					_bsf = av_bitstream_filter_init("h264_mp4toannexb");
					if (_bsf == nullptr) {
						log->printf("DemuxerLibAV::selectVideoStream(): av_bitstream_filter_init failed!\n");
						return S_FAIL;
					}
				}
			}
			return S_OK;
		}
	}

	return S_FAIL;
}

STATUS DemuxerLibAV::selectAudioStream(U32 index_audio) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::selectAudioStream(): demuxer not initialized!\n");
		return S_FAIL;
	}

	U32 count_audio = 0;
	for (U32 i = 0; i < _afc->nb_streams; i++) {
		if (_afc->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (count_audio++ == index_audio) {
				_audioStream = _afc->streams[i];
				return S_OK;
			}
		}
	}

	return S_FAIL;
}

STATUS DemuxerLibAV::seekFrame(float seek, U32 flags) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::seekFrame(): demuxer not opened!\n");
		return S_FAIL;
	}

	int seek_flags = 0;
	if (flags & SEEK_BY_PERCENT) {
		if (_afc->duration == (int64_t)AV_NOPTS_VALUE || _afc->duration == 0) {
			log->printf("DemuxerLibAV::seekFrame(SEEK_BY_PERCENT): failed. null duration in stream!\n");
			return S_FAIL;
		}
		if (seek < 0.0 || seek > 100.0) {
			log->printf("DemuxerLibAV::seekFrame(SEEK_BY_PERCENT): wrong range of seek: %f!\n", seek);
			return S_FAIL;
		}
		_pts += (seek / 100) * _afc->duration;
	} else if (flags & SEEK_BY_TIME) {
		if (_afc->start_time != (int64_t)AV_NOPTS_VALUE) {
			_pts = _afc->start_time;
		} else {
			_pts = 0;
		}
		_pts += seek * AV_TIME_BASE;
		if (_pts < 0) {
			_pts = 0;
		} else {
			if (seek < 0.0) {
				seek_flags = AVSEEK_FLAG_BACKWARD;
			}
		}
	} else {
		return S_FAIL;
	}

	if (av_seek_frame(_afc, -1, _pts, seek_flags) < 0) {
		log->printf("DemuxerLibAV::seekFrame(): av_seek_frame failed!\n");
		return S_FAIL;
	}

	return S_OK;
}

STATUS DemuxerLibAV::readNextFrame(StreamFrame &frame) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::getNextFrame(): demuxer not opened!\n");
		return S_FAIL;
	}

	if (_bsf && _streamFrame.videoFrame.data) {
		av_free(_streamFrame.videoFrame.data);
		_streamFrame.videoFrame.data = nullptr;
	}

	_streamFrame.videoFrame.data = nullptr;
	_streamFrame.videoFrame.dataSize = 0;

	if (av_read_frame(_afc, &_packedFrame) == 0) {
		if (_packedFrame.stream_index == _videoStream->index) {
			if (_bsf) {
				int err = av_bitstream_filter_filter(_bsf, _videoStream->codec,
						nullptr, &_streamFrame.videoFrame.data, (S32 *)&_streamFrame.videoFrame.dataSize,
						_packedFrame.data, _packedFrame.size, 0);
				if (err < 0) {
					log->printf("DemuxerLibAV::getNextFrame(): av_bitstream_filter_filter failed!\n");
					return S_FAIL;
				}
			} else {
				_streamFrame.videoFrame.pts = _packedFrame.pts * av_q2d(_videoStream->time_base);
				_streamFrame.videoFrame.keyFrame = (_packedFrame.flags & AV_PKT_FLAG_KEY) != 0;
				_streamFrame.videoFrame.dataSize = _packedFrame.size;
				_streamFrame.videoFrame.data = (U8 *)malloc(_packedFrame.size);
				memcpy(_streamFrame.videoFrame.data, _packedFrame.data, _packedFrame.size);
				av_packet_unref(&_packedFrame);
			}
			memcpy(&frame, &_streamFrame, sizeof(StreamFrame));
			return S_OK;
		} else if (_packedFrame.stream_index == _audioStream->index) {
			// TODO
			memcpy(&frame, &_streamFrame, sizeof(StreamFrame));
			return S_OK;
		} else {
			av_packet_unref(&_packedFrame);
		}
	}

	return S_FAIL;
}

STATUS DemuxerLibAV::getVideoStreamInfo(StreamVideoInfo &info) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::getVideoStreamInfo(): demuxer not opened!\n");
		return S_FAIL;
	}
	if (_videoStream == nullptr) {
		log->printf("DemuxerLibAV::getVideoStreamInfo(): video stream null!\n");
		return S_FAIL;
	}

	info.width = (U32)_videoStream->codec->width;
	info.height = (U32)_videoStream->codec->height;
	info.timeBaseScale = (U32)_videoStream->codec->time_base.num;
	info.timeBaseRate = (U32)_videoStream->codec->time_base.den;

	switch (_videoStream->codec->codec_id) {
	case AV_CODEC_ID_MPEG1VIDEO:
		info.codecId = CODEC_ID_MPEG1VIDEO;
		break;
	case AV_CODEC_ID_MPEG2VIDEO:
		info.codecId = CODEC_ID_MPEG2VIDEO;
		break;
	case AV_CODEC_ID_H261:
		info.codecId = CODEC_ID_H261;
		break;
	case AV_CODEC_ID_H263:
		info.codecId = CODEC_ID_H263;
		break;
	case AV_CODEC_ID_MPEG4:
		info.codecId = CODEC_ID_MPEG4;
		break;
	case AV_CODEC_ID_MSMPEG4V1:
		info.codecId = CODEC_ID_MSMPEG4V1;
		break;
	case AV_CODEC_ID_MSMPEG4V2:
		info.codecId = CODEC_ID_MSMPEG4V2;
		break;
	case AV_CODEC_ID_MSMPEG4V3:
		info.codecId = CODEC_ID_MSMPEG4V3;
		break;
	case AV_CODEC_ID_H263P:
		info.codecId = CODEC_ID_H263P;
		break;
	case AV_CODEC_ID_H263I:
		info.codecId = CODEC_ID_H263I;
		break;
	case AV_CODEC_ID_FLV1:
		info.codecId = CODEC_ID_FLV1;
		break;
	case AV_CODEC_ID_SVQ1:
		info.codecId = CODEC_ID_SVQ1;
		break;
	case AV_CODEC_ID_SVQ3:
		info.codecId = CODEC_ID_SVQ3;
		break;
	case AV_CODEC_ID_AIC:
		info.codecId = CODEC_ID_AIC;
		break;
	case AV_CODEC_ID_DVVIDEO:
		info.codecId = CODEC_ID_DVVIDEO;
		break;
	case AV_CODEC_ID_VP3:
		info.codecId = CODEC_ID_VP3;
		break;
	case AV_CODEC_ID_VP5:
		info.codecId = CODEC_ID_VP5;
		break;
	case AV_CODEC_ID_VP6:
		info.codecId = CODEC_ID_VP6;
		break;
	case AV_CODEC_ID_VP6A:
		info.codecId = CODEC_ID_VP6A;
		break;
	case AV_CODEC_ID_VP6F:
		info.codecId = CODEC_ID_VP6F;
		break;
	case AV_CODEC_ID_VP7:
		info.codecId = CODEC_ID_VP7;
		break;
	case AV_CODEC_ID_VP8:
		info.codecId = CODEC_ID_VP8;
		break;
	case AV_CODEC_ID_VP9:
		info.codecId = CODEC_ID_VP9;
		break;
	case AV_CODEC_ID_WEBP:
		info.codecId = CODEC_ID_WEBP;
		break;
	case AV_CODEC_ID_THEORA:
		info.codecId = CODEC_ID_THEORA;
		break;
	case AV_CODEC_ID_RV10:
		info.codecId = CODEC_ID_RV10;
		break;
	case AV_CODEC_ID_RV20:
		info.codecId = CODEC_ID_RV20;
		break;
	case AV_CODEC_ID_RV30:
		info.codecId = CODEC_ID_RV30;
		break;
	case AV_CODEC_ID_RV40:
		info.codecId = CODEC_ID_RV40;
		break;
	case AV_CODEC_ID_WMV1:
		info.codecId = CODEC_ID_WMV1;
		break;
	case AV_CODEC_ID_WMV2:
		info.codecId = CODEC_ID_WMV2;
		break;
	case AV_CODEC_ID_WMV3:
		info.codecId = CODEC_ID_WMV3;
		break;
	case AV_CODEC_ID_VC1:
		info.codecId = CODEC_ID_VC1;
		break;
	case AV_CODEC_ID_H264:
		info.codecId = CODEC_ID_H264;
		break;
	case AV_CODEC_ID_HEVC:
		info.codecId = CODEC_ID_HEVC;
		break;
	default:
		info.codecId = CODEC_ID_NONE;
		log->printf("DemuxerLibAV::getVideoStreamCodecId(): Unknown codec: 0x%08h!\n",
				_videoStream->codec->codec_id);
		return S_FAIL;
	}

	return S_OK;
}

STATUS DemuxerLibAV::getVideoStreamExtraData(U32 &size, U8 **data) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::getVideoStreamExtraData(): demuxer not opened!\n");
		return S_FAIL;
	}

	if (_videoStream == nullptr) {
		log->printf("DemuxerLibAV::getVideoStreamExtraData(): video stream null!\n");
		return S_FAIL;
	}

	size = (U32)_videoStream->codec->extradata_size;
	*data = _videoStream->codec->extradata;

	return S_OK;
}

} // namespace
