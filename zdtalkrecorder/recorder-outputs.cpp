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

#include "recorder-outputs.hpp"
#include "recorder-app.h"
#include "recorder-audio-encoders.hpp"
#include "recorder-define.h"
#include "recorder-obs-context.h"

#include <libavcodec/avcodec.h>
#include <util/platform.h>
#include <util/util.hpp>

#include <string>
#include <algorithm>
using namespace std;

static bool CreateAACEncoder(OBSEncoder &res, string &id, int bitrate,
    const char *name, size_t idx)
{
    const char *id_ = GetAACEncoderForBitrate(bitrate);
    if (!id_) {
        id.clear();
        res = nullptr;
        return false;
    }

    if (id == id_)
        return true;

    id = id_;
    res = obs_audio_encoder_create(id_, name, nullptr, idx, nullptr);

    if (res) {
        obs_encoder_release(res);
        return true;
    }

    return false;
}

static bool icq_available(obs_encoder_t *encoder)
{
    obs_properties_t *props = obs_encoder_properties(encoder);
    obs_property_t *p = obs_properties_get(props, "rate_control");
    bool icq_found = false;

    size_t num = obs_property_list_item_count(p);
    for (size_t i = 0; i < num; i++) {
        const char *val = obs_property_list_item_string(p, i);
        if (strcmp(val, "ICQ") == 0) {
            icq_found = true;
            break;
        }
    }

    obs_properties_destroy(props);
    return icq_found;
}

/* ------------------------------- Signals --------------------------------- */

static void OBSStreamStarting(void *data, calldata_t *params)
{
    BasicOutputHandler *output = static_cast<BasicOutputHandler*>(data);
    obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");

    int sec = (int)obs_output_get_active_delay(obj);
    QMetaObject::invokeMethod(output->context_, "StreamingStarting", 
        Q_ARG(int, sec));
    blog(LOG_INFO, "=============== Streaming Starting ===============");
}

static void OBSStreamStopping(void *data, calldata_t *params)
{
    BasicOutputHandler *output = static_cast<BasicOutputHandler*>(data);
    obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");

    int sec = (int)obs_output_get_active_delay(obj);
    QMetaObject::invokeMethod(output->context_, "StreamingStopping",
        Q_ARG(int, sec));
    blog(LOG_INFO, "=============== Streaming Stopping ===============");
}

static void OBSStartStreaming(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	output->streamingActive = true;
    QMetaObject::invokeMethod(output->context_, "StreamingStarted");

	UNUSED_PARAMETER(params);
    blog(LOG_INFO, "=============== Streaming Started ===============");
}

static void OBSStopStreaming(void *data, calldata_t *params)
{
    blog(LOG_INFO, "=============== Streaming Stopped ===============");
	BasicOutputHandler *output = static_cast<BasicOutputHandler*>(data);
    output->streamingActive = false;
    QString msg;
    int code = (int)calldata_int(params, "code");
    const char *last_error = calldata_string(params, "last_error");
    switch (code)
    {
    case OBS_OUTPUT_SUCCESS:
        blog(LOG_INFO, "Streaming finished!");
        break;
    case OBS_OUTPUT_BAD_PATH:
        msg = QObject::tr("无效的地址！");
        break;
    case OBS_OUTPUT_CONNECT_FAILED:
        msg = QObject::tr("无法连接到服务器！");
        break;
    case OBS_OUTPUT_INVALID_STREAM:
        msg = QObject::tr("无法访问流密钥或无法连接服务器！");
        break;
    case OBS_OUTPUT_ERROR:
        msg = QObject::tr("连接服务器时发生意外错误！");
        break;
    case OBS_OUTPUT_DISCONNECTED:
        msg = QObject::tr("已与服务器断开连接！");
        break;
    default:
        blog(LOG_ERROR, "Stream error, code = %d, error = %s.",
            code, last_error);
        msg = QObject::tr("发生未指定错误") + QString("Code:%1)！").arg(code);
        break;
    }

    if (!msg.isEmpty()) {
        QMetaObject::invokeMethod(output->context_, "ErrorOccurred",
            Q_ARG(int, kErrorClientStreaming), Q_ARG(QString, msg));
    } else {
        QMetaObject::invokeMethod(output->context_, "StreamingStopped");
    }
}

static void OBSStartRecording(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler*>(data);
	output->recordingActive = true;
    QMetaObject::invokeMethod(output->context_, "RecordingStarted");

	UNUSED_PARAMETER(params);
    blog(LOG_INFO, "=============== Recording Started ===============");
}

static void OBSStopRecording(void *data, calldata_t *params)
{
    blog(LOG_INFO, "=============== Recording Stopped ===============");
	BasicOutputHandler *output = static_cast<BasicOutputHandler*>(data);
    output->recordingActive = false;
    QString msg;
    int code = (int)calldata_int(params, "code");
    const char *last_error = calldata_string(params, "last_error");
    switch (code)
    {
    case OBS_OUTPUT_SUCCESS:
        blog(LOG_INFO, "Recording finished!");
        break;
    case OBS_OUTPUT_NO_SPACE:
        msg = QObject::tr("磁盘存储空间不足！");
        break;
    case OBS_OUTPUT_UNSUPPORTED:
        msg = QObject::tr("格式不支持！");
        break;
    default:
        blog(LOG_ERROR, "Record error, code = %d, error = %s.",
            code, last_error);
        msg = QObject::tr("发生未指定错误") + QString("Code:%1)！").arg(code);
        break;
    }

    if (!msg.isEmpty()) {
        QMetaObject::invokeMethod(output->context_, "ErrorOccurred",
            Q_ARG(int, kErrorClientRecording), Q_ARG(QString, msg));
    }
    else {
        const char *filePath =
            config_get_string(App()->GetGlobalConfig(), "Output", "FilePath");
        QMetaObject::invokeMethod(output->context_, "RecordingStopped",
            Q_ARG(QString, QString::fromUtf8(filePath)));
    }
}

static void OBSRecordStopping(void *data, calldata_t *params)
{
    BasicOutputHandler *output = static_cast<BasicOutputHandler*>(data);
    QMetaObject::invokeMethod(output->context_, "RecordingStopping");
    UNUSED_PARAMETER(params);
    blog(LOG_INFO, "=============== Recording Stopping ===============");
}

/* ------------------------------- Output --------------------------------- */

BasicOutputHandler::BasicOutputHandler(RecorderObsContext *context) :
    context_(context)
{
    const char *encoder = config_get_string(App()->GetGlobalConfig(),
        "Output", "StreamEncoder");
    if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0)
        LoadStreamingPreset_h264("obs_qsv11");
    else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0)
        LoadStreamingPreset_h264("amd_amf_h264");
    else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0)
        LoadStreamingPreset_h264("ffmpeg_nvenc");
    else
        LoadStreamingPreset_h264("obs_x264");

    if (!CreateAACEncoder(aacStreaming, aacStreamEncID, GetAudioBitrate(),
        "streaming_aac", 0)) {
        blog(LOG_ERROR, "Failed to create aac streaming encoder");
        throw "Failed to create aac streaming encoder";
    }

    const char *recMode = config_get_string(App()->GetGlobalConfig(),
        "Output", "RecMode");
    ffmpegOutput = strcmp(recMode, "ffmpeg") == 0;

    if (ffmpegOutput) {
        fileOutput = obs_output_create("ffmpeg_output",
            "ffmpeg_output", nullptr, nullptr);
        if (!fileOutput) {
            blog(LOG_ERROR, "Failed to create recording ffmpeg output");
            throw "Failed to create recording ffmpeg output";
        }
        obs_output_release(fileOutput);
    } else {
        const char *encoder = config_get_string(App()->GetGlobalConfig(),
            "Output", "RecEncoder");
        if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0)
            LoadRecordingPreset_h264("obs_qsv11");
        else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0)
            LoadRecordingPreset_h264("amd_amf_h264");
        else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0)
            LoadRecordingPreset_h264("ffmpeg_nvenc");
        else
            LoadRecordingPreset_h264("obs_x264");

        aacRecording = aacStreaming;
        aacRecEncID = aacStreamEncID;

        fileOutput = obs_output_create("ffmpeg_muxer",
            "ffmpeg_muxer", nullptr, nullptr);
        if (!fileOutput) {
            blog(LOG_ERROR, "Failed to create recording muxer output");
            throw "Failed to create recording muxer output";
        }
        obs_output_release(fileOutput);
    }

    streamOutput = obs_output_create("rtmp_output",
        "streaming_output", nullptr, nullptr);
    if (!streamOutput) {
        blog(LOG_ERROR, "Failed to create streaming rtmp output");
        throw "Failed to create streaming rtmp output";
    }
    obs_output_release(streamOutput);

    startRecording.Connect(obs_output_get_signal_handler(fileOutput),
        "start", OBSStartRecording, this);
    stopRecording.Connect(obs_output_get_signal_handler(fileOutput),
        "stop", OBSStopRecording, this);
    recordStopping.Connect(obs_output_get_signal_handler(fileOutput),
        "stopping", OBSRecordStopping, this);

    startStreaming.Connect(obs_output_get_signal_handler(streamOutput),
        "start", OBSStartStreaming, this);
    stopStreaming.Connect(obs_output_get_signal_handler(streamOutput),
        "stop", OBSStopStreaming, this);
    streamStopping.Connect(obs_output_get_signal_handler(streamOutput),
        "stopping", OBSStreamStopping, this);
}

bool BasicOutputHandler::StartStreaming(obs_service_t *service)
{
    UpdateStreamingSettings(service);

    if (obs_output_start(streamOutput)) {
        firstTotal = obs_output_get_total_frames(streamOutput);
        firstDropped = obs_output_get_frames_dropped(streamOutput);
        blog(LOG_INFO, "First total:%d, dropped:%d.", firstTotal, firstDropped);
        return true;
    }

    const char *error = obs_output_get_last_error(streamOutput);
    bool has_last_error = error && *error;

    blog(LOG_WARNING, "Stream output failed to start!%s%s",
        has_last_error ? "  Last Error: " : "",
        has_last_error ? error : "");
    return false;
}

bool BasicOutputHandler::StartRecording()
{
    UpdateRecordingSettings();

    if (!obs_output_start(fileOutput))  {
        blog(LOG_ERROR, "Recording start failed. %s.",
            obs_output_get_last_error(fileOutput));
        return false;
    }

    return true;
}

void BasicOutputHandler::StopStreaming(bool force)
{
    if (force)
        obs_output_force_stop(streamOutput);
    else
        obs_output_stop(streamOutput);
}

void BasicOutputHandler::StopRecording(bool force)
{
    if (force)
        obs_output_force_stop(fileOutput);
    else
        obs_output_stop(fileOutput);
}

bool BasicOutputHandler::StreamingActive() const
{
    return obs_output_active(streamOutput);
}

bool BasicOutputHandler::RecordingActive() const
{
    return obs_output_active(fileOutput);
}

OBSData BasicOutputHandler::GetStreamEncSettings()
{
    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "preset", "medium");
    obs_data_set_string(settings, "tune", "stillimage");
    obs_data_set_string(settings, "x264opts", "");
    obs_data_set_string(settings, "rate_control", "CRF");
    obs_data_set_int(settings, "crf", 22);
    obs_data_set_bool(settings, "vfr", false);
    obs_data_set_string(settings, "profile", "main");
    obs_data_set_int(settings, "keyint_sec", 10);

    OBSData dataRet(settings);
    obs_data_release(settings);
    return dataRet;
}

void BasicOutputHandler::UpdateStreamingSettings(obs_service_t *service)
{
    obs_encoder_set_scaled_size(h264Streaming, 0, 0);

    obs_encoder_set_video(h264Streaming, obs_get_video());
    obs_encoder_set_audio(aacStreaming, obs_get_audio());

    obs_data_t *audioSettings = obs_data_create();
    obs_data_set_int(audioSettings, "bitrate", GetAudioBitrate());
    obs_data_release(audioSettings);
    obs_service_apply_encoder_settings(service, GetStreamEncSettings(), audioSettings);

    obs_output_set_video_encoder(streamOutput, h264Streaming);
    obs_output_set_audio_encoder(streamOutput, aacStreaming, 0);

    obs_output_set_reconnect_settings(streamOutput, 0, 0);
    obs_output_set_service(streamOutput, service);
}

void BasicOutputHandler::LoadRecordingPreset_h264(const char *encoderId)
{
    h264Recording = obs_video_encoder_create(encoderId,
        "recording_h264", nullptr, nullptr);
    if (!h264Recording) {
        blog(LOG_ERROR, "Failed to create recording h264 encoder");
        throw "Failed to create recording h264 encoder";
    }
    obs_encoder_release(h264Recording);
}

void BasicOutputHandler::LoadStreamingPreset_h264(const char *encoderId)
{
    h264Streaming = obs_video_encoder_create(encoderId,
        "streaming_h264", nullptr, nullptr);
    if (!h264Streaming) {
        blog(LOG_ERROR, "Failed to create streaming h264 encoder");
        throw "Failed to create streaming h264 encoder";
    }
    obs_encoder_release(h264Streaming);
}

#define CROSS_DIST_CUTOFF 2000.0

int BasicOutputHandler::CalcCRF(int crf)
{
    int cx = config_get_uint(App()->GetGlobalConfig(), "Video", "OutputCX");
    int cy = config_get_uint(App()->GetGlobalConfig(), "Video", "OutputCY");
    double fCX = double(cx);
    double fCY = double(cy);

    double crossDist = sqrt(fCX * fCX + fCY * fCY);
    double crfResReduction =
        fmin(CROSS_DIST_CUTOFF, crossDist) / CROSS_DIST_CUTOFF;
    crfResReduction = (1.0 - crfResReduction) * 10.0;

    return crf - int(crfResReduction);
}

void BasicOutputHandler::UpdateRecordingSettings_x264_crf(int crf)
{
    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "crf", crf);
    obs_data_set_bool(settings, "use_bufsize", true);
    obs_data_set_string(settings, "rate_control", "CRF");
    obs_data_set_string(settings, "profile", "main");
    obs_data_set_string(settings, "preset", "veryfast");

    obs_encoder_update(h264Recording, settings);

    obs_data_release(settings);
}

void BasicOutputHandler::UpdateRecordingSettings_qsv11(int crf)
{
    bool icq = icq_available(h264Recording);

    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "profile", "main");

    if (icq) {
        obs_data_set_string(settings, "rate_control", "ICQ");
        obs_data_set_int(settings, "icq_quality", crf);
    } else {
        obs_data_set_string(settings, "rate_control", "CQP");
        obs_data_set_int(settings, "qpi", crf);
        obs_data_set_int(settings, "qpp", crf);
        obs_data_set_int(settings, "qpb", crf);
    }

    obs_encoder_update(h264Recording, settings);

    obs_data_release(settings);
}

void BasicOutputHandler::UpdateRecordingSettings_nvenc(int cqp)
{
    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "rate_control", "CQP");
    obs_data_set_string(settings, "profile", "main");
    obs_data_set_string(settings, "preset", "hq");
    obs_data_set_int(settings, "cqp", cqp);

    obs_encoder_update(h264Recording, settings);

    obs_data_release(settings);
}

void BasicOutputHandler::UpdateRecordingSettings_amd_cqp(int cqp)
{
    obs_data_t *settings = obs_data_create();

    // Static Properties
    obs_data_set_int(settings, "Usage", 0);
    obs_data_set_int(settings, "Profile", 100); // High

    // Rate Control Properties
    obs_data_set_int(settings, "RateControlMethod", 0);
    obs_data_set_int(settings, "QP.IFrame", cqp);
    obs_data_set_int(settings, "QP.PFrame", cqp);
    obs_data_set_int(settings, "QP.BFrame", cqp);
    obs_data_set_int(settings, "VBVBuffer", 1);
    obs_data_set_int(settings, "VBVBuffer.Size", 100000);

    // Picture Control Properties
    obs_data_set_double(settings, "KeyframeInterval", 2.0);
    obs_data_set_int(settings, "BFrame.Pattern", 0);

    // Update and release
    obs_encoder_update(h264Recording, settings);
    obs_data_release(settings);
}

void BasicOutputHandler::UpdateRecordingSettings()
{
    if (ffmpegOutput) {
        UpdateRecordingSettings_ffmpeg();
        return;
    }

    int crf = 22;
    const char *videoEncoder = config_get_string(App()->GetGlobalConfig(),
        "Output", "RecEncoder");
    if (strcmp(videoEncoder, SIMPLE_ENCODER_QSV) == 0)
        UpdateRecordingSettings_qsv11(crf);
    else if (strcmp(videoEncoder, SIMPLE_ENCODER_AMD) == 0)
        UpdateRecordingSettings_amd_cqp(crf);
    else if (strcmp(videoEncoder, SIMPLE_ENCODER_NVENC) == 0)
        UpdateRecordingSettings_nvenc(crf);
    else
        UpdateRecordingSettings_x264_crf(crf);

    obs_encoder_set_video(h264Recording, obs_get_video());
    obs_encoder_set_audio(aacRecording, obs_get_audio());

    obs_output_set_video_encoder(fileOutput, h264Recording);
    obs_output_set_audio_encoder(fileOutput, aacRecording, 0);

    const char *path = config_get_string(App()->GetGlobalConfig(),
        "Output", "FilePath");

    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "path", path);
    obs_data_set_string(settings, "muxer_settings", "movflags = faststart");
    obs_output_update(fileOutput, settings);
    obs_data_release(settings);
}

void BasicOutputHandler::UpdateRecordingSettings_ffmpeg()
{
    obs_data_t *settings = obs_data_create();

    const char *path = config_get_string(App()->GetGlobalConfig(),
        "Output", "FilePath");
    const char *recFormat = config_get_string(App()->GetGlobalConfig(),
        "Output", "RecFormat");
    if (!recFormat) 
        recFormat = "mp4";
    int cx = config_get_uint(App()->GetGlobalConfig(), "Video", "OutputCX");
    int cy = config_get_uint(App()->GetGlobalConfig(), "Video", "OutputCY");
    int fps = config_get_uint(App()->GetGlobalConfig(), "Video", "FPSInt");
    int aBitrate = config_get_uint(App()->GetGlobalConfig(), "Output", "ABitrate");
    int vBitrate = config_get_uint(App()->GetGlobalConfig(), "Output", "VBitrate");

    obs_data_set_string(settings, "url", path);
    if (strcmp(recFormat, "mp4") == 0) {
        obs_data_set_string(settings, "muxer_settings", "movflags=faststart");
        obs_data_set_string(settings, "format_name", "mp4");
        obs_data_set_string(settings, "format_mime_type", "video/mp4");
        obs_data_set_string(settings, "video_encoder", "libx264");
        obs_data_set_int(settings, "video_encoder_id", AV_CODEC_ID_H264);
        obs_data_set_string(settings, "video_settings", "profile=main x264-params=crf=22");
    } else if (strcmp(recFormat, "flv") == 0 ) {
        obs_data_set_string(settings, "format_name", "flv");
        obs_data_set_string(settings, "format_mime_type", "video/x-flv");
        obs_data_set_string(settings, "video_encoder", "flv");
        obs_data_set_int(settings, "video_encoder_id", AV_CODEC_ID_FLV1);
        obs_data_set_int(settings, "video_bitrate", vBitrate);
    }
    obs_data_set_int(settings, "gop_size", fps * 10);
    obs_data_set_int(settings, "audio_bitrate", aBitrate);
    obs_data_set_string(settings, "audio_encoder", "aac");
    obs_data_set_int(settings, "audio_encoder_id", AV_CODEC_ID_AAC);
    obs_data_set_string(settings, "audio_settings", NULL);

    obs_data_set_int(settings, "scale_width", cx);
    obs_data_set_int(settings, "scale_height", cy);

    obs_output_set_mixer(fileOutput, 0/*aTrack - 1*/);
    obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
    obs_output_update(fileOutput, settings);

    obs_data_release(settings);
}

int BasicOutputHandler::GetAudioBitrate() const
{
    int bitrate = (int)config_get_uint(App()->GetGlobalConfig(), "Output",
        "ABitrate");

    return FindClosestAvailableAACBitrate(bitrate);
}

void BasicOutputHandler::LogStreaming()
{
    if (!streamOutput)
        return;

    uint64_t totalBytes = obs_output_get_total_bytes(streamOutput);
    uint64_t curTime = os_gettime_ns();
    uint64_t bytesSent = totalBytes;

    if (bytesSent < lastBytesSent)
        bytesSent = 0;
    if (bytesSent == 0)
        lastBytesSent = 0;

    uint64_t bitsBetween = (bytesSent - lastBytesSent) * 8;
    long double timePassed = (long double)(curTime - lastBytesSentTime) /
        1000000000.0l;
    long double kbps = (long double)bitsBetween / timePassed / 1000.0l;

    if (timePassed < 0.01l)
        kbps = 0.0l;

    long double num = 0;
    int total = obs_output_get_total_frames(streamOutput);
    int dropped = obs_output_get_frames_dropped(streamOutput);
    if (total < firstTotal || dropped < firstDropped) {
        firstTotal = 0;
        firstDropped = 0;
    }
    total -= firstTotal;
    dropped -= firstDropped;
    num = total ? (long double)dropped / (long double)total * 100.0l : 0.0l;

    blog(LOG_INFO, "Streaming => bitrate:%.2lf kb/s, frames:%d / %d (%.2lf%%).",
        kbps, dropped, total, num);

    lastBytesSent = bytesSent;
    lastBytesSentTime = curTime;
}