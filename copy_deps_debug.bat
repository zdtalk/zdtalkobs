@echo off

set PROJECT_ROOT=%~dp0
set PROJECT_CONFIG=Debug
set PROJECT_RELEASE_PATH=%PROJECT_ROOT%\build\%PROJECT_CONFIG%

copy %PROJECT_ROOT%\deps\prebuild\win32\bin\avcodec-57.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\avdevice-57.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\avfilter-6.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\avformat-57.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\avutil-55.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\swresample-2.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\swscale-4.dll %PROJECT_RELEASE_PATH%

copy %PROJECT_ROOT%\deps\prebuild\win32\bin\zlib.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\libcurl.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\libx264-152.dll %PROJECT_RELEASE_PATH%

copy %PROJECT_ROOT%\deps\prebuild\win32\bin\libopus-0.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\libvorbis-0.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\libvorbisenc-2.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\libogg-0.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\deps\prebuild\win32\bin\libvpx-1.dll %PROJECT_RELEASE_PATH%

copy %PROJECT_ROOT%\third_party\CrashRpt\bin\CrashRpt1402d.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\third_party\CrashRpt\bin\CrashSender1402d.exe %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\third_party\CrashRpt\bin\dbghelp.dll %PROJECT_RELEASE_PATH%
copy %PROJECT_ROOT%\third_party\CrashRpt\bin\crashrpt_lang.ini %PROJECT_RELEASE_PATH%

copy %PROJECT_ROOT%\third_party\directx_setup\DSETUP.dll %PROJECT_RELEASE_PATH%

mkdir %PROJECT_RELEASE_PATH%\data
mkdir %PROJECT_RELEASE_PATH%\data\libobs
mkdir %PROJECT_RELEASE_PATH%\data\obs-plugins
copy %PROJECT_ROOT%\libobs\data\*.*  %PROJECT_RELEASE_PATH%\data\libobs\*.*
copy %PROJECT_ROOT%\streamEncoder.json  %PROJECT_RELEASE_PATH%\data

for /f %%i in ('dir /b /ad "%PROJECT_ROOT%\plugins"') do (
	mkdir %PROJECT_RELEASE_PATH%\data\obs-plugins\%%i
    echo d | xcopy /S /E /Y %PROJECT_ROOT%\plugins\%%i\data %PROJECT_RELEASE_PATH%\data\obs-plugins\%%i
)
copy %PROJECT_ROOT%\plugins\obs-ffmpeg\ffmpeg-mux\%PROJECT_CONFIG%\ffmpeg-mux32.exe %PROJECT_RELEASE_PATH%\data\obs-plugins\obs-ffmpeg
copy %PROJECT_ROOT%\plugins\win-capture\get-graphics-offsets\%PROJECT_CONFIG%\get-graphics-offsets32.exe %PROJECT_RELEASE_PATH%\data\obs-plugins\win-capture
copy %PROJECT_ROOT%\plugins\win-capture\graphics-hook\%PROJECT_CONFIG%\graphics-hook32.dll %PROJECT_RELEASE_PATH%\data\obs-plugins\win-capture
copy %PROJECT_ROOT%\plugins\win-capture\inject-helper\%PROJECT_CONFIG%\inject-helper32.exe %PROJECT_RELEASE_PATH%\data\obs-plugins\win-capture

copy %PROJECT_ROOT%\config_release.ini %PROJECT_RELEASE_PATH%\data\config.ini

pause