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

#ifndef ZDTALK_VOLUME_CONTROLLER_H_ 
#define ZDTALK_VOLUME_CONTROLLER_H_

#include "obs.hpp"

#include <QObject>
#include <QSharedPointer>
#include <QTimer>

class VolumeMeter : public QObject
{
    Q_OBJECT
private:
    obs_volmeter_t *obs_volmeter;

    inline void resetLevels();
    inline void handleChannelCofigurationChange();
    inline bool detectIdle(uint64_t ts);
    inline void calculateBallistics(uint64_t ts,
        qreal timeSinceLastRedraw = 0.0);
    inline void calculateBallisticsForChannel(int channelNr,
        uint64_t ts, qreal timeSinceLastRedraw);

    uint64_t currentLastUpdateTime = 0;
    float currentMagnitude[MAX_AUDIO_CHANNELS];
    float currentPeak[MAX_AUDIO_CHANNELS];
    float currentInputPeak[MAX_AUDIO_CHANNELS];

    QPixmap *tickPaintCache = NULL;
    int displayNrAudioChannels = 0;
    float displayMagnitude[MAX_AUDIO_CHANNELS];
    float displayPeak[MAX_AUDIO_CHANNELS];
    float displayPeakHold[MAX_AUDIO_CHANNELS];
    uint64_t displayPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
    float displayInputPeakHold[MAX_AUDIO_CHANNELS];
    uint64_t displayInputPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];

    qreal minimumLevel;
    qreal warningLevel;
    qreal errorLevel;
    qreal clipLevel;
    qreal minimumInputLevel;
    qreal peakDecayRate;
    qreal magnitudeIntegrationTime;
    qreal peakHoldDuration;
    qreal inputPeakHoldDuration;

    uint64_t lastRedrawTime = 0;

    int peakLevel[MAX_AUDIO_CHANNELS];

    const char *sourceName;

public:
    enum PeakLevel
    {
        kPeakLevelUnknown,
        kPeakLevelMinimum,
        kPeakLevelNominal,
        kPeakLevelWarning,
        kPeakLevelError,
        kPeakLevelClip
    };
    explicit VolumeMeter(QObject *parent = 0,
        obs_volmeter_t *obs_volmeter = 0, const char *name = NULL);

    void setLevels(
        const float magnitude[MAX_AUDIO_CHANNELS],
        const float peak[MAX_AUDIO_CHANNELS],
        const float inputPeak[MAX_AUDIO_CHANNELS]);
};

class VolumeController : public QObject
{
    Q_OBJECT
public:
    VolumeController(OBSSource);
    ~VolumeController();

    inline obs_source_t * GetSource() const { return source_; }
    inline const char * GetName() const { return name_; }
    void SetMuted(bool muted);

private slots:
    void VolumeChanged();
    void VolumeMuted(bool muted);

private:
    static void OBSVolumeChanged(void *param, float db);
    static void OBSVolumeLevel(void *data,
        const float magnitude[MAX_AUDIO_CHANNELS],
        const float peak[MAX_AUDIO_CHANNELS],
        const float inputPeak[MAX_AUDIO_CHANNELS]);
    static void OBSVolumeMuted(void *data, calldata_t *calldata);

private:
    OBSSource source_;
    const char *name_;
    obs_fader_t     *obs_fader_;
    obs_volmeter_t  *obs_volmeter_;
    VolumeMeter     *volMeter;
};

#endif // ZDTALK_VOLUME_CONTROLLER_H_