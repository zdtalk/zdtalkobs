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

#ifndef ZDTALK_RECORDER_AUDIO_ENCODERS_H_
#define ZDTALK_RECORDER_AUDIO_ENCODERS_H_

#include <obs.hpp>

#include <map>

const std::map<int, const char*> &GetAACEncoderBitrateMap();
const char *GetAACEncoderForBitrate(int bitrate);
int FindClosestAvailableAACBitrate(int bitrate);

#endif // ZDTALK_RECORDER_AUDIO_ENCODERS_H_
