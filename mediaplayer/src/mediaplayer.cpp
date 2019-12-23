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

#include <unistd.h>
#include <stdlib.h>

#include "basetypes.h"
#include "avtypes.h"
#include "logs.h"
#include "display_base.h"
#include "audio_base.h"
#include "demuxer_base.h"
#include "decoder_video_base.h"
#include "decoder_audio_base.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace MediaPLayer {

int Player(int argc, char *argv[]) {
	int option;
	const char *filename;
	Display *display = nullptr;
	Audio *audio = nullptr;
	Demuxer *demuxer = nullptr;
	DecoderVideo *decoderVideo = nullptr;
	DecoderAudio *decoderAudio = nullptr;
	bool hwAccel = false;

	if (CreateLogs() == S_FAIL)
		goto end;


	while ((option = getopt(argc, argv, ":")) != -1) {
		switch (option) {
		default:
			break;
		}
	}

	if (optind < argc) {
		filename = argv[optind];
		log->printf("\ninput file: %s\n\n", filename);
	} else {
		log->printf("Missing filename param!\n");
		goto end;
	}

	demuxer = CreateDemuxer(DEMUXER_LIBAV);
	if (demuxer == nullptr) {
		log->printf("Failed get handle to libav demuxer!\n");
		goto end;
	}

	if (demuxer->openFile(filename) == S_FAIL) {
		log->printf("Failed open file with demuxer!\n");
		goto end;
	}
	if (demuxer->selectVideoStream() == S_FAIL) {
		log->printf("Failed select video stream by demuxer!\n");
		goto end;
	}
	if (demuxer->selectAudioStream(-1) == S_FAIL) {
		log->printf("No audio stream!\n");
	}

	decoderVideo = CreateDecoderVideo(DECODER_LIBDCE);
	if (decoderVideo == nullptr) {
		log->printf("Failed get handle to libdce decoder!\n");
	} else if (!decoderVideo->isCapable(demuxer)) {
		delete decoderVideo;
		decoderVideo = nullptr;
	} else {
		hwAccel = true;
	}
	decoderVideo = CreateDecoderVideo(DECODER_LIBAV);
	if (decoderVideo == nullptr) {
		log->printf("Failed get handle to video decoder!\n");
		goto end;
	} else if (!decoderVideo->isCapable(demuxer)) {
		delete decoderVideo;
		decoderVideo = nullptr;
		log->printf("Failed get capable video decoder!\n");
		goto end;
	}
	if (decoderVideo->init(demuxer) == S_FAIL) {
		log->printf("Failed get init video decoder!\n");
		goto end;
	}

	decoderAudio = CreateDecoderAudio(DECODER_LIBAV);
	if (decoderAudio == nullptr) {
		log->printf("Failed get handle to audio decoder!\n");
		goto end;
	}
	if (decoderAudio->init(demuxer) == S_FAIL) {
		log->printf("Failed get init audio decoder!\n");
		goto end;
	}

	display = CreateDisplay(DISPLAY_OMAPDRM);
	if (display == nullptr) {
		log->printf("Failed get handle to OAMP DRM display!\n");
		goto end;
	}
	if (display->init() == S_FAIL) {
		log->printf("Failed init OMAP DRM display!\n");
		delete display;
		display = CreateDisplay(DISPLAY_FBDEV);
		if (display == nullptr) {
			log->printf("Failed get handle to FBDEV display!\n");
			goto end;
		}
		if (display->init() == S_FAIL) {
			log->printf("Failed init FBDEV display!\n");
			goto end;
		}
	}
	if (display->configure(decoderVideo->getVideoFmt(demuxer), 25) == S_FAIL) {
		log->printf("Failed configure display!\n");
		goto end;
	}

	audio = CreateAudio(AUDIO_ALSA);
	if (audio == nullptr) {
		log->printf("Failed get handle to audio Alsa!\n");
		goto end;
	}

	StreamFrame inputFrame;
	while (demuxer->readNextFrame(&inputFrame) == S_OK) {
		bool frameReady = false;
		if (inputFrame.videoFrame.data != nullptr) {
			if (decoderVideo->decodeFrame(frameReady, &inputFrame) != S_OK) {
				log->printf("Failed decode frame!\n");
				break;
			}
		}

		if (frameReady) {
			VideoFrame outputFrame{};
			if (decoderVideo->getVideoStreamOutputFrame(demuxer, &outputFrame) != S_OK) {
				log->printf("Failed get decoded frame!\n");
				break;
			}

			if (display->putImage(&outputFrame) == S_FAIL) {
				log->printf("Failed configure display!\n");
				break;
			}

			if (display->flip() == S_FAIL) {
				log->printf("Failed flip display!\n");
				break;
			}
		}
	}

end:
	delete decoderVideo;
	delete decoderAudio;
	delete demuxer;
	delete display;
	delete audio;
	delete log;

	return 0;
}

} // namespace

int main(int argc, char *argv[]) {
	return MediaPLayer::Player(argc, argv);
}

