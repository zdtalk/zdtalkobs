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


#include "recorder-client.h"
#include "recorder-app.h"
#include "recorder-define.h"
#include "recorder-obs-context.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QThread>
#include <QTimer>
#include <QDebug>

#define TAG_OUT "<<"
#define TAG_IN  ">>"
#define SEPARATOR "============================================================"

#ifdef TEST
bool RecorderClient::Init()
{
#ifdef THREADWORKER
    emit OBSInit();
    return true;
#else
    obs_context_->Init();
#endif
}

void RecorderClient::CropVideo(const QRect &rect)
{
#ifdef THREADWORKER
    emit OBSCropVideo(rect);
#else
    obs_context_->CropVideo(rect);
#endif
}

bool RecorderClient::UpdateWindow(const QString &title)
{
#ifdef THREADWORKER
    emit OBSUpdateWindow(title);
    return true;
#else
    return obs_context_->UpdateWindowCapture(title);
#endif
}

void RecorderClient::StartRecording(const QString &path)
{
#ifdef THREADWORKER
    emit OBSStartRecording(path);
#else
    obs_context_->StartRecording(path);
#endif
}

void RecorderClient::StopRecording()
{
#ifdef THREADWORKER
    emit OBSStopRecording(true);
#else
    obs_context_->StopRecording(true);
#endif
}

void RecorderClient::StartStreaming(const QString &server, const QString &key)
{
#ifdef THREADWORKER
    emit OBSStartStreaming(server, key);
#else
    obs_context_->StartStreaming(server, key);
#endif
}

void RecorderClient::StopStreaming()
{
#ifdef THREADWORKER
    emit OBSStopStreaming(true);
#else
    obs_context_->StopStreaming(true);
#endif
}
#endif

RecorderClient::RecorderClient(QObject *parent) : QObject(parent)
{
    // Local Socket
    socket_ = new QLocalSocket(this);
    connect(socket_, &QLocalSocket::connected,
            this,    &RecorderClient::OnSocketConnected);
    connect(socket_, &QLocalSocket::disconnected,
            this,    &RecorderClient::OnSocketDisconnected);
    connect(socket_, static_cast<void(QLocalSocket::*)
        (QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
            this,    &RecorderClient::OnSocketError);
    connect(socket_, &QLocalSocket::readyRead,
            this,    &RecorderClient::OnSocketReadyRead);

    // OBS
    obs_context_ = new RecorderObsContext;
#ifndef TEST
    connect(obs_context_, &RecorderObsContext::Inited,
        this, &RecorderClient::OnOBSInited);
    connect(obs_context_, &RecorderObsContext::RecordingStarted,
        this, &RecorderClient::OnOBSRecordingStarted);
    connect(obs_context_, &RecorderObsContext::RecordingStopped,
        this, &RecorderClient::OnOBSRecordingStopped);
    connect(obs_context_, &RecorderObsContext::StreamingStarted,
        this, &RecorderClient::OnOBSStreamingStarted);
    connect(obs_context_, &RecorderObsContext::StreamingStopped,
        this, &RecorderClient::OnOBSStreamingStopped);
    connect(obs_context_, &RecorderObsContext::ErrorOccurred,
        this, &RecorderClient::OnOBSErrorOccurred);
#else
    connect(obs_context_, SIGNAL(Inited()),
        parent, SLOT(OnOBSInited()));
    connect(obs_context_, SIGNAL(RecordingStarted()),
        parent, SLOT(OnRecordingStarted()), Qt::QueuedConnection);
    connect(obs_context_, SIGNAL(RecordingStopping()),
        parent, SLOT(OnRecordingStopping()));
    connect(obs_context_, SIGNAL(RecordingStopped(const QString &)),
        parent, SLOT(OnRecordingStopped(const QString &)));
    connect(obs_context_, SIGNAL(StreamingStarting(int)),
        parent, SLOT(OnStreamingStarting(int)));
    connect(obs_context_, SIGNAL(StreamingStarted()),
        parent, SLOT(OnStreamingStarted()));
    connect(obs_context_, SIGNAL(StreamingStopping(int)),
        parent, SLOT(OnStreamingStopping(int)));
    connect(obs_context_, SIGNAL(StreamingStopped()),
        parent, SLOT(OnStreamingStopped()));
    connect(obs_context_, SIGNAL(ErrorOccurred(const int, const QString &)),
        parent, SLOT(OnErrorOccurred(const int, const QString &)));
#endif

#ifdef THREADWORKER
    thread_ = new QThread(this);
    obs_context_->moveToThread(thread_);
    connect(this, &RecorderClient::OBSInit,
        obs_context_, &RecorderObsContext::Init);
    connect(this, &RecorderClient::OBSUpdateWindow,
        obs_context_, &RecorderObsContext::UpdateWindowCapture);
    //connect(this,        &RecorderClient::OBSRelease,
    //    obs_context_, &RecorderObsContext::Release);
    connect(this, &RecorderClient::OBSScaleVideo,
        obs_context_, &RecorderObsContext::ScaleVideo);
    connect(this, &RecorderClient::OBSCropVideo,
        obs_context_, &RecorderObsContext::CropVideo);
    connect(this, &RecorderClient::OBSUpdateCaptureConfig,
        obs_context_, &RecorderObsContext::UpdateCaptureConfig);
    connect(this, &RecorderClient::OBSResetAudioInput,
        obs_context_, &RecorderObsContext::ResetAudioInput);
    connect(this, &RecorderClient::OBSResetAudioOutput,
        obs_context_, &RecorderObsContext::ResetAudioOutput);
    connect(this, &RecorderClient::OBSDownmixMonoInput,
        obs_context_, &RecorderObsContext::DownmixMonoInput);
    connect(this, &RecorderClient::OBSDownmixMonoOutput,
        obs_context_, &RecorderObsContext::DownmixMonoOutput);
    connect(this, &RecorderClient::OBSMuteAudioInput,
        obs_context_, &RecorderObsContext::MuteAudioInput);
    connect(this, &RecorderClient::OBSMuteAudioOutput,
        obs_context_, &RecorderObsContext::MuteAudioOutput);
    connect(this, &RecorderClient::OBSStartRecording,
        obs_context_, &RecorderObsContext::StartRecording);
    connect(this, &RecorderClient::OBSStopRecording,
        obs_context_, &RecorderObsContext::StopRecording);
    connect(this, &RecorderClient::OBSStartStreaming,
        obs_context_, &RecorderObsContext::StartStreaming);
    connect(this, &RecorderClient::OBSStopStreaming,
        obs_context_, &RecorderObsContext::StopStreaming);
    connect(this, &RecorderClient::OBSLogStreamStats,
        obs_context_, &RecorderObsContext::LogStreamStats);

#ifdef TEST
    thread_->start();
#endif

#endif
}

RecorderClient::~RecorderClient()
{
#ifdef THREADWORKER
    if (thread_->isRunning()) {
        qInfo("Waiting for recorder thread end...");
        thread_->quit();
        thread_->wait(1000);
        qInfo("Recording thread end.");
    }
#endif

    delete obs_context_;
}

void RecorderClient::ConnectToServer(const QString &server)
{
    qInfo() << "Connect to" << server << "...";
    socket_->connectToServer(server);
}

// Socket Callback -------------------------------------------------------------
void RecorderClient::OnSocketConnected()
{
    qInfo() << "Socket Connected.";
#ifdef THREADWORKER
    if (!thread_->isRunning()) {
        qInfo() << "Start Recording Thread.";
        thread_->start();
    }
#endif
}

void RecorderClient::OnSocketDisconnected()
{
    qInfo() << "Socket disconnected.";
	QTimer::singleShot(100, App(), SLOT(quit()));
}

void RecorderClient::OnSocketError(QLocalSocket::LocalSocketError)
{
    qWarning() << "Socket Error :" << socket_->errorString();
    QTimer::singleShot(100, App(), SLOT(quit()));
}

void RecorderClient::OnSocketReadyRead()
{
    QByteArray readData = socket_->readAll();
    QDataStream in(readData);
    in.setVersion(QDataStream::Qt_4_0);

    if (readData.size() < (int)sizeof(quint16)) {
        qWarning() << TAG_IN << "Received Data Is Small. Skip.";
        return;
    }

    do {
        quint16 dataSize;
        quint8 event;
        in >> dataSize >> event;
        qDebug() << TAG_IN << "Received Event:" << event;

        switch (event) {
        case kEventInit:
#ifdef THREADWORKER
            emit OBSInit();
#else
            obs_context_->Init();
#endif
            break;
        case kEventUpdateWindow:
        {
            QString title;
            in >> title;
            qInfo() << TAG_IN << "UpdateWindowCapture:" << title;
#ifdef THREADWORKER
            emit OBSUpdateWindow(title);
#else
            obs_context_->UpdateWindowCapture(title);
#endif
        }
            break;
        case kEventScaleVideo:
        {
            QSize size;
            in >> size;
            qInfo() << TAG_IN << "Scale:" << size;
#ifdef THREADWORKER
            emit OBSScaleVideo(size);
#else
            obs_context_->ScaleVideo(size);
#endif
        }
            break;
        case kEventCropVideo:
        {
            QRect rect;
            in >> rect;
            qInfo() << TAG_IN << "Crop:" << rect;
#ifdef THREADWORKER
            emit OBSCropVideo(rect);
#else
            obs_context_->CropVideo(rect);
#endif
        }
            break;
        case kEventUpdateCaptureConfig:
        {
            bool cursor, compatibility;
            in >> cursor >> compatibility;
            qInfo() << TAG_IN << "Update Video Config:" << cursor << compatibility;
#ifdef THREADWORKER
            emit OBSUpdateCaptureConfig(cursor, compatibility);
#else
            obs_context_->UpdateCaptureConfig(cursor, compatibility);
#endif
        }
            break;
        case kEventResetAudioInput:
        {
            QString deviceId, deviceDesc;
            in >> deviceId >> deviceDesc;
            qInfo() << TAG_IN << "Reset Audio Input:" << deviceId << deviceDesc;
#ifdef THREADWORKER
            emit OBSResetAudioInput(deviceId, deviceDesc);
#else
            obs_context_->ResetAudioInput(deviceId, deviceDesc);
#endif
        }
            break;
        case kEventResetAudioOutput:
        {
            QString deviceId, deviceDesc;
            in >> deviceId >> deviceDesc;
            qInfo() << TAG_IN << "Reset Audio Output:" << deviceId << deviceDesc;
#ifdef THREADWORKER
            emit OBSResetAudioOutput(deviceId, deviceDesc);
#else
            obs_context_->ResetAudioOutput(deviceId, deviceDesc);
#endif
        }
            break;
        case kEventDownmixMonoInput:
        {
            bool enable;
            in >> enable;
            qInfo() << TAG_IN << "Downmix Input:" << enable;
#ifdef THREADWORKER
            emit OBSDownmixMonoInput(enable);
#else
            obs_context_->DownmixMonoInput(enable);
#endif
        }
            break;
        case kEventDownmixMonoOutput:
        {
            bool enable;
            in >> enable;
            qInfo() << TAG_IN << "Downmix output:" << enable;
#ifdef THREADWORKER
            emit OBSDownmixMonoOutput(enable);
#else
            obs_context_->DownmixMonoOutput(enable);
#endif
        }
            break;
        case kEventMuteAudioInput:
        {
            bool enable;
            in >> enable;
            qInfo() << TAG_IN << "Mute Input:" << enable;
#ifdef THREADWORKER
            emit OBSMuteAudioInput(enable);
#else
            obs_context_->MuteAudioInput(enable);
#endif
        }
            break;
        case kEventMuteAudioOutput:
        {
            bool enable;
            in >> enable;
            qInfo() << TAG_IN << "Mute Output:" << enable;
#ifdef THREADWORKER
            emit OBSMuteAudioOutput(enable);
#else
            obs_context_->MuteAudioOutput(enable);
#endif
        }
            break;
        case kEventStartRecording:
        {
            QString path;
            in >> path;
            qInfo() << TAG_IN << "Start Recording:" << path;
#ifdef THREADWORKER
            emit OBSStartRecording(path);
#else
            obs_context_->StartRecording(path);
#endif
        }
            break;
        case kEventStopRecording:
        {
            bool force;
            in >> force;
            qInfo() << TAG_IN << "Stop Recording:" << force;
#ifdef THREADWORKER
            emit OBSStopRecording(force);
#else
            obs_context_->StopRecording(force);
#endif
        }
            break;
        case kEventStartStreaming:
        {
            QString server, key;
            in >> server >> key;
            qInfo() << TAG_IN << "Start Streaming:" << server << key;
#ifdef THREADWORKER
            emit OBSStartStreaming(server, key);
#else
            obs_context_->StartStreaming(server, key);
#endif
        }
            break;
        case kEventStopStreaming:
        {
            bool force;
            in >> force;
            qInfo() << TAG_IN << "Stop Streaming:" << force;
#ifdef THREADWORKER
            emit OBSStopStreaming(force);
#else
            obs_context_->StopStreaming(force);
#endif
        }
            break;
        case kEventExit:
            qInfo() << TAG_IN << "Exit.";
            QTimer::singleShot(100, App(), SLOT(quit()));
            break;
        default:
            break;
        }

    } while (!in.atEnd());
}

// Send Message ----------------------------------------------------------------
void RecorderClient::SendMessageToServer(int event, int param1,
    const QString &param2)
{
    if (!socket_->isWritable()) {
        qWarning() << "Socket is not writable!";
        return;
    }

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    out << (quint16)0;
    out << (quint8)event;
    if (param1 != kErrorNone)
        out << (quint8)param1;
    if (!param2.isEmpty())
        out << param2;
    out.device()->seek(0);
    out << (quint16)(block.size() - sizeof(quint16));

    socket_->write(block);
    socket_->flush();
}

// OBS Callback ----------------------------------------------------------------
void RecorderClient::OnOBSInited()
{
    qInfo() << TAG_OUT << "Inited.";
    SendMessageToServer(kEventInited);
}

void RecorderClient::OnOBSRecordingStarted()
{
    qInfo() << TAG_OUT << "Recording Started.";
    SendMessageToServer(kEventRecordingStarted);
}

void RecorderClient::OnOBSRecordingStopped(const QString &path)
{
    qInfo() << TAG_OUT << "Recording Finished." << path;
    SendMessageToServer(kEventRecordingStopped, kErrorNone, path);
}

void RecorderClient::OnOBSStreamingStarted()
{
    qInfo() << TAG_OUT << "Streaming Started.";
    SendMessageToServer(kEventStreamingStarted);
}

void RecorderClient::OnOBSStreamingStopped()
{
    qInfo() << TAG_OUT << "Streaming Finished.";
    SendMessageToServer(kEventStreamingStopped);
}

void RecorderClient::OnOBSErrorOccurred(const int type, const QString &msg)
{
    qCritical() << TAG_OUT << "!!! Error Occurred :" << type << msg;
    SendMessageToServer(kEventErrorOccurred, type, msg);
}
