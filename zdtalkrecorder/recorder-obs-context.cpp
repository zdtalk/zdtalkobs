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

#include "recorder-obs-context.h"
#include "recorder-app.h"
#include "recorder-define.h"
#include "recorder-platform.h"

#include <fstream>
#include <iostream>
#include <mutex>
using std::string;

#include <libavcodec/avcodec.h>
#include <util/platform.h>
#include <util/util.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

static inline enum obs_scale_type GetScaleType(const char *scale_type)
{
	if (strcmp(scale_type, "bilinear") == 0)
		return OBS_SCALE_BILINEAR;
	else if (strcmp(scale_type, "lanczos") == 0)
		return OBS_SCALE_LANCZOS;
	else
		return OBS_SCALE_BICUBIC;
}

static inline enum video_format GetVideoFormatFromName(const char *name)
{
	if (strcmp(name, "I420") == 0)
		return VIDEO_FORMAT_I420;
	else if (strcmp(name, "NV12") == 0)
		return VIDEO_FORMAT_NV12;
	else if (strcmp(name, "I444") == 0)
		return VIDEO_FORMAT_I444;
#if 0 //currently unsupported
	else if (strcmp(name, "YVYU") == 0)
		return VIDEO_FORMAT_YVYU;
	else if (strcmp(name, "YUY2") == 0)
		return VIDEO_FORMAT_YUY2;
	else if (strcmp(name, "UYVY") == 0)
		return VIDEO_FORMAT_UYVY;
#endif
	else
		return VIDEO_FORMAT_RGBA;
}

static bool EncoderAvailable(const char *encoder)
{
	const char *val;
	int i = 0;

	while (obs_enum_encoder_types(i++, &val))
		if (strcmp(val, encoder) == 0)
			return true;

	return false;
}

static void DetectEncoder()
{
	const char *stream_encoder = config_get_string(App()->GetGlobalConfig(),
		"Output", "StreamEncoder");
	const char *rec_encoder = config_get_string(App()->GetGlobalConfig(),
		"Output", "RecEncoder");
	bool reset_stream_encoder = false;
	bool reset_rec_encoder = false;

	if (EncoderAvailable("obs_qsv11"))
		blog(LOG_INFO, "Hardware.QSV is available.");
	else {
		blog(LOG_INFO, "Hardware.QSV is unavailable.");
		reset_stream_encoder = strcmp(stream_encoder, SIMPLE_ENCODER_QSV) == 0;
		reset_rec_encoder = strcmp(rec_encoder, SIMPLE_ENCODER_QSV) == 0;
	}

	if (EncoderAvailable("ffmpeg_nvenc"))
		blog(LOG_INFO, "Hardware.NVENC is available.");
	else {
		blog(LOG_INFO, "Hardware.NVENC is unavailable.");
		reset_stream_encoder = reset_stream_encoder ||
			strcmp(stream_encoder, SIMPLE_ENCODER_NVENC) == 0;
		reset_rec_encoder = reset_rec_encoder ||
			strcmp(rec_encoder, SIMPLE_ENCODER_NVENC) == 0;
	}

	if (EncoderAvailable("amd_amf_h264"))
		blog(LOG_INFO, "Hardware.AMD is available.");
	else {
		blog(LOG_INFO, "Hardware.AMD is unavailable.");
		reset_stream_encoder = reset_stream_encoder ||
			strcmp(stream_encoder, SIMPLE_ENCODER_AMD) == 0;
		reset_rec_encoder = reset_rec_encoder ||
			strcmp(rec_encoder, SIMPLE_ENCODER_AMD) == 0;
	}

	if (reset_stream_encoder) {
		qWarning("Reset stream encoder from %s to %s", stream_encoder,
			SIMPLE_ENCODER_X264);
		config_set_string(App()->GetGlobalConfig(),
			"Output", "StreamEncoder", SIMPLE_ENCODER_X264);
	}
	if (reset_rec_encoder) {
		qWarning("Reset record encoder from %s to %s", rec_encoder,
			SIMPLE_ENCODER_X264);
		config_set_string(App()->GetGlobalConfig(),
			"Output", "RecEncoder", SIMPLE_ENCODER_X264);
	}
}

static bool AddFilterToSource(obs_source_t *source, const char *id,
	const char *name)
{
	if (!source || (!id || !*id))
		return false;

	const char *filter_name = name;
	if (!name || !*name)
		filter_name = obs_source_get_display_name(id);

	obs_source_t *existing_filter = 
		obs_source_get_filter_by_name(source, filter_name);
	if (existing_filter) {
		blog(LOG_WARNING, "Filter %s exists.", id);
		obs_source_release(existing_filter);
		return true;
	}

	obs_source_t *filter = 
		obs_source_create(id, filter_name, nullptr, nullptr);
	if (!filter) {
		blog(LOG_WARNING, "Create filter %s failed.", id);
		return false;
	}

	obs_source_filter_add(source, filter);
	const char *source_name = obs_source_get_name(source);
	blog(LOG_INFO, "Add filter '%s' (%s) to source '%s'.",
		filter_name, id, source_name);

	obs_source_release(filter);

	return true;
}

static bool AddFilterToAudioInput(const char *id, const char *name)
{
    if (!id || !*id)
        return false;

    obs_source_t *source = obs_get_output_source(ZDTALK_AUDIO_INPUT_INDEX);
	if (!source) {
		blog(LOG_WARNING, "Get audio input source failed.");
		return false;
	}

	bool ret = AddFilterToSource(source, id, name);
    obs_source_release(source);
	return ret;
}

static inline bool HasAudioDevices(const char *source_id)
{
	obs_properties_t *props = obs_get_source_properties(source_id);
    if (!props)
        return false;

	size_t count = 0;
    obs_property_t *devices = obs_properties_get(props, "device_id");
    if (devices)
        count = obs_property_list_item_count(devices);

    obs_properties_destroy(props);

    return count != 0;
}

static void ResetAudioDevice(const char *sourceId, const char *deviceId,
                             const char *deviceDesc, int channel)
{
    bool disable = deviceId && strcmp(deviceId, "disabled") == 0;
    obs_source_t *source;
    obs_data_t *settings;

    source = obs_get_output_source(channel);
    if (source) {
        if (disable) {
            obs_set_output_source(channel, nullptr);
        } else {
            settings = obs_source_get_settings(source);
            const char *oldId = obs_data_get_string(settings,
                                                    "device_id");
            if (strcmp(oldId, deviceId) != 0) {
                obs_data_set_string(settings, "device_id",
                                    deviceId);
            }
            // if (channel == ZDTALK_AUDIO_OUTPUT_INDEX)
            //     obs_data_set_bool(settings, "use_device_timing", true);
            // else
            obs_data_set_bool(settings, "use_device_timing", false);
            obs_source_update(source, settings);
            obs_data_release(settings);

            obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
            obs_source_set_audio_mixers(source, 1);
        }

        obs_source_release(source);

    } else if (!disable) {
        settings = obs_data_create();
        obs_data_set_string(settings, "device_id", deviceId);
        // if (channel == ZDTALK_AUDIO_OUTPUT_INDEX)
        //     obs_data_set_bool(settings, "use_device_timing", true);
        // else
        obs_data_set_bool(settings, "use_device_timing", false);
        source = obs_source_create(sourceId, deviceDesc, settings, nullptr);
        obs_data_release(settings);

        obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
        obs_source_set_audio_mixers(source, 1);

        obs_set_output_source(channel, source);
        obs_source_release(source);
    }
}

static void AddSource(void *_data, obs_scene_t *scene)
{
    obs_source_t *source = (obs_source_t *)_data;
    obs_scene_add(scene, source);
    obs_source_release(source);
}

static bool FindSceneItemAndScale(obs_scene_t *scene, obs_sceneitem_t *item,
                                  void *param)
{
    UNUSED_PARAMETER(scene);

    RecorderObsContext *context = static_cast<RecorderObsContext *>(param);
    QSize orgSize = context->GetOriginalSize();

    float base_width = 
        config_get_int(App()->GetGlobalConfig(), "Video", "BaseCX");
    float base_height = 
        config_get_int(App()->GetGlobalConfig(), "Video", "BaseCY");
    float width = base_width;
    float height = base_height;
    float sratio = base_height / orgSize.height();
    bool align_width = false;
    float base_ratio = base_width / base_height;
    float orginal_ratio = orgSize.width() * 1.f / orgSize.height();
    if (base_ratio >= orginal_ratio)
        width = base_height * orginal_ratio;
    else {
        height = base_width / orginal_ratio;
        sratio = base_width / orgSize.width();
        align_width = true;
    }
    vec2 scale;
    scale.x = scale.y = sratio;
    obs_sceneitem_set_scale(item, &scale);
    vec2 pos;
    if (align_width)
        vec2_set(&pos, 0, (base_height - height) / 2);
    else
        vec2_set(&pos, (base_width - width) / 2, 0);
    obs_sceneitem_set_pos(item, &pos);

    blog(LOG_INFO, "Move/Scale item to: (%f, %f)-(%f %f).", 
        pos.x, pos.y, width, height);
    return true;
}

// --- RecorderObsContext Member Functions -------------------------------------
void RecorderObsContext::SourceActivated(void *data, calldata_t *params)
{
    obs_source_t *source = (obs_source_t*)calldata_ptr(params, "source");
    uint32_t     flags = obs_source_get_output_flags(source);

    if (flags & OBS_SOURCE_AUDIO)
        QMetaObject::invokeMethod(static_cast<RecorderObsContext *>(data),
        "ActivateAudioSource",
        Q_ARG(OBSSource, OBSSource(source)));
}

void RecorderObsContext::SourceDeactivated(void *data, calldata_t *params)
{
    obs_source_t *source = (obs_source_t*)calldata_ptr(params, "source");
    uint32_t     flags = obs_source_get_output_flags(source);

    if (flags & OBS_SOURCE_AUDIO)
        QMetaObject::invokeMethod(static_cast<RecorderObsContext *>(data),
        "DeactivateAudioSource",
        Q_ARG(OBSSource, OBSSource(source)));
}

RecorderObsContext::RecorderObsContext(QObject *parent) : 
    QObject(parent)
{
    qRegisterMetaType<OBSSource>("OBSSource");
}

RecorderObsContext::~RecorderObsContext()
{
    qDebug() << "Release context...";

    rtmp_service_ = nullptr;
    output_handler_.reset();

    ClearSceneData();

	signal_handlers_.clear();

    qDebug() << "Release context end.";
}

void RecorderObsContext::ClearVolumeControls()
{
    VolumeController *control;

    for (size_t i = 0; i < volumes_.size(); i++) {
        control = volumes_[i];
        delete control;
    }

    volumes_.clear();
}

void RecorderObsContext::ClearSceneData()
{
    ClearVolumeControls();

    obs_set_output_source(0, nullptr);
    obs_set_output_source(1, nullptr);
    obs_set_output_source(2, nullptr);
    obs_set_output_source(3, nullptr);
    obs_set_output_source(4, nullptr);
    obs_set_output_source(5, nullptr);

    auto cb = [](void *, obs_source_t *source)
    {
        obs_source_remove(source);
        return true;
    };
    obs_enum_sources(cb, nullptr);

    obs_scene_release(scene_);

    capture_source_ = nullptr;
    scene_ = nullptr;
}

bool RecorderObsContext::CreateScene()
{
	if (scene_) {
		return true;
	}

    size_t idx = 0;
    const char *id;
    obs_source_t *fade_source = nullptr;
    while (obs_enum_transition_types(idx++, &id)) {
        if (obs_is_source_configurable(id))
            continue;
        if (strcmp(id, "fade_transition") == 0) {
            fade_source = obs_source_create_private(id, "FadeTransition", NULL);
            break;
        }
    }
    if (!fade_source) {
        blog(LOG_ERROR, "fade_transition is not found.");
        return false;
    }
    obs_set_output_source(0, fade_source);

    scene_ = obs_scene_create("Scene");
    if (!scene_) {
        blog(LOG_ERROR, "Create scene failed.");
        return false;
    }
    obs_transition_set(fade_source, obs_scene_get_source(scene_));

    obs_source_release(fade_source);

    return true;
}

obs_service_t * RecorderObsContext::GetService()
{
    if (!rtmp_service_) {
        rtmp_service_ = obs_service_create("rtmp_custom", "RtmpService",
            nullptr, nullptr);
        obs_service_release(rtmp_service_);
    }
    return rtmp_service_;
}

void RecorderObsContext::SetService(obs_service_t *newService)
{
    if (newService)
        rtmp_service_ = newService;
}

bool RecorderObsContext::StartupOBS()
{
	if (obs_initialized()) {
		return true;
	}

	blog(LOG_INFO, "obs version %s.", obs_get_version_string());

	if (!obs_startup(config_get_string(App()->GetGlobalConfig(), "General", "Language"),
		nullptr, App()->GetProfilerNameStore())) {
		blog(LOG_ERROR, "obs startup failed!");
		return false;
	}

	AddExtraModulePaths();
	obs_load_all_modules();
	blog(LOG_INFO, "---------------------------------");
	obs_log_loaded_modules();
	blog(LOG_INFO, "---------------------------------");
	obs_post_load_modules();

	blog(LOG_INFO, "---------------------------------");
	DetectEncoder();
	blog(LOG_INFO, "---------------------------------");

	// InitOBSCallbacks();

	//#if defined(_WIN32)
	//    const char *monitor = config_get_string(App()->GetGlobalConfig(), "Audio",
	//        "Monitor");
	//    obs_set_audio_monitoring_device("AudioMonitor", monitor);
	//    blog(LOG_INFO, "Audio monitoring device:\tid: %s.", monitor);
	//#endif

	return true;
}

void RecorderObsContext::AddExtraModulePaths()
{
	QString base_path = config_get_string(App()->GetGlobalConfig(),
		"General", "BasePath");
	QString bin_path = base_path + "/obs-plugins";
	QString data_path = base_path + "/data/obs-plugins/%module%";
	obs_add_module_path(bin_path.toStdString().c_str(),
		data_path.toStdString().c_str());
}

bool RecorderObsContext::ResetAudio()
{
	struct obs_audio_info oai;
	oai.samples_per_sec = (uint32_t)config_get_int(App()->GetGlobalConfig(),
		"Audio", "SampleRate");
	const char *channel = config_get_string(App()->GetGlobalConfig(),
		"Audio", "ChannelSetup");
	if (strcmp(channel, "Mono") == 0)
		oai.speakers = SPEAKERS_MONO;
	else
		oai.speakers = SPEAKERS_STEREO;
	return obs_reset_audio(&oai);
}

int RecorderObsContext::ResetVideo()
{
	struct obs_video_info ovi;

	const char *colorFormat = config_get_string(App()->GetGlobalConfig(),
		"Video", "ColorFormat");
	const char *colorSpace = config_get_string(App()->GetGlobalConfig(),
		"Video", "ColorSpace");
	const char *colorRange = config_get_string(App()->GetGlobalConfig(),
		"Video", "ColorRange");
	const char *scaleType = config_get_string(App()->GetGlobalConfig(),
		"Video", "ScaleType");

	ovi.fps_num = (uint32_t)config_get_int(App()->GetGlobalConfig(),
		"Video", "FPSInt");
	ovi.fps_den = 1;
	ovi.base_width = (uint32_t)config_get_int(App()->GetGlobalConfig(),
		"Video", "BaseCX");
	ovi.base_height = (uint32_t)config_get_int(App()->GetGlobalConfig(),
		"Video", "BaseCY");
	ovi.output_width = (uint32_t)config_get_int(App()->GetGlobalConfig(),
		"Video", "OutputCX");
	ovi.output_height = (uint32_t)config_get_int(App()->GetGlobalConfig(),
		"Video", "OutputCY");
	ovi.output_format = GetVideoFormatFromName(colorFormat);
	ovi.colorspace = strcmp(colorSpace, "601") == 0 ?
	VIDEO_CS_601 : VIDEO_CS_709;
	ovi.range = strcmp(colorRange, "Full") == 0 ?
	VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;
	ovi.adapter = config_get_uint(App()->GetGlobalConfig(), "Video",
		"AdapterIdx");
	ovi.gpu_conversion = true;
	ovi.scale_type = GetScaleType(scaleType);
	ovi.graphics_module = DL_D3D11;

	int ret = obs_reset_video(&ovi);

	if (IS_WIN32 && ret != OBS_VIDEO_SUCCESS) {
		blog(LOG_WARNING, "Failed to initialize obs video (%d) "
			"with graphics_module='%s'.", ret, ovi.graphics_module);
	}

	return ret;
}

void RecorderObsContext::InitOBSCallbacks()
{
	signal_handlers_.reserve(signal_handlers_.size() + 6);
	signal_handlers_.emplace_back(obs_get_signal_handler(), "source_activate",
		RecorderObsContext::SourceActivated, this);
	signal_handlers_.emplace_back(obs_get_signal_handler(), "source_deactivate",
		RecorderObsContext::SourceDeactivated, this);
}

bool RecorderObsContext::Init()
{
    ProfileScope("RecorderObsContext::Init");

	if (!StartupOBS()) {
		emit ErrorOccurred(kErrorClientInit, tr("OBSStartup"));
		return false;
	}

	if (!ResetAudio()) {
		blog(LOG_ERROR, "Reset audio failed.");
		emit ErrorOccurred(kErrorClientInit, tr("ResetAudio"));
		return false;
	}

	int ret = ResetVideo();
	if (ret != OBS_VIDEO_SUCCESS) {
		switch (ret) {
		case OBS_VIDEO_MODULE_NOT_FOUND:
			blog(LOG_ERROR, "Failed to initialize video: graphics module not found.");
			break;
		case OBS_VIDEO_NOT_SUPPORTED:
			blog(LOG_ERROR, "Unsupported.");
			break;
		case OBS_VIDEO_INVALID_PARAM:
			blog(LOG_ERROR, "Failed to initialize video: invalid parameters.");
			break;
		default:
			blog(LOG_ERROR, "Unknown.");
			break;
		}
		blog(LOG_ERROR, "Reset video failed.");
		emit ErrorOccurred(kErrorClientInit, tr("ResetVideo"));
		return false;
	}

    if (!InitService()) {
        emit ErrorOccurred(kErrorClientInit, tr("初始化服务失败"));
        return false;
    }

    if (!ResetOutputs()) {
        emit ErrorOccurred(kErrorClientInit, tr("初始化输出失败"));
        return false;
    }

    if (!CreateScene()) {
        emit ErrorOccurred(kErrorClientInit, tr("初始化场景失败"));
        return false;
    }

    if (HasAudioDevices(App()->OutputAudioSource())) {
        ResetAudioDevice(App()->OutputAudioSource(),
            ZDTALK_AUDIO_OUTPUT_DEVICE_ID, "Default Desktop Audio",
            ZDTALK_AUDIO_OUTPUT_INDEX);
    } else {
        blog(LOG_WARNING, "Audio device %s not found.", App()->OutputAudioSource());
    }

    if (HasAudioDevices(App()->InputAudioSource())) {
        ResetAudioDevice(App()->InputAudioSource(),
            ZDTALK_AUDIO_INPUT_DEVICE_ID, "Default Mic/Aux",
            ZDTALK_AUDIO_INPUT_INDEX);
        // 设置降噪
        AddFilterToAudioInput("noise_suppress_filter", "NoiseSuppress");
    } else {
        blog(LOG_WARNING, "Audio device %s not found.", App()->InputAudioSource());
    }

    obs_set_output_source(2, nullptr);
    obs_set_output_source(4, nullptr);
    obs_set_output_source(5, nullptr);

    emit Inited();
    return true;
}

void RecorderObsContext::ActivateAudioSource(OBSSource source)
{
    VolumeController *vol = new VolumeController(source);

    volumes_.push_back(vol);
}

void RecorderObsContext::DeactivateAudioSource(OBSSource source)
{
    for (size_t i = 0; i < volumes_.size(); i++) {
        if (volumes_[i]->GetSource() == source) {
            delete volumes_[i];
            volumes_.erase(volumes_.begin() + i);
            break;
        }
    }
}

bool RecorderObsContext::UpdateWindowCapture(const QString &title)
{
    if (capture_source_)
        return true;

    capture_source_ = obs_source_create("window_capture", "WindowCapture", 
        nullptr, nullptr);
    if (!capture_source_) {
        blog(LOG_ERROR, "Create window capture source failed.");
        emit ErrorOccurred(kErrorClientWindowCapture, tr("创建窗口捕获源失败"));
        return false;
    }

    // update source
    obs_data_t *setting = obs_data_create();
    obs_data_t *cur_setting = obs_source_get_settings(capture_source_);
	obs_data_apply(setting, cur_setting);
	obs_data_release(cur_setting);

    blog(LOG_INFO, "Find window '%s'.", title.toStdString().c_str());
	obs_properties_t *properties = 
		obs_source_properties(capture_source_);
	if (!properties) {
		blog(LOG_WARNING, "Get capture source properties failed.");
		return false;
	}
	const char *window_item_name = nullptr;
	obs_property_t *property = obs_properties_first(properties);
    while (property) {
        const char *prop_name = obs_property_name(property);
		if (strcmp(prop_name, "window") != 0) {
			obs_property_next(&property);
			continue;
        }
		size_t count = obs_property_list_item_count(property);
		for (size_t i = 0; i < count; ++i) {
			const char *name = obs_property_list_item_name(property, i);
			blog(LOG_INFO, "Window %d - name : %s.", i+1, name);
			if (title.compare(QString::fromUtf8(name), 
				Qt::CaseInsensitive) == 0) {
				window_item_name = obs_property_list_item_string(property, i);
				blog(LOG_INFO, "Window %s Found.", window_item_name);
				break;
			}
		}
		if (window_item_name) {
			obs_data_set_string(setting, prop_name, window_item_name);
			obs_source_update(capture_source_, setting);
		}
		break;
    }
    obs_data_release(setting);
	obs_properties_destroy(properties);
	if (!window_item_name) {
		blog(LOG_INFO, "Find window failed.");
		emit ErrorOccurred(kErrorClientWindowCapture, tr("查找应用窗口失败"));
		return false;
	}

	AddFilterToSource(capture_source_, "crop_filter", "CropFilter");

    auto func = [] (obs_scene_t *, obs_sceneitem_t *item, void *)
    {
        obs_sceneitem_remove(item);
        return true;
    };
    obs_scene_enum_items(scene_, func, nullptr);
    obs_scene_atomic_update(scene_, AddSource, capture_source_);

    UpdateCaptureConfig(true, true);

    return true;
}

void RecorderObsContext::CropVideo(const QRect &rect)
{
    if (!capture_source_) return;

    if (rect.isEmpty()) {
        blog(LOG_WARNING, "Source crop rect(%d %d %d %d) is empty.",
             rect.left(), rect.top(), rect.right(), rect.bottom());
        return;
    }

    bool relative = false;
    obs_source_t *existing_filter =
		obs_source_get_filter_by_name(capture_source_, "CropFilter");
    if (!existing_filter)
        return;
    obs_data_t *settings = obs_source_get_settings(existing_filter);
    obs_data_set_bool(settings, "relative", relative);
    obs_data_set_int(settings, "left", rect.left());
    obs_data_set_int(settings, "top", rect.top());
    if (relative) {
        obs_data_set_int(settings, "right", rect.right());
        obs_data_set_int(settings, "bottom", rect.bottom());
        blog(LOG_INFO, "Update source crop [Left:%d Top:%d Right:%d Bottom:%d].",
            rect.left(), rect.top(), rect.right(), rect.bottom());
    }
    else {
        obs_data_set_int(settings, "cx", rect.width());
        obs_data_set_int(settings, "cy", rect.height());
        blog(LOG_INFO, "Update source crop [Left:%d Top:%d Width:%d Height:%d].",
            rect.left(), rect.top(), rect.width(), rect.height());
    }
    obs_source_update(existing_filter, settings);
    obs_data_release(settings);
    obs_source_release(existing_filter);

    ScaleVideo(QSize(rect.width(), rect.height()));
}

bool RecorderObsContext::InitService()
{
	if (rtmp_service_) {
		return true;
	}

    rtmp_service_ = obs_service_create("rtmp_custom", "rtmp custom service",
        nullptr, nullptr);
    if (!rtmp_service_) {
        blog(LOG_ERROR, "Create rtmp service failed.");
        return false;
    }
    obs_service_release(rtmp_service_);

    return true;
}

bool RecorderObsContext::ResetOutputs()
{
    if (!output_handler_ || !output_handler_->Active()) {
        output_handler_.reset();
        output_handler_.reset(new BasicOutputHandler(this));
    }

    return true;
}

/**
 * @brief 设置音频输入、输出设备
 *        输出设备序号： 1， 2
 *        输入设备序号： 3， 4， 5
 */
void RecorderObsContext::ResetAudioInput(const QString &/*deviceId*/,
    const QString &deviceDesc)
{
    obs_properties_t *input_props =
        obs_get_source_properties(App()->InputAudioSource());
    bool find = false;
    const char *currentDeviceId = nullptr;

    obs_source_t *source =
            obs_get_output_source(ZDTALK_AUDIO_INPUT_INDEX);
    if (source) {
        obs_data_t *settings = nullptr;
        settings = obs_source_get_settings(source);
        if (settings)
            currentDeviceId = obs_data_get_string(settings, "device_id");
        obs_data_release(settings);
        obs_source_release(source);
    }

    if (input_props) {
        obs_property_t *inputs = obs_properties_get(input_props, "device_id");
        size_t count = obs_property_list_item_count(inputs);
        for (size_t i = 0; i < count; i++) {
            const char *name = obs_property_list_item_name(inputs, i);
            const char *id = obs_property_list_item_string(inputs, i);
            if (QString(name).contains(deviceDesc)) {
                blog(LOG_INFO, "Reset audio input, use %s.", name);
                ResetAudioDevice(App()->InputAudioSource(), id, name,
                                 ZDTALK_AUDIO_INPUT_INDEX);
                find = true;
                break;
            }
        }
        obs_properties_destroy(input_props);
    }

    if (!find) {
        if (QString(currentDeviceId) !=
                QString(ZDTALK_AUDIO_INPUT_DEVICE_ID)) {
            blog(LOG_INFO, "Reset audio input, use \"default\".");
            ResetAudioDevice(App()->InputAudioSource(),
                             ZDTALK_AUDIO_INPUT_DEVICE_ID,
                             "Default Mic/Aux",
                             ZDTALK_AUDIO_INPUT_INDEX);
        }
    }

	AddFilterToAudioInput("noise_suppress_filter", "NoiseSuppress");
}

void RecorderObsContext::ResetAudioOutput(const QString &/*deviceId*/,
    const QString &deviceDesc)
{
    obs_properties_t *output_props =
        obs_get_source_properties(App()->OutputAudioSource());
    bool find = false;
    const char *currentDeviceId = nullptr;

    obs_source_t *source =
            obs_get_output_source(ZDTALK_AUDIO_OUTPUT_INDEX);
    if (source) {
        obs_data_t *settings = nullptr;
        settings = obs_source_get_settings(source);
        if (settings)
            currentDeviceId = obs_data_get_string(settings, "device_id");
        obs_data_release(settings);
        obs_source_release(source);
    }

    if (output_props) {
        obs_property_t *outputs = obs_properties_get(output_props, "device_id");
        size_t count = obs_property_list_item_count(outputs);
        for (size_t i = 0; i < count; i++) {
            const char *name = obs_property_list_item_name(outputs, i);
            const char *id = obs_property_list_item_string(outputs, i);
            if (QString(name).contains(deviceDesc)) {
                blog(LOG_INFO, "Reset audio output, use %s.", name);
                ResetAudioDevice(App()->OutputAudioSource(), id, name,
                                 ZDTALK_AUDIO_OUTPUT_INDEX);
                find = true;
                break;
            }
        }
        obs_properties_destroy(output_props);
    }

    if (!find) {
        if (QString(currentDeviceId) !=
                QString(ZDTALK_AUDIO_OUTPUT_DEVICE_ID)) {
            blog(LOG_INFO, "Reset audio output, use \"default\".");
            ResetAudioDevice(App()->OutputAudioSource(),
                             ZDTALK_AUDIO_OUTPUT_DEVICE_ID,
                             "Default Desktop Audio",
                             ZDTALK_AUDIO_OUTPUT_INDEX);
        }
    }
}

void RecorderObsContext::DownmixMonoInput(bool enable)
{
    obs_source_t *source =
            obs_get_output_source(ZDTALK_AUDIO_INPUT_INDEX);
    if (source) {
        uint32_t flags = obs_source_get_flags(source);
        bool forceMonoActive = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;
        blog(LOG_INFO, "Audio input force mono %d.", forceMonoActive);
        if (forceMonoActive != enable) {
            if (enable)
                flags |= OBS_SOURCE_FLAG_FORCE_MONO;
            else
                flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
            obs_source_set_flags(source, flags);
        }
        obs_source_release(source);
    }
}

void RecorderObsContext::DownmixMonoOutput(bool enable)
{
    obs_source_t *source =
            obs_get_output_source(ZDTALK_AUDIO_OUTPUT_INDEX);
    if (source) {
        uint32_t flags = obs_source_get_flags(source);
        bool forceMonoActive = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;
        blog(LOG_INFO, "Audio output force mono %d.", forceMonoActive);
        if (forceMonoActive != enable) {
            if (enable)
                flags |= OBS_SOURCE_FLAG_FORCE_MONO;
            else
                flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
            obs_source_set_flags(source, flags);
        }
        obs_source_release(source);
    }
}

void RecorderObsContext::MuteAudioInput(bool mute)
{
    obs_source_t *source =
            obs_get_output_source(ZDTALK_AUDIO_INPUT_INDEX);
    if (source) {
        bool old = obs_source_muted(source);
//        if (old != mute) {
        blog(LOG_INFO, "Mute audio input from:%d to:%d.", old, mute);
        obs_source_set_muted(source, mute);
//        }
        obs_source_release(source);
    }
}

void RecorderObsContext::MuteAudioOutput(bool mute)
{
    obs_source_t *source =
            obs_get_output_source(ZDTALK_AUDIO_OUTPUT_INDEX);
    if (source) {
        bool old = obs_source_muted(source);
//        if (old != mute) {
        blog(LOG_INFO, "Mute audio output from:%d to:%d.", old, mute);
        obs_source_set_muted(source, mute);
//        }
        obs_source_release(source);
    }
}

void RecorderObsContext::StartRecording(const QString &output)
{
    if (output_handler_->RecordingActive())
        return;

    if (output.isEmpty()) {
        blog(LOG_ERROR, "Recording parameter invalid, outputPath=%s.",
             output.toStdString().c_str());
        emit ErrorOccurred(kErrorClientRecording, tr("参数错误"));
        return;
    }

    DownmixMonoInput(true);

    config_set_default_string(App()->GetGlobalConfig(), "Output", "FilePath",
        output.toStdString().c_str());

    if (!output_handler_->StartRecording())
        emit ErrorOccurred(kErrorClientRecording, tr("启动失败"));
}

void RecorderObsContext::StopRecording(bool force)
{
    if (output_handler_ && output_handler_->RecordingActive())
        output_handler_->StopRecording(force);
}

void RecorderObsContext::StartStreaming(const QString &server, const QString &key)
{
    if (output_handler_->StreamingActive())
        return;

    if (server.isEmpty() || key.isEmpty()) {
        blog(LOG_ERROR, "Streaming parameter invalid, server=%s, key=%s.",
             server.toStdString().c_str(), key.toStdString().c_str());
        emit ErrorOccurred(kErrorClientStreaming, tr("参数错误"));
        return;
    }

    obs_data_t *oldSettings = obs_service_get_settings(GetService());
    QString oldServer = obs_data_get_string(oldSettings, "server");
    QString oldKey = obs_data_get_string(oldSettings, "key");
    obs_data_release(oldSettings);
    if (oldServer != server || oldKey != key) {
        OBSData settings = obs_data_create();
        obs_data_release(settings);

        obs_data_set_string(settings, "server", server.toStdString().c_str());
        obs_data_set_string(settings, "key", key.toStdString().c_str());
        obs_data_set_bool(settings, "use_auth", false);

        obs_service_t *newService = obs_service_create("rtmp_custom", "RtmpService",
            settings, nullptr);
        if (!newService) {
            blog(LOG_ERROR, "Create rtmp service failed.");
            emit ErrorOccurred(kErrorClientStreaming, tr("启动失败"));
            return;
        }
        SetService(newService);
        obs_service_release(newService);
    } else
        blog(LOG_INFO, "Stream server and key are not changed.");

    if (!output_handler_->StartStreaming(GetService()))
        emit ErrorOccurred(kErrorClientStreaming, tr("启动失败"));
}

void RecorderObsContext::StopStreaming(bool force)
{
    if (output_handler_ && output_handler_->StreamingActive())
        output_handler_->StopStreaming(force);
}

void RecorderObsContext::UpdateCaptureConfig(bool cursor, bool compatibility)
{
    if (!capture_source_) return;

    blog(LOG_INFO, "update source cursor=%d, compatibility=%d",
         cursor, compatibility);
    obs_data_t *settings = obs_source_get_settings(capture_source_);
    obs_data_set_bool(settings, "cursor", cursor);
    obs_data_set_bool(settings, "compatibility", compatibility);
    //obs_data_set_bool(settings, "use_wildcards", useWildcards);
    obs_source_update(capture_source_, settings);
    obs_data_release(settings);
}

void RecorderObsContext::ScaleVideo(const QSize &size)
{
    //if (org_width_ == size.width() && org_height_ == size.height())
    //    return;
    org_width_ = size.width();
    org_height_ = size.height();

    blog(LOG_INFO, "Scale scene to %dx%d.", org_width_, org_height_);

    if (scene_)
        obs_scene_enum_items(scene_, FindSceneItemAndScale, (void *)this);
    else
        blog(LOG_WARNING, "Scene is not created, cannot scale.");
}

void RecorderObsContext::LogStreamStats()
{
    output_handler_->LogStreaming();
}
