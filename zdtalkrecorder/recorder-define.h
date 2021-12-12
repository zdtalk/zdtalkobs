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

#ifndef ZDTALK_RECORDER_DEFINE_H_
#define ZDTALK_RECORDER_DEFINE_H_

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#define ZDTALK_STREAMING_MAX_RETRY_TIMES   3
#define ZDTALK_STREAMING_RETRY_INTERVAL    5

#define ZDTALK_AUDIO_OUTPUT_INDEX          1
#define ZDTALK_AUDIO_INPUT_INDEX           3

#define ZDTALK_AUDIO_OUTPUT_DEVICE_ID      "default"
#define ZDTALK_AUDIO_INPUT_DEVICE_ID       "default"

#define DL_OPENGL "libobs-opengl.dll"
#define DL_D3D11  "libobs-d3d11.dll"

#define SIMPLE_ENCODER_X264         "x264"
#define SIMPLE_ENCODER_X264_LOWCPU  "x264_lowcpu"
#define SIMPLE_ENCODER_QSV          "qsv"
#define SIMPLE_ENCODER_NVENC        "nvenc"
#define SIMPLE_ENCODER_AMD          "amd"

#ifdef _WIN32
#define IS_WIN32 1
#elif
#define IS_WIN32 0
#endif

#define NEGATIVE(v) ((v) > 0 ? -(v) : (v))

enum ZDTalkRecorderEvent
{
    // Server to client
    kEventInit,
    kEventUpdateWindow,
    kEventScaleVideo,
    kEventCropVideo,
    kEventUpdateCaptureConfig,
    kEventResetAudioInput,
    kEventResetAudioOutput,
    kEventDownmixMonoInput,
    kEventDownmixMonoOutput,
    kEventMuteAudioInput,
    kEventMuteAudioOutput,
    kEventStartRecording,
    kEventStopRecording,
    kEventStartStreaming,
    kEventStopStreaming,
    kEventExit,

    // Client to server
    kEventInited,
    kEventRecordingStarted,
    kEventRecordingStopped,
    kEventStreamingStarted,
    kEventStreamingStopped,
    kEventStateNotify,
    kEventErrorOccurred,
};

enum ZDTalkRecorderError
{
    kErrorNone,
    kErrorClientStartup,
    kErrorClientInit,
    kErrorClientWindowCapture,
    kErrorClientRecording,
    kErrorClientStreaming,
    kErrorClientUnknown,
};

#endif // ZDTALK_RECORDER_DEFINE_H_
