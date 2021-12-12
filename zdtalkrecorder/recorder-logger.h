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

#ifndef ZDTALKOBS_RECORDER_LOGGER_H_
#define ZDTALKOBS_RECORDER_LOGGER_H_

#include <QFile>
#include "singlebase.h"

class RecorderLogger : public Singleton<RecorderLogger>
{
public:
    friend class Singleton<RecorderLogger>;
    ~RecorderLogger() {}

    QString LogPath() const { return file_.fileName(); }
    void OpenLogFile(const QString &);
    void WriteLog(const QString &);

private:
    RecorderLogger() {}

    QFile file_;
    QMutex mutex_;
};

#endif // ZDTALKOBS_RECORDER_LOGGER_H_
