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

#ifndef ZDTALK_RECORDER_SINGLE_BASE_H_
#define ZDTALK_RECORDER_SINGLE_BASE_H_

template <class T>
class Singleton
{
public:
    Singleton() = default;
    Singleton(const Singleton &) = delete;
    Singleton & operator=(const Singleton &) = delete;
    virtual ~Singleton() {}

    static T* GetInstance()
    {
        static T instance;
        return &instance;
    }
};

#endif // ZDTALK_RECORDER_SINGLE_BASE_H_
