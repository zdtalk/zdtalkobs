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

#include "recorder-logger.h"

#include <QMutexLocker>
#include <QTextStream>

void RecorderLogger::OpenLogFile(const QString &path)
{
    QMutexLocker locker(&mutex_);

    if (file_.isOpen())
        file_.close();

    file_.setFileName(path);
    if (file_.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream ts(&file_);
        ts.setCodec("UTF-8");
        ts << "\n";
    }
}

void RecorderLogger::WriteLog(const QString &text)
{
    QMutexLocker locker(&mutex_);

    if (file_.isOpen()) {
        QTextStream ts(&file_);
        ts.setCodec("UTF-8");
        ts << text << "\n";
    }
}
