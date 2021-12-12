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

#ifndef ZDTALK_RECORDER_OBS_CONTEXT_H_
#define ZDTALK_RECORDER_OBS_CONTEXT_H_

#include <string>
#include <memory>

#include <QObject>
#include <QRect>
#include <QSize>

#include "obs.h"
#include "obs.hpp"

#include "recorder-outputs.hpp"
#include "recorder-volume-controller.h"

class RecorderObsContext : public QObject
{
    Q_OBJECT
public:
    explicit RecorderObsContext(QObject *parent = nullptr);
	~RecorderObsContext();

    const QSize GetOriginalSize() const { 
        return QSize(org_width_, org_height_); 
    }

    obs_service_t * GetService();
    void SetService(obs_service_t *service);

signals:
    void Inited();
    void RecordingStarted();
    void RecordingStopping();
    void RecordingStopped(const QString &);
    void StreamingStarting(int);
    void StreamingStarted();
    void StreamingStopping(int);
    void StreamingStopped();
    void ErrorOccurred(const int, const QString &);

public:
    static void SourceActivated(void *data, calldata_t *params);
    static void SourceDeactivated(void *data, calldata_t *params);

public slots:
    bool Init();
    bool InitService();
    bool ResetOutputs();

    bool UpdateWindowCapture(const QString &windowTitle);

    void ScaleVideo(const QSize &);
    void UpdateCaptureConfig(bool cursor = true, bool compatibility = false);
    void CropVideo(const QRect &);

    /* 试麦切换设备后，需要重新设置音频设备 */
    void ResetAudioInput(const QString &deviceId, const QString &deviceDesc);
    void ResetAudioOutput(const QString &deviceId, const QString &deviceDesc);

    /* 处理单声道问题 */
    void DownmixMonoInput(bool enable);
    void DownmixMonoOutput(bool enable);

    /* 静音状态与 RTC 保持一致 */
    void MuteAudioInput(bool);
    void MuteAudioOutput(bool);

    void StartRecording(const QString &output);
    void StopRecording(bool force);

    void StartStreaming(const QString &server, const QString &key);
    void StopStreaming(bool force);

    void LogStreamStats();

private slots:
    void ActivateAudioSource(OBSSource source);
    void DeactivateAudioSource(OBSSource source);

private:
    bool CreateScene();
    void ClearSceneData();
    void ClearVolumeControls();

	bool StartupOBS();
	void AddExtraModulePaths();
	bool ResetAudio();
	int ResetVideo();
	void InitOBSCallbacks();

private:
    OBSService rtmp_service_;
    std::unique_ptr<BasicOutputHandler> output_handler_;

    OBSScene scene_;
    OBSSource capture_source_;

    int org_width_ = 0;
    int org_height_ = 0;

    std::vector<VolumeController *> volumes_;
	std::vector<OBSSignal> signal_handlers_;
};

#endif // ZDTALK_RECORDER_OBS_CONTEXT_H_