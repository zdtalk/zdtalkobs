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

#ifndef ZDTALK_RECORDER_H_
#define ZDTALK_RECORDER_H_

#include <qapplication.h>
#include "recorder-client.h"
#include "util/util.hpp"
#include "util/profiler.hpp"

class ZDTalkRecorder : public QApplication
{
    Q_OBJECT
public:
    ZDTalkRecorder(int &argc, char **argv, profiler_name_store_t *store);
    ~ZDTalkRecorder();

    config_t * GetGlobalConfig() const { return global_config_; }
    const char *InputAudioSource() const;
    const char *OutputAudioSource() const;

    int Init(const QString &server, const QString &config);

    profiler_name_store_t * GetProfilerNameStore() const
    {
        return profilerNameStore;
    }

private:
    QString GetValidBasePath();
	bool InitGlobalConfig(const QString &path);
    void InitDefaultsGlobalConfig();

private:
    ConfigFile global_config_;
    OBSContext obs_context_;
    RecorderClient *client_ = nullptr;

    profiler_name_store_t *profilerNameStore = nullptr;
};

inline ZDTalkRecorder * App() { return static_cast<ZDTalkRecorder *>(qApp); }

#endif // ZDTALK_RECORDER_H_