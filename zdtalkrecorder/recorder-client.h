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

#ifndef ZDTALKOBS_RECORDER_CLIENT_H_
#define ZDTALKOBS_RECORDER_CLIENT_H_

#include "recorder-obs-context.h"

#include <QLocalSocket>
#include <QObject>
#include <QThread>

#define THREADWORKER

class RecorderClient : public QObject
{
    Q_OBJECT
public:
    explicit RecorderClient(QObject *parent = 0);
    ~RecorderClient();

    void ConnectToServer(const QString &server);

#ifdef TEST
    bool Init();
    void CropVideo(const QRect &);
    bool UpdateWindow(const QString &);
    void StartRecording(const QString &);
    void StopRecording();
    void StartStreaming(const QString &, const QString &);
    void StopStreaming();
#endif

signals:
    // Client -> Recording Thread
    void OBSInit();
    void OBSUpdateWindow(const QString &title);
    void OBSRelease();
    void OBSScaleVideo(const QSize &);
    void OBSCropVideo(const QRect &);
    void OBSUpdateCaptureConfig(bool cursor = true, bool compatibility = false);
    void OBSResetAudioInput(const QString &id, const QString &desc);
    void OBSResetAudioOutput(const QString &id, const QString &desc);
    void OBSDownmixMonoInput(bool enable);
    void OBSDownmixMonoOutput(bool enable);
    void OBSMuteAudioInput(bool);
    void OBSMuteAudioOutput(bool);
    void OBSStartRecording(const QString &output);
    void OBSStopRecording(bool force);
    void OBSStartStreaming(const QString &server, const QString &key);
    void OBSStopStreaming(bool force);
    void OBSLogStreamStats();

public slots:
    // Recording Callback -> Client
    void OnOBSInited();
    void OnOBSRecordingStarted();
    void OnOBSRecordingStopped(const QString &);
    void OnOBSStreamingStarted();
    void OnOBSStreamingStopped();
    void OnOBSErrorOccurred(const int, const QString &);

private slots:
    // Socket
    void OnSocketConnected();
    void OnSocketDisconnected();
    void OnSocketError(QLocalSocket::LocalSocketError socketError);
    void OnSocketReadyRead();

private:
    void SendMessageToServer(int event, int param1 = 0,
                             const QString &param2 = QStringLiteral(""));

private:
    QLocalSocket *socket_ = nullptr;
    RecorderObsContext *obs_context_ = nullptr;

#ifdef THREADWORKER
    QThread *thread_ = nullptr;
#endif
};

#endif // ZDTALKOBS_RECORDER_CLIENT_H_
