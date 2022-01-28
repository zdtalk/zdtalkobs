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

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTextcodec>
#include <QTimeZone>
#include <QThread>

#include <Windows.h>
#include <iostream>
#include "util/platform.h"

#include "CrashRpt.h"
#include "strconv.h"

#include "dsetup.h"

#include "recorder-app.h"
#include "recorder-logger.h"
#include "recorder-version.h"

#ifdef TEST
#include "demo/demowindow.h"
#endif

#define LOG_CRASHED "!!! Crashed !!!"

static const char *kAppName = "ZDTalkRecorder";
static const wchar_t *kInstanceMutexName = L"ZDTalkRecorderMutex";

QString g_log_file_path;
QString g_crash_dir_path;
QString g_crash_file_path;

// [CrashRpt Handler]
static int CALLBACK CrashCallback(CR_CRASH_CALLBACK_INFOA *pInfo)
{
    UNREFERENCED_PARAMETER(pInfo);

    return CR_CB_DODEFAULT;
}

static BOOL WINAPI PreCrashCallback(LPVOID lpvState)
{
    UNREFERENCED_PARAMETER(lpvState);

    static bool inside_handler = false;
    if (inside_handler)
        return FALSE;
    inside_handler = true;

    qCritical(LOG_CRASHED);
    return TRUE;
}

static bool SetupCrashRpt(CrAutoInstallHelper **helper, const QString &user_name,
    const QString &openid)
{
    strconv_t strconv;

    MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(MiniDumpWithFullMemory);

    CR_INSTALL_INFOA info = { 0 };
    info.cb = sizeof(CR_INSTALL_INFOA);
    info.pszAppName = kAppName;
    info.pszAppVersion = ZDTALK_RECORDER_VERSION_STR;
    info.uMiniDumpType = mdt;
    info.pfnCrashCallback = PreCrashCallback;
    info.dwFlags |= CR_INST_ALL_POSSIBLE_HANDLERS;
    info.dwFlags |= CR_INST_NO_GUI;
    if (mdt != MiniDumpNormal)
        info.dwFlags |= CR_INST_NO_MINIDUMP;
    // info.dwFlags |= CR_INST_STORE_ZIP_ARCHIVES;
    // info.dwFlags |= CR_INST_DONT_SEND_REPORT;
    info.pszDebugHelpDLL = NULL;
    info.pszEmailTo = "xxx@izaodao.com;";
    info.pszEmailSubject = "Crash Report";
    info.pszErrorReportSaveDir =
        strconv.utf82a(g_crash_dir_path.toUtf8().constData());

    *helper = new CrAutoInstallHelper(&info);
    if ((*helper)->m_nInstallStatus != 0) {
        char lastErr[1024] = { 0 };
        crGetLastErrorMsgA(lastErr, 1024);
        qWarning() << "CrashRpt install failed, "
            << (*helper)->m_nInstallStatus << lastErr;
        return false;
    }

    crSetCrashCallbackA(CrashCallback, nullptr);
    crAddFile2A(strconv.utf82a(g_log_file_path.toUtf8().constData()),
        NULL, "Log File", CR_AF_MAKE_FILE_COPY);
    if (!user_name.isEmpty())
        crAddPropertyA("UserName", 
        strconv.utf82a(user_name.toStdString().c_str()));
    if (!openid.isEmpty())
        crAddPropertyA("OpenID", strconv.utf82a(openid.toUtf8().constData()));

    return true;
}
// [CrashRpt Handler]

// [Qt Log Handler]
static void QtLogHandler(QtMsgType type, const QMessageLogContext &,
    const QString &msg)
{
    QString tag = "UNKNOWN";
    switch (type)
    {
    case QtInfoMsg:
        tag = QString("INFO");    break;
    case QtDebugMsg:
        tag = QString("DEBUG");   break;
    case QtWarningMsg:
        tag = QString("WARNING"); break;
    case QtCriticalMsg:
        tag = QString("ERROR");   break;
    case QtFatalMsg:
        tag = QString("FATAL");   break;
    default:
        break;
    }

    QString text = QStringLiteral("[%1][%2:%3] %4")
        .arg(QDateTime::currentDateTime().toTimeZone(QTimeZone("UTC+08:00")).
        toString("hh:mm:ss.zzz"))
        .arg(quintptr(QThread::currentThreadId()))
        .arg(tag).arg(msg);
    RecorderLogger::GetInstance()->WriteLog(text);
}
// [Qt Log Handler]

// [OBS Log Handler]
static void OBSLogHandler(int level, const char *format, va_list args, void *param)
{
    UNUSED_PARAMETER(level);
    UNUSED_PARAMETER(param);

    char str[4096];

#ifndef _WIN32
    va_list args2;
    va_copy(args2, args);
#endif

    vsnprintf_s(str, sizeof(str), format, args);

    switch (level) {
    case LOG_DEBUG:   qDebug() << "--" << str;    break;
    case LOG_WARNING: qWarning() << "--" << str;  break;
    case LOG_ERROR:   qCritical() << "--" << str; break;
    case LOG_INFO:    qInfo() << "--" << str;     break;
    }
}
// [OBS Log Handler]

static void PrintLicense()
{
    std::cout << 
        "Copyright (C) 2020 by Zaodao(Dalian) Education Technology Co., Ltd..\n"
        "This program is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 2 of the License, or\n"
        "(at your option) any later version.\n\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program.  If not, see <http://www.gnu.org/licenses/>."
        "\n\n";
}

static void PrintHelpDoc()
{
    PrintLicense();

    std::cout << "--help, -h: Get list of available commands.\n"
        << "--version, -v: Get current version.\n\n"
        << "--log, -l: Log Output File Path.\n"
        << "--crash, -c: Crash Output File Path.\n"
        << "--config, -f: Config File Path.\n"
        << "--user, -u: ZDTalk User Name.\n"
        << "--id, -i: ZDTalk User OpenId.\n"
        << "--server, -s: ZDTalk Recorder Server Name.\n";
    exit(0);
}

static void PrintVersion()
{
    PrintLicense();

    std::cout << kAppName << " " << ZDTALK_RECORDER_VERSION_STR << "\n";

    exit(0);
}

static inline bool arg_is(const char *arg, const char *long_form,
    const char *short_form)
{
    return (long_form  && strcmp(arg, long_form) == 0) ||
        (short_form && strcmp(arg, short_form) == 0);
}

static bool UpdateFilePath(const char *arg, QString &filePath)
{
    if (!arg || arg[0] == '\0')
        return false;

    QString path = QString::fromLocal8Bit(arg);
    if (!QFileInfo(path).absoluteDir().exists())
        return false;

    filePath = path;
    return true;
}

static bool UpdateInfo(const char *arg, QString &info)
{
    if (!arg || arg[0] == '\0')
        return false;

    info = QString::fromLocal8Bit(arg);
    return true;
}

static void OnDXProcessError(QProcess::ProcessError error)
{
	qWarning() << "InstallDX failed." << error;
}

static void OnDXProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qInfo() << "InstallDX finished." << exitCode << exitStatus;
}

static bool InstallDX()
{
	QString command = QCoreApplication::applicationDirPath() + "/DxWebUpdate.exe";
    command = QString("\"%1\" /Q").arg(command);
	QProcess *process = new QProcess();
	QObject::connect(process,
		static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error),
		&OnDXProcessError);
	QObject::connect(process,
		static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
		&OnDXProcessFinished);
    process->execute(command);
	return true;
}

static bool DetectDX()
{
	DWORD dwVersion = 0;
	DWORD dwRevision = 0;
    if (!DirectXSetupGetVersion(&dwVersion, &dwRevision)) {
        qWarning() << "Get DirectX version failed.";
        return false;
    }

    const wchar_t *dlls[] = { 
        L"D3DCompiler_34.dll", L"D3DCompiler_35.dll",
        L"D3DCompiler_36.dll", L"D3DCompiler_37.dll",
        L"D3DCompiler_38.dll", L"D3DCompiler_39.dll",
        L"D3DCompiler_40.dll", L"D3DCompiler_41.dll", 
        L"D3DCompiler_42.dll", L"D3DCompiler_43.dll", 
        L"D3DCompiler_47.dll", L"D3DCompiler_49.dll", 
    };

    for (int i = 0; i < sizeof(dlls) / sizeof(dlls[0]); ++i) {
        HMODULE dxDll = LoadLibrary(dlls[i]);
        if (dxDll) {
            qInfo() << "Load" << QString::fromStdWString(dlls[i]) 
                << "successful.";
            FreeLibrary(dxDll);
            return true;
        }
    }

    qWarning() << "Load DirectX Dll failed.";
	return false;
}

static auto ProfilerNameStoreRelease = [] (profiler_name_store_t *store)
{
    profiler_name_store_free(store);
};

using ProfilerNameStore = std::unique_ptr<profiler_name_store_t, 
    decltype(ProfilerNameStoreRelease)>;

ProfilerNameStore CreateNameStore()
{
    return ProfilerNameStore{ profiler_name_store_create(),
        ProfilerNameStoreRelease };
}

static auto SnapshotRelease = [] (profiler_snapshot_t *snap)
{
    profile_snapshot_free(snap);
};

using ProfilerSnapshot =
std::unique_ptr<profiler_snapshot_t, decltype(SnapshotRelease)>;

static ProfilerSnapshot GetSnapshot()
{
    return ProfilerSnapshot{ profile_snapshot_create(), SnapshotRelease };
}

static auto ProfilerFree = [] (void *)
{
    profiler_stop();

    auto snap = GetSnapshot();

    profiler_print(snap.get());
    profiler_print_time_between_calls(snap.get());

    profiler_free();
};

static int RunRecorder(const QString server, const QString config,
    int argc, char *argv[])
{
    auto profilerNameStore = CreateNameStore();

    std::unique_ptr<void, decltype(ProfilerFree)>
        prof_release(static_cast<void*>(&ProfilerFree),
        ProfilerFree);

    profiler_start();
    profile_register_root("Recorder", 0);

    ScopeProfiler prof{ "Recorder::Init" };

    QCoreApplication::addLibraryPath(".");

    ZDTalkRecorder app(argc, argv, profilerNameStore.get());

    os_chdir(QCoreApplication::applicationDirPath().toUtf8().constData());

    if (app.Init(server, config) != 0) {
        return -1;
    }

#ifdef TEST
    DemoWindow window;
    window.show();
#endif

    prof.Stop();

    return app.exec();
}

int main(int argc, char *argv[])
{
//#ifdef WIN32
//    HANDLE instance_mutex = CreateMutexW(NULL, TRUE, kInstanceMutexName);
//    if (GetLastError() == ERROR_ALREADY_EXISTS) {
//        CloseHandle(instance_mutex);
//        return 0;
//    }
//#endif

    QString user_name, openid, server_name, config_path;
    for (int i = 1; i < argc; ++i) {
        if (arg_is(argv[i], "--log", "-l"))
            UpdateFilePath(argv[++i], g_log_file_path);
        else if (arg_is(argv[i], "--crash", "-c"))
            UpdateFilePath(argv[++i], g_crash_file_path);
		else if (arg_is(argv[i], "--config", "-f"))
			UpdateFilePath(argv[++i], config_path);
        else if (arg_is(argv[i], "--user", "-u"))
            UpdateInfo(argv[++i], user_name);
        else if (arg_is(argv[i], "--id", "-i"))
            UpdateInfo(argv[++i], openid);
        else if (arg_is(argv[i], "--server", "-s"))
            UpdateInfo(argv[++i], server_name);
        else if (arg_is(argv[i], "--version", "-v"))
            PrintVersion();
        else if (arg_is(argv[i], "--help", "-h"))
            PrintHelpDoc();
    }
    if (g_log_file_path.isEmpty() || g_crash_file_path.isEmpty())
        PrintHelpDoc();

#ifdef QT_NO_DEBUG
    RecorderLogger::GetInstance()->OpenLogFile(g_log_file_path);
    qInstallMessageHandler(QtLogHandler);
#endif

    g_crash_dir_path = QFileInfo(g_crash_file_path).absoluteDir().absolutePath();
    CrAutoInstallHelper *cr_install_helper = nullptr;
    SetupCrashRpt(&cr_install_helper, user_name, openid);

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    base_set_log_handler(OBSLogHandler, nullptr);
    qInfo() << "======================= Recorder Start =======================";
    qInfo() << ZDTALK_RECORDER_VERSION_STR;
    
    DetectDX();
	//if (!DetectDX())
	//	InstallDX();

    int ret = RunRecorder(server_name, config_path, argc, argv);

    blog(LOG_INFO, "Memory leaks: %ld.", bnum_allocs());
    qInfo() << "======================= Recorder End =======================\n";
    base_set_log_handler(nullptr, nullptr);

    delete cr_install_helper;

    return ret;
}
