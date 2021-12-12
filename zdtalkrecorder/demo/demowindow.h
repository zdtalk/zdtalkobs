#pragma once

#include <QtWidgets/QMainWindow>

#include "../recorder-client.h"

namespace Ui {
    class DemoWindowClass;
}

class DemoWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DemoWindow(QWidget *parent = Q_NULLPTR);
    ~DemoWindow();

protected:
    void showEvent(QShowEvent *) override;
    void resizeEvent(QResizeEvent *) override;

public slots:
    void OnOBSInited();
    void OnRecordingStarted();
    void OnRecordingStopping();
    void OnRecordingStopped(const QString &);
    void OnStreamingStarting(int);
    void OnStreamingStarted();
    void OnStreamingStopping(int);
    void OnStreamingStopped();
    void OnErrorOccurred(const int, const QString &);

private slots:
    void Init();
    void UpdateRecordWindow();

private:
    void OnBtnAddItemClicked();
    void OnBtnRecordClicked();
    void OnBtnStreamClicked();

private:
    Ui::DemoWindowClass *ui;

    RecorderClient *client;

    bool isRecording;
    bool isStreaming;
};
