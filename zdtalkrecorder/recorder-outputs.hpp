/******************************************************************************
    Copyright (C) 2020 by Zaodao(Dalian) Education Technology Co., Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#ifndef ZDTALK_RECORDER_OUTPUTS_H_
#define ZDTALK_RECORDER_OUTPUTS_H_

#include "obs.h"
#include "obs.hpp"

#include <string>

class RecorderObsContext;

struct BasicOutputHandler 
{
    OBSEncoder             h264Streaming;
    OBSEncoder             aacStreaming;
    std::string            aacStreamEncID;

    OBSEncoder             h264Recording;
    OBSEncoder             aacRecording;
    std::string            aacRecEncID;

    bool                   ffmpegOutput;

    OBSOutput              streamOutput;
	OBSOutput              fileOutput;

	bool                   streamingActive = false;
	bool                   recordingActive = false;

	OBSSignal              startRecording;
	OBSSignal              stopRecording;
	OBSSignal              startStreaming;
	OBSSignal              stopStreaming;
	OBSSignal              streamStopping;
	OBSSignal              recordStopping;

    uint64_t               lastBytesSent;
    uint64_t               lastBytesSentTime;
    int                    firstTotal;
    int                    firstDropped;

    RecorderObsContext *context_;

    BasicOutputHandler(RecorderObsContext *context);

	~BasicOutputHandler() {};

	bool StartStreaming(obs_service_t *service);
	bool StartRecording();
	void StopStreaming(bool force = true);
	void StopRecording(bool force = true);
	bool StreamingActive() const;
	bool RecordingActive() const;

    int GetAudioBitrate() const;
    OBSData GetStreamEncSettings();

    void UpdateStreamingSettings(obs_service_t *service);
    void UpdateRecordingSettings();

    int CalcCRF(int crf);
    void UpdateRecordingSettings_x264_crf(int crf);
    void UpdateRecordingSettings_qsv11(int crf);
    void UpdateRecordingSettings_nvenc(int cqp);
    void UpdateRecordingSettings_amd_cqp(int cqp);
    void UpdateRecordingSettings_ffmpeg();

    void LoadRecordingPreset_h264(const char *encoder);
    void LoadStreamingPreset_h264(const char *encoder);

    void LogStreaming();

	inline bool Active() const {
		return streamingActive || recordingActive;
	}
};

#endif // ZDTALK_RECORDER_OUTPUTS_H_
