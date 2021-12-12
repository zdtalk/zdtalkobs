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

#include "recorder-volume-controller.h"

#include "util/platform.h"

#define CLAMP(x, min, max) ((x) < min ? min : ((x) > max ? max : (x)))

void VolumeController::OBSVolumeChanged(void *data, float db)
{
    Q_UNUSED(db);
    VolumeController *controller = static_cast<VolumeController *>(data);
    QMetaObject::invokeMethod(controller, "VolumeChanged");
}

void VolumeController::OBSVolumeLevel(void *data,
    const float magnitude[MAX_AUDIO_CHANNELS],
    const float peak[MAX_AUDIO_CHANNELS],
    const float inputPeak[MAX_AUDIO_CHANNELS])
{
    VolumeController *controller = static_cast<VolumeController *>(data);
    controller->volMeter->setLevels(magnitude, peak, inputPeak);
}

void VolumeController::OBSVolumeMuted(void *data, calldata_t *calldata)
{
    VolumeController *controller = static_cast<VolumeController *>(data);
    bool muted = calldata_bool(calldata, "muted");
    QMetaObject::invokeMethod(controller, "VolumeMuted", Q_ARG(bool, muted));
}

VolumeController::VolumeController(OBSSource source) :
    source_(source),
    name_(obs_source_get_name(source)),
    obs_fader_(obs_fader_create(OBS_FADER_CUBIC)),
    obs_volmeter_(obs_volmeter_create(OBS_FADER_LOG))
{
    volMeter = new VolumeMeter(this, obs_volmeter_, name_);

    obs_fader_add_callback(obs_fader_, OBSVolumeChanged, this);
    obs_volmeter_add_callback(obs_volmeter_, OBSVolumeLevel, this);

    signal_handler_connect(obs_source_get_signal_handler(source_),
        "mute", OBSVolumeMuted, this);

    obs_fader_attach_source(obs_fader_, source_);
    obs_volmeter_attach_source(obs_volmeter_, source_);

    obs_fader_set_deflection(obs_fader_, 1.f);
    VolumeChanged();
}

VolumeController::~VolumeController()
{
    obs_fader_remove_callback(obs_fader_, OBSVolumeChanged, this);
    obs_volmeter_remove_callback(obs_volmeter_, OBSVolumeLevel, this);

    signal_handler_disconnect(obs_source_get_signal_handler(source_),
        "mute", OBSVolumeMuted, this);

    obs_fader_destroy(obs_fader_);
    obs_volmeter_destroy(obs_volmeter_);
}

void VolumeController::SetMuted(bool muted)
{
    obs_source_set_muted(source_, muted);
}

void VolumeController::VolumeChanged()
{
    int value = (int)(obs_fader_get_deflection(obs_fader_) * 100.0f);
    blog(LOG_INFO, "%s volume changed %d", GetName(), value);
}

void VolumeController::VolumeMuted(bool muted)
{
    blog(LOG_INFO, "%s muted %d", GetName(), muted);
}

// VolumeMeter -----------------------------------------------------------------
VolumeMeter::VolumeMeter(QObject *parent, obs_volmeter_t *obs_volmeter,
    const char *name)
    : QObject(parent), obs_volmeter(obs_volmeter), sourceName(name)
{
    minimumLevel = -60.0;                               // -60 dB
    warningLevel = -20.0;                               // -20 dB
    errorLevel = -9.0;                                  //  -9 dB
    clipLevel = -0.5;                                   //  -0.5 dB
    minimumInputLevel = -50.0;                          // -50 dB
    peakDecayRate = 11.76;                              //  20 dB / 1.7 sec
    magnitudeIntegrationTime = 0.3;                     //  99% in 300 ms
    peakHoldDuration = 20.0;                            //  20 seconds
    inputPeakHoldDuration = 1.0;                        //  1 second

    handleChannelCofigurationChange();
}

void VolumeMeter::setLevels(
    const float magnitude[MAX_AUDIO_CHANNELS],
    const float peak[MAX_AUDIO_CHANNELS],
    const float inputPeak[MAX_AUDIO_CHANNELS])
{
    for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
        currentMagnitude[channelNr] = magnitude[channelNr];
        currentPeak[channelNr] = peak[channelNr];
        currentInputPeak[channelNr] = inputPeak[channelNr];
    }

    uint64_t ts = os_gettime_ns();
    qreal timeSinceLastRedraw = (ts - currentLastUpdateTime) * 0.000000001;

    handleChannelCofigurationChange();
    calculateBallistics(ts, timeSinceLastRedraw);

    for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {
        int lastPeakLevel = peakLevel[channelNr];
        float peakHold = displayInputPeakHold[channelNr];
        if (peakHold < minimumInputLevel) {
            peakLevel[channelNr] = kPeakLevelMinimum;
        }
        else if (peakHold < warningLevel) {
            peakLevel[channelNr] = kPeakLevelNominal;
        }
        else if (peakHold < errorLevel) {
            peakLevel[channelNr] = kPeakLevelWarning;
        }
        else if (peakHold <= clipLevel) {
            peakLevel[channelNr] = kPeakLevelError;
        }
        else {
            peakLevel[channelNr] = kPeakLevelClip;
        }

        if (lastPeakLevel != peakLevel[channelNr]) {
            std::string levelStr(peakLevel[channelNr] * 2, '=');
            blog(LOG_INFO, "############# %s(Channel:%d) peak level %s.",
                sourceName, channelNr, levelStr.c_str());
        }
    }

    currentLastUpdateTime = ts;
}

inline void VolumeMeter::resetLevels()
{
    currentLastUpdateTime = 0;
    for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
        currentMagnitude[channelNr] = -M_INFINITE;
        currentPeak[channelNr] = -M_INFINITE;
        currentInputPeak[channelNr] = -M_INFINITE;

        displayMagnitude[channelNr] = -M_INFINITE;
        displayPeak[channelNr] = -M_INFINITE;
        displayPeakHold[channelNr] = -M_INFINITE;
        displayPeakHoldLastUpdateTime[channelNr] = 0;
        displayInputPeakHold[channelNr] = -M_INFINITE;
        displayInputPeakHoldLastUpdateTime[channelNr] = 0;

        peakLevel[channelNr] = kPeakLevelUnknown;
    }
}

inline void VolumeMeter::handleChannelCofigurationChange()
{
    int currentNrAudioChannels = obs_volmeter_get_nr_channels(obs_volmeter);
    if (displayNrAudioChannels != currentNrAudioChannels) {
        displayNrAudioChannels = currentNrAudioChannels;

        resetLevels();
    }
}

inline bool VolumeMeter::detectIdle(uint64_t ts)
{
    float timeSinceLastUpdate = (ts - currentLastUpdateTime) * 0.000000001;
    if (timeSinceLastUpdate > 0.5) {
        resetLevels();
        return true;
    }
    else {
        return false;
    }
}

inline void VolumeMeter::calculateBallisticsForChannel(int channelNr,
    uint64_t ts, qreal timeSinceLastRedraw)
{
    if (currentPeak[channelNr] >= displayPeak[channelNr] ||
        isnan(displayPeak[channelNr])) {
        // Attack of peak is immediate.
        displayPeak[channelNr] = currentPeak[channelNr];
    }
    else {
        // Decay of peak is 40 dB / 1.7 seconds for Fast Profile
        // 20 dB / 1.7 seconds for Medium Profile (Type I PPM)
        // 24 dB / 2.8 seconds for Slow Profile (Type II PPM)
        qreal decay = peakDecayRate * timeSinceLastRedraw;
        displayPeak[channelNr] = CLAMP(displayPeak[channelNr] - decay,
            currentPeak[channelNr], 0);
    }

    if (currentPeak[channelNr] >= displayPeakHold[channelNr] ||
        !isfinite(displayPeakHold[channelNr])) {
        // Attack of peak-hold is immediate, but keep track
        // when it was last updated.
        displayPeakHold[channelNr] = currentPeak[channelNr];
        displayPeakHoldLastUpdateTime[channelNr] = ts;
    }
    else {
        // The peak and hold falls back to peak
        // after 20 seconds.
        qreal timeSinceLastPeak = (uint64_t)(ts -
            displayPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
        if (timeSinceLastPeak > peakHoldDuration) {
            displayPeakHold[channelNr] = currentPeak[channelNr];
            displayPeakHoldLastUpdateTime[channelNr] = ts;
        }
    }

    if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] ||
        !isfinite(displayInputPeakHold[channelNr])) {
        // Attack of peak-hold is immediate, but keep track
        // when it was last updated.
        displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
        displayInputPeakHoldLastUpdateTime[channelNr] = ts;
    }
    else {
        // The peak and hold falls back to peak after 1 second.
        qreal timeSinceLastPeak = (uint64_t)(ts -
            displayInputPeakHoldLastUpdateTime[channelNr]) *
            0.000000001;
        if (timeSinceLastPeak > inputPeakHoldDuration) {
            displayInputPeakHold[channelNr] =
                currentInputPeak[channelNr];
            displayInputPeakHoldLastUpdateTime[channelNr] =
                ts;
        }
    }

    if (!isfinite(displayMagnitude[channelNr])) {
        // The statements in the else-leg do not work with
        // NaN and infinite displayMagnitude.
        displayMagnitude[channelNr] =
            currentMagnitude[channelNr];
    }
    else {
        // A VU meter will integrate to the new value to 99% in 300 ms.
        // The calculation here is very simplified and is more accurate
        // with higher frame-rate.
        qreal attack = (currentMagnitude[channelNr] -
            displayMagnitude[channelNr]) *
            (timeSinceLastRedraw /
            magnitudeIntegrationTime) * 0.99;
        displayMagnitude[channelNr] = CLAMP(
            displayMagnitude[channelNr] + attack,
            minimumLevel, 0);
    }
}

inline void VolumeMeter::calculateBallistics(uint64_t ts,
    qreal timeSinceLastRedraw)
{
    for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
        calculateBallisticsForChannel(channelNr, ts,
            timeSinceLastRedraw);
    }
}
