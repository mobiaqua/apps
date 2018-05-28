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
	if (_initialized) {
		log->printf("DemuxerLibAV::openFile(): demuxer is allready open!\n");
		return S_FAIL;
	}

	int err = avformat_open_input(&_afc, filename, nullptr, nullptr);
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

	_initialized = true;
	return S_OK;
}

void DemuxerLibAV::closeFile() {
	if (_initialized) {
		return;
	}

	if (_streamFrame.videoFrame.data) {
		av_free(_streamFrame.videoFrame.data);
		_streamFrame.videoFrame.data = nullptr;
	}

	av_packet_unref(&_packedFrame);
	avformat_close_input(&_afc);
}

STATUS DemuxerLibAV::selectVideoStream() {
	if (!_initialized) {
		log->printf("DemuxerLibAV::selectVideoStream(): demuxer not opened!\n");
		return S_FAIL;
	}

	for (U32 i = 0; i < _afc->nb_streams; i++) {
		AVStream *stream = _afc->streams[i];
		AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
		if (codec == NULL) {
			log->printf("DemuxerLibAV::selectVideoStream(): avcodec_find_decoder failed!\n");
			return S_FAIL;
		}
		AVCodecContext *cc = avcodec_alloc_context3(codec);
		if (cc == NULL) {
			log->printf("DemuxerLibAV::selectVideoStream(): avcodec_alloc_context3 failed!\n");
			return S_FAIL;
		}
		if (avcodec_parameters_to_context(cc, stream->codecpar) < 0) {
			log->printf("DemuxerLibAV::selectVideoStream(): avcodec_alloc_context3 failed!\n");
			avcodec_free_context(&cc);
			return S_FAIL;
		}
		if (cc->codec_type == AVMEDIA_TYPE_VIDEO) {
			_videoStream = stream;
			if (cc->codec_id == AV_CODEC_ID_H264) {
				if (cc->extradata && cc->extradata_size >= 8 && cc->extradata[0] == 1) {
					const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
					if (bsf == nullptr) {
						log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_get_by_name failed!\n");
						avcodec_free_context(&cc);
						return S_FAIL;
					}
				    if (av_bsf_alloc(bsf, &_bsf) < 0) {
						log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_alloc failed!\n");
						avcodec_free_context(&cc);
						return S_FAIL;
					}
				    if (avcodec_parameters_from_context(_bsf->par_in, cc) < 0)
				    {
						log->printf("DemuxerLibAV::selectVideoStream(): avcodec_parameters_from_context failed!\n");
						av_bsf_free(&_bsf);
						avcodec_free_context(&cc);
						return S_FAIL;
				    }
				    _bsf->time_base_in = cc->time_base;
					if (av_bsf_init(_bsf) < 0) {
						log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_init failed!\n");
						av_bsf_free(&_bsf);
						avcodec_free_context(&cc);
						return S_FAIL;
					}
				}
			} else if (cc->codec_id == AV_CODEC_ID_MPEG4) {
				const AVBitStreamFilter *bsf = av_bsf_get_by_name("mpeg4_unpack_bframes");
				if (bsf == nullptr) {
					log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_get_by_name failed!\n");
					avcodec_free_context(&cc);
					return S_FAIL;
				}
			    if (av_bsf_alloc(bsf, &_bsf) < 0) {
					log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_alloc failed!\n");
					avcodec_free_context(&cc);
					return S_FAIL;
				}
			    if (avcodec_parameters_from_context(_bsf->par_in, cc) < 0)
			    {
					log->printf("DemuxerLibAV::selectVideoStream(): avcodec_parameters_from_context failed!\n");
					av_bsf_free(&_bsf);
					avcodec_free_context(&cc);
					return S_FAIL;
			    }
			    _bsf->time_base_in = cc->time_base;
				if (av_bsf_init(_bsf) < 0) {
					log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_init failed!\n");
					av_bsf_free(&_bsf);
					avcodec_free_context(&cc);
					return S_FAIL;
				}
			} else if (cc->codec_id == AV_CODEC_ID_HEVC) {
				if (cc->extradata && cc->extradata_size >= 8 && cc->extradata[0] == 1) {
					const AVBitStreamFilter *bsf = av_bsf_get_by_name("hevc_mp4toannexb");
					if (bsf == nullptr) {
						log->printf("DemuxerLibAV::selectVideoStream(): av_bitstream_filter_init failed!\n");
						avcodec_free_context(&cc);
						return S_FAIL;
					}
				    if (av_bsf_alloc(bsf, &_bsf) < 0) {
						log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_alloc failed!\n");
						avcodec_free_context(&cc);
						return S_FAIL;
					}
				    if (avcodec_parameters_from_context(_bsf->par_in, cc) < 0)
				    {
						log->printf("DemuxerLibAV::selectVideoStream(): avcodec_parameters_from_context failed!\n");
						av_bsf_free(&_bsf);
						avcodec_free_context(&cc);
						return S_FAIL;
				    }
				    _bsf->time_base_in = cc->time_base;
					if (av_bsf_init(_bsf) < 0) {
						log->printf("DemuxerLibAV::selectVideoStream(): av_bsf_init failed!\n");
						av_bsf_free(&_bsf);
						avcodec_free_context(&cc);
						return S_FAIL;
					}
				}
			}

			_videoStreamInfo.width = (U32)cc->width;
			_videoStreamInfo.height = (U32)cc->height;
			_videoStreamInfo.timeBaseScale = (U32)cc->time_base.num;
			_videoStreamInfo.timeBaseRate = (U32)cc->time_base.den;
			_videoStreamInfo.priv = cc;

			switch (cc->codec_id) {
			case AV_CODEC_ID_MPEG1VIDEO:
				_videoStreamInfo.codecId = CODEC_ID_MPEG1VIDEO;
				break;
			case AV_CODEC_ID_MPEG2VIDEO:
				_videoStreamInfo.codecId = CODEC_ID_MPEG2VIDEO;
				break;
			case AV_CODEC_ID_H261:
				_videoStreamInfo.codecId = CODEC_ID_H261;
				break;
			case AV_CODEC_ID_H263:
				_videoStreamInfo.codecId = CODEC_ID_H263;
				break;
			case AV_CODEC_ID_MPEG4:
				_videoStreamInfo.codecId = CODEC_ID_MPEG4;
				break;
			case AV_CODEC_ID_MSMPEG4V1:
				_videoStreamInfo.codecId = CODEC_ID_MSMPEG4V1;
				break;
			case AV_CODEC_ID_MSMPEG4V2:
				_videoStreamInfo.codecId = CODEC_ID_MSMPEG4V2;
				break;
			case AV_CODEC_ID_MSMPEG4V3:
				_videoStreamInfo.codecId = CODEC_ID_MSMPEG4V3;
				break;
			case AV_CODEC_ID_H263P:
				_videoStreamInfo.codecId = CODEC_ID_H263P;
				break;
			case AV_CODEC_ID_H263I:
				_videoStreamInfo.codecId = CODEC_ID_H263I;
				break;
			case AV_CODEC_ID_FLV1:
				_videoStreamInfo.codecId = CODEC_ID_FLV1;
				break;
			case AV_CODEC_ID_SVQ1:
				_videoStreamInfo.codecId = CODEC_ID_SVQ1;
				break;
			case AV_CODEC_ID_SVQ3:
				_videoStreamInfo.codecId = CODEC_ID_SVQ3;
				break;
			case AV_CODEC_ID_AIC:
				_videoStreamInfo.codecId = CODEC_ID_AIC;
				break;
			case AV_CODEC_ID_DVVIDEO:
				_videoStreamInfo.codecId = CODEC_ID_DVVIDEO;
				break;
			case AV_CODEC_ID_VP3:
				_videoStreamInfo.codecId = CODEC_ID_VP3;
				break;
			case AV_CODEC_ID_VP5:
				_videoStreamInfo.codecId = CODEC_ID_VP5;
				break;
			case AV_CODEC_ID_VP6:
				_videoStreamInfo.codecId = CODEC_ID_VP6;
				break;
			case AV_CODEC_ID_VP6A:
				_videoStreamInfo.codecId = CODEC_ID_VP6A;
				break;
			case AV_CODEC_ID_VP6F:
				_videoStreamInfo.codecId = CODEC_ID_VP6F;
				break;
			case AV_CODEC_ID_VP7:
				_videoStreamInfo.codecId = CODEC_ID_VP7;
				break;
			case AV_CODEC_ID_VP8:
				_videoStreamInfo.codecId = CODEC_ID_VP8;
				break;
			case AV_CODEC_ID_VP9:
				_videoStreamInfo.codecId = CODEC_ID_VP9;
				break;
			case AV_CODEC_ID_WEBP:
				_videoStreamInfo.codecId = CODEC_ID_WEBP;
				break;
			case AV_CODEC_ID_THEORA:
				_videoStreamInfo.codecId = CODEC_ID_THEORA;
				break;
			case AV_CODEC_ID_RV10:
				_videoStreamInfo.codecId = CODEC_ID_RV10;
				break;
			case AV_CODEC_ID_RV20:
				_videoStreamInfo.codecId = CODEC_ID_RV20;
				break;
			case AV_CODEC_ID_RV30:
				_videoStreamInfo.codecId = CODEC_ID_RV30;
				break;
			case AV_CODEC_ID_RV40:
				_videoStreamInfo.codecId = CODEC_ID_RV40;
				break;
			case AV_CODEC_ID_WMV1:
				_videoStreamInfo.codecId = CODEC_ID_WMV1;
				break;
			case AV_CODEC_ID_WMV2:
				_videoStreamInfo.codecId = CODEC_ID_WMV2;
				break;
			case AV_CODEC_ID_WMV3:
				_videoStreamInfo.codecId = CODEC_ID_WMV3;
				break;
			case AV_CODEC_ID_VC1:
				_videoStreamInfo.codecId = CODEC_ID_VC1;
				break;
			case AV_CODEC_ID_H264:
				_videoStreamInfo.codecId = CODEC_ID_H264;
				break;
			case AV_CODEC_ID_HEVC:
				_videoStreamInfo.codecId = CODEC_ID_HEVC;
				break;
			default:
				_videoStreamInfo.codecId = CODEC_ID_NONE;
				log->printf("DemuxerLibAV::selectVideoStream(): Unknown codec: 0x%08x!\n",
						cc->codec_id);
				avcodec_free_context(&cc);
				return S_FAIL;
			}

			switch (cc->pix_fmt) {
			case AV_PIX_FMT_RGB24:
				_videoStreamInfo.pixelfmt = FMT_RGB24;
				break;
			case AV_PIX_FMT_ARGB:
				_videoStreamInfo.pixelfmt = FMT_ARGB;
				break;
			case AV_PIX_FMT_YUV420P:
				_videoStreamInfo.pixelfmt = FMT_YUV420P;
				break;
			case AV_PIX_FMT_YUV422P:
				_videoStreamInfo.pixelfmt = FMT_YUV422P;
				break;
			case AV_PIX_FMT_YUV444P:
				_videoStreamInfo.pixelfmt = FMT_YUV444P;
				break;
			case AV_PIX_FMT_NV12:
				_videoStreamInfo.pixelfmt = FMT_NV12;
				break;
			default:
				_videoStreamInfo.pixelfmt = FMT_NONE;
				log->printf("DemuxerLibAV::selectVideoStream(): Unknown pixel format: 0x%08x!\n", cc->pix_fmt);
				avcodec_free_context(&cc);
				return S_FAIL;
			}
			return S_OK;
		}
	}

	return S_FAIL;
}

STATUS DemuxerLibAV::selectAudioStream(S32 index_audio) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::selectAudioStream(): demuxer not initialized!\n");
		return S_FAIL;
	}

	S32 count_audio = 0;
	for (U32 i = 0; i < _afc->nb_streams; i++) {
		AVStream *stream = _afc->streams[i];
		AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
		if (codec == NULL) {
			log->printf("DemuxerLibAV::selectAudioStream(): avcodec_find_decoder failed!\n");
			return S_FAIL;
		}
		AVCodecContext *cc = avcodec_alloc_context3(codec);
		if (cc == NULL) {
			log->printf("DemuxerLibAV::selectAudioStream(): avcodec_alloc_context3 failed!\n");
			return S_FAIL;
		}
		if (avcodec_parameters_to_context(cc, stream->codecpar) < 0) {
			log->printf("DemuxerLibAV::selectAudioStream(): avcodec_alloc_context3 failed!\n");
			avcodec_free_context(&cc);
			return S_FAIL;
		}
		if (cc->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (count_audio++ == index_audio || index_audio == -1) {
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
		_pts += (seek / 100.0) * _afc->duration;
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

STATUS DemuxerLibAV::readNextFrame(StreamFrame *frame) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::getNextFrame(): demuxer not opened!\n");
		return S_FAIL;
	}

	if (_streamFrame.videoFrame.data) {
		av_free(_streamFrame.videoFrame.data);
	}
	if (_streamFrame.audioFrame.data) {
		av_free(_streamFrame.audioFrame.data);
	}

	memset(&_streamFrame, 0, sizeof(StreamFrame));

	if (av_read_frame(_afc, &_packedFrame) == 0) {
		if (_packedFrame.stream_index == _videoStream->index) {
			if (_bsf) {
				if (av_bsf_send_packet(_bsf, &_packedFrame) < 0) {
					log->printf("DemuxerLibAV::getNextFrame(): av_bsf_send_packet failed!\n");
					av_packet_unref(&_packedFrame);
					return S_FAIL;
				}
				if (av_bsf_receive_packet(_bsf, &_packedFrame) < 0) {
					log->printf("DemuxerLibAV::getNextFrame(): av_bsf_receive_packet failed!\n");
					av_packet_unref(&_packedFrame);
					return S_FAIL;
				}
			}
			_streamFrame.videoFrame.pts = _packedFrame.pts * av_q2d(_videoStream->time_base);
			_streamFrame.videoFrame.keyFrame = (_packedFrame.flags & AV_PKT_FLAG_KEY) != 0;
			_streamFrame.videoFrame.dataSize = _packedFrame.size;
			_streamFrame.videoFrame.data = (U8 *)av_malloc(_packedFrame.size + AV_INPUT_BUFFER_PADDING_SIZE);
			memcpy(_streamFrame.videoFrame.data, _packedFrame.data, _packedFrame.size);
			memset(_streamFrame.videoFrame.data + _packedFrame.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
			_streamFrame.priv = &_packedFrame;
		} else if (_packedFrame.stream_index == _audioStream->index) {
			_streamFrame.audioFrame.dataSize = _packedFrame.size;
			_streamFrame.audioFrame.data = (U8 *)av_malloc(_packedFrame.size + AV_INPUT_BUFFER_PADDING_SIZE);
			memcpy(_streamFrame.audioFrame.data, _packedFrame.data, _packedFrame.size);
			memset(_streamFrame.audioFrame.data + _packedFrame.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
			_streamFrame.priv = &_packedFrame;
		}

		memcpy(frame, &_streamFrame, sizeof(StreamFrame));
		return S_OK;
	}

	return S_FAIL;
}

STATUS DemuxerLibAV::getVideoStreamInfo(StreamVideoInfo *info) {
	if (!_initialized) {
		log->printf("DemuxerLibAV::getVideoStreamInfo(): demuxer not opened!\n");
		return S_FAIL;
	}
	if (_videoStream == nullptr) {
		log->printf("DemuxerLibAV::getVideoStreamInfo(): video stream null!\n");
		return S_FAIL;
	}

	memcpy(info, &_videoStreamInfo, sizeof(StreamVideoInfo));

	return S_OK;
}

} // namespace
