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

#include "recorder-app.h"
#include "recorder-platform.h"
#include "recorder-define.h"

#include "obs.h"
#include <libavcodec/avcodec.h>

#define INPUT_AUDIO_SOURCE  "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"

ZDTalkRecorder::ZDTalkRecorder(int &argc, char **argv, profiler_name_store_t *store) :
#ifdef TEST
    QApplication(argc, argv),
#else
    QApplication(argc, argv, false),
#endif
    profilerNameStore(store)
{
#ifdef _WIN32
    DisableAudioDucking(true);

    uint32_t ver = GetWindowsVersion();
    if (ver > 0 && ver < 0x602) {
        qInfo("Enable Aero.");
        SetAeroEnabled(true);
    }
#endif

#ifndef TEST
    client_ = new RecorderClient;
#endif
}

ZDTalkRecorder::~ZDTalkRecorder()
{
#ifdef _WIN32
    DisableAudioDucking(false);
#endif

#ifndef TEST
	delete client_;
#endif

	global_config_.Save();

    QApplication::sendPostedEvents(this);
}

const char * ZDTalkRecorder::InputAudioSource() const
{
    return INPUT_AUDIO_SOURCE;
}

const char * ZDTalkRecorder::OutputAudioSource() const
{
    return OUTPUT_AUDIO_SOURCE;
}

int ZDTalkRecorder::Init(const QString &server, const QString &config)
{
    if (!InitGlobalConfig(config))
		return NEGATIVE(kErrorClientStartup);

#ifndef TEST
    client_->ConnectToServer(server);
#endif

    return kErrorNone;
}

QString ZDTalkRecorder::GetValidBasePath()
{
    QString base_path = QCoreApplication::applicationDirPath();
#ifndef TEST
    base_path += "/obs";
#endif
    return base_path;
}

bool ZDTalkRecorder::InitGlobalConfig(const QString &path)
{
	const QString base_path = GetValidBasePath();
	QString config_path = path;
	if (path.isEmpty())
		config_path = base_path + "/data/config.ini";
    int errorcode = global_config_.Open(config_path.toUtf8().constData(),
        CONFIG_OPEN_ALWAYS);
    if (errorcode != CONFIG_SUCCESS) {
        qWarning("Failed to open %s: %d", config_path.toUtf8().constData(),
            errorcode);
        return false;
	} else
		qInfo() << "Global config path:" << config_path;

    if (config_has_user_value(global_config_, "General", "BasePath")) {
        const char *old_path = config_get_string(global_config_,
            "General", "BasePath");
        if (base_path != old_path) {
            config_set_string(global_config_, "General", "BasePath",
                base_path.toUtf8().constData());
        }
	} 
	
	if (!config_has_user_value(global_config_, "Output", "RecFormat")) {
		config_set_int(global_config_, "Video", "AdapterIdx", 0);
		config_set_int(global_config_, "Video", "OutputCX", 1280);
		config_set_int(global_config_, "Video", "OutputCY", 720);
		config_set_uint(global_config_, "Video", "FPSInt", 15);
		config_set_string(global_config_, "Video", "ScaleType", "bicubic");
		config_set_int(global_config_, "Audio", "SampleRate", 44100);
		config_set_string(global_config_, "Output", "RecFormat", "mp4");
		config_set_string(global_config_, "Output", "StreamEncoder",
			SIMPLE_ENCODER_X264);
		config_set_uint(global_config_, "Output", "VBitrate", 150);
		config_set_uint(global_config_, "Output", "ABitrate", 128);
	}

	config_remove_value(global_config_, "Output", "FilePath");
    config_set_default_string(global_config_, "General", "BasePath",
        base_path.toUtf8().constData());
    
    InitDefaultsGlobalConfig();

    return true;
}

void ZDTalkRecorder::InitDefaultsGlobalConfig()
{
    if (!global_config_) return;

    config_set_default_string(global_config_, "General", "Language", "en-US");

    config_set_default_string(global_config_, "Output", "RecMode", "ffmpeg");

    config_set_default_bool(global_config_, "Output", "Reconnect", false);

    config_set_default_string(global_config_, "Output", "StreamEncoder", 
        SIMPLE_ENCODER_X264);
    config_set_default_string(global_config_, "Output", "RecEncoder", 
        SIMPLE_ENCODER_X264);

    config_set_default_uint(global_config_, "Output", "VBitrate", 150);
    config_set_default_uint(global_config_, "Output", "ABitrate", 128);
    config_set_default_string(global_config_, "Output",  "Preset", "veryfast");

    // Video -------------------------------------------------------------------
    config_set_default_int(global_config_, "Video", "AdapterIdx", 0);
    config_set_default_int(global_config_, "Video", "BaseCX", 1920);
    config_set_default_int(global_config_, "Video", "BaseCY", 1080);
    config_set_default_int(global_config_, "Video", "OutputCX", 1280);
    config_set_default_int(global_config_, "Video", "OutputCY", 720);
    config_set_default_uint(global_config_, "Video", "FPSInt", 15);
    // bilinear:fastest and takes the fewest resources, but doesn't look as good
    // bicubic
    // lanczos:takes more resources but looks better
    config_set_default_string(global_config_, "Video", "ScaleType", "bicubic");
    config_set_default_string(global_config_, "Video", "ColorFormat", "I420");
    config_set_default_string(global_config_, "Video", "ColorSpace", "601");
    config_set_default_string(global_config_, "Video", "ColorRange", "Partial");

    // Audio -------------------------------------------------------------------
    config_set_default_int(global_config_, "Audio", "SampleRate", 44100);
    config_set_default_string(global_config_, "Audio", "ChannelSetup", "Stereo");
}