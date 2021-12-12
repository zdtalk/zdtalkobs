#include "demowindow.h"
#include "ui_demowindowform.h"
#include "../recorder-app.h"
#include "../recorder-define.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <string>

DemoWindow::DemoWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::DemoWindowClass),
    isRecording(false),
    isStreaming(false)
{
    ui->setupUi(this);

    ui->pushButtonRecord->setEnabled(false);
    ui->pushButtonStream->setEnabled(false);

    QAction *open_action = ui->menu->addAction("Show &Recordings");
    connect(open_action, &QAction::triggered, [] {
        QString dirPath =
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    });

    connect(ui->pushButtonAddItem, &QPushButton::clicked,
        this, &DemoWindow::OnBtnAddItemClicked);
    connect(ui->pushButtonRecord, &QPushButton::clicked,
        this, &DemoWindow::OnBtnRecordClicked);
    connect(ui->pushButtonStream, &QPushButton::clicked,
        this, &DemoWindow::OnBtnStreamClicked);

    client = new RecorderClient(this);
}

DemoWindow::~DemoWindow()
{
	delete client;
    delete ui;
}

void DemoWindow::Init()
{
#ifdef TEST
    if (!client->Init()) {
        QMessageBox::warning(this, "Error", "Obs init failed!");
        QTimer::singleShot(1, App(), SLOT(quit()));
        return;
    }

    QFileInfo fi(QCoreApplication::applicationFilePath());
    QString desc = QString("[%1]: %2").arg(fi.fileName()).arg(windowTitle());
    if (!client->UpdateWindow(desc)) {
        QMessageBox::warning(this, "Error", "Find Window failed!");
        QTimer::singleShot(1, App(), SLOT(quit()));
    }

    UpdateRecordWindow();
#endif

    ui->pushButtonRecord->setEnabled(true);
    ui->pushButtonStream->setEnabled(true);
}

void DemoWindow::UpdateRecordWindow()
{
#ifdef TEST
    QDesktopWidget *desktop = App()->desktop();
    int screen_number = desktop->screenNumber(this);
    QScreen *screen = App()->screens().at(screen_number);
    qreal ratio = screen->devicePixelRatio();
    QRect rect(0, 0, width() * ratio, height() * ratio);
    client->CropVideo(rect);
#endif
}

void DemoWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    static bool first = true;
    if (first)
        QTimer::singleShot(1, this, SLOT(Init()));
    first = false;
}

void DemoWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    UpdateRecordWindow();
}

void DemoWindow::OnBtnAddItemClicked()
{
    int num = ui->listWidget->count() + 1;
    std::string str(num, 'A');
    ui->listWidget->addItem(QString::fromStdString(str));
}

void DemoWindow::OnBtnRecordClicked()
{
#ifdef TEST
    ui->pushButtonRecord->setEnabled(false);
    if (!isRecording) {
        QString dirPath = 
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        QString fileName = 
            QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss");
        fileName += ".mp4";
        QString filePath = dirPath + "\\" + fileName;
        client->StartRecording(filePath);
    } else
        client->StopRecording();
#endif
}

void DemoWindow::OnBtnStreamClicked()
{
#ifdef TEST
    ui->pushButtonStream->setEnabled(false);
    if (!isStreaming)
        client->StartStreaming("rtmp://192.168.1.60/live", "test");
    else
        client->StopStreaming();
#endif
}

void DemoWindow::OnOBSInited()
{
    ui->pushButtonRecord->setEnabled(true);
    ui->pushButtonStream->setEnabled(true);
}

void DemoWindow::OnRecordingStarted()
{
    isRecording = true;
    ui->pushButtonRecord->setText("Stop Record");
    ui->pushButtonRecord->setEnabled(true);
}

void DemoWindow::OnRecordingStopping()
{
    ui->pushButtonRecord->setText("Stopping");
}

void DemoWindow::OnRecordingStopped(const QString &)
{
    isRecording = false;
    ui->pushButtonRecord->setText("Start Record");
    ui->pushButtonRecord->setEnabled(true);
}

void DemoWindow::OnStreamingStarting(int)
{
    ui->pushButtonStream->setText("Starting");
}

void DemoWindow::OnStreamingStarted()
{
    isStreaming = true;
    ui->pushButtonStream->setText("Stop Stream");
    ui->pushButtonStream->setEnabled(true);
}

void DemoWindow::OnStreamingStopping(int)
{
    ui->pushButtonStream->setText("Stopping");
}

void DemoWindow::OnStreamingStopped()
{
    isStreaming = false;
    ui->pushButtonStream->setText("Start Stream");
    ui->pushButtonStream->setEnabled(true);
}

void DemoWindow::OnErrorOccurred(const int code, const QString &msg)
{
    if (code == kErrorClientRecording) {
        ui->pushButtonRecord->setText("Start Record");
        ui->pushButtonRecord->setEnabled(true);
    } else if (code == kErrorClientStreaming) {
        ui->pushButtonStream->setText("Start Stream");
        ui->pushButtonStream->setEnabled(true);
    } else if (code == kErrorClientInit) {
        ui->pushButtonRecord->setEnabled(false);
        ui->pushButtonStream->setEnabled(false);
    } else {
        ui->pushButtonRecord->setText("Start Record");
        ui->pushButtonStream->setText("Start Stream");
        ui->pushButtonRecord->setEnabled(true);
        ui->pushButtonStream->setEnabled(true);
    }

    QMessageBox::warning(this, "Error", msg, QMessageBox::Ok);
}