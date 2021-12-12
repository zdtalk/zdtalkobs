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

#include "recorder-platform.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <Dwmapi.h>
#include <psapi.h>
#include <util/windows/win-version.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <util/windows/WinHandle.hpp>
#include <util/windows/HRError.hpp>
#include <util/windows/ComPtr.hpp>

bool DisableAudioDucking(bool disable)
{
    ComPtr<IMMDeviceEnumerator>   devEnum;
    ComPtr<IMMDevice>             device;
    ComPtr<IAudioSessionManager2> sessionManager2;
    ComPtr<IAudioSessionControl>  sessionControl;
    ComPtr<IAudioSessionControl2> sessionControl2;

    HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator),
        nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        (void **)&devEnum);
    if (FAILED(result))
        return false;

    result = devEnum->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(result))
        return false;

    result = device->Activate(__uuidof(IAudioSessionManager2),
        CLSCTX_INPROC_SERVER, nullptr,
        (void **)&sessionManager2);
    if (FAILED(result))
        return false;

    result = sessionManager2->GetAudioSessionControl(nullptr, 0,
        &sessionControl);
    if (FAILED(result))
        return false;

    result = sessionControl->QueryInterface(&sessionControl2);
    if (FAILED(result))
        return false;

    result = sessionControl2->SetDuckingPreference(disable);
    return SUCCEEDED(result);
}

void SetAeroEnabled(bool enable)
{
    static HRESULT(WINAPI *func)(UINT) = nullptr;
    static bool failed = false;

    if (!func) {
        if (failed)
            return;

        HMODULE dwm = LoadLibraryW(L"dwmapi");
        if (!dwm) {
            failed = true;
            return;
        }

        func = reinterpret_cast<decltype(func)>(GetProcAddress(dwm,
            "DwmEnableComposition"));
        if (!func) {
            failed = true;
            return;
        }
    }

    func(enable ? DWM_EC_ENABLECOMPOSITION : DWM_EC_DISABLECOMPOSITION);
}

uint32_t GetWindowsVersion()
{
    static uint32_t ver = 0;

    if (ver == 0) {
        struct win_version_info ver_info;

        get_win_ver(&ver_info);
        ver = (ver_info.major << 8) | ver_info.minor;
    }

    return ver;
}
#endif