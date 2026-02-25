#include "AudioEngine.h"

#include <QAudioOutput>
#include <QAudioDevice>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QPropertyAnimation>

AudioEngine::AudioEngine(QObject *parent) : QObject(parent) {
    rebuildSfxIndex();
}

bool AudioEngine::playSfx(const QString &name) {
    if (QMediaDevices::audioOutputs().isEmpty()) {
        qWarning() << "AudioEngine: no audio output device available, skipping SFX" << name;
        return false;
    }

    const QUrl source = resolveSfxUrl(name);
    if (!source.isValid()) {
        qWarning() << "AudioEngine: unknown SFX" << name;
        return false;
    }

    auto *player = new QMediaPlayer(this);
    auto *output = new QAudioOutput(player);
    output->setVolume(1.0);
    player->setAudioOutput(output);
    player->setSource(source);
    player->setProperty("sfxKey", QFileInfo(source.path()).completeBaseName().toLower());

    connect(player, &QMediaPlayer::playbackStateChanged, this, [player](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState) {
            player->deleteLater();
        }
    });
    connect(player, &QMediaPlayer::mediaStatusChanged, this, [player](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::InvalidMedia) {
            player->deleteLater();
        }
    });
    connect(player, &QMediaPlayer::errorOccurred, this, [player](QMediaPlayer::Error error, const QString &errorString) {
        qWarning() << "AudioEngine: playback error" << error << errorString;
        player->deleteLater();
    });

    player->play();
    return true;
}

bool AudioEngine::playLoopSfx(const QString &name) {
    if (QMediaDevices::audioOutputs().isEmpty()) {
        qWarning() << "AudioEngine: no audio output device available, skipping loop SFX" << name;
        return false;
    }

    const QUrl source = resolveSfxUrl(name);
    if (!source.isValid()) {
        qWarning() << "AudioEngine: unknown loop SFX" << name;
        return false;
    }

    const QString key = QFileInfo(source.path()).completeBaseName().toLower();
    stopSfxByPrefix(key);

    auto *player = new QMediaPlayer(this);
    auto *output = new QAudioOutput(player);
    output->setVolume(1.0);
    player->setAudioOutput(output);
    player->setSource(source);
    player->setLoops(QMediaPlayer::Infinite);
    player->setProperty("sfxKey", key);

    connect(player, &QMediaPlayer::playbackStateChanged, this, [player](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState) {
            player->deleteLater();
        }
    });
    connect(player, &QMediaPlayer::mediaStatusChanged, this, [player](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::InvalidMedia) {
            player->deleteLater();
        }
    });
    connect(player, &QMediaPlayer::errorOccurred, this, [player](QMediaPlayer::Error error, const QString &errorString) {
        qWarning() << "AudioEngine: loop playback error" << error << errorString;
        player->deleteLater();
    });

    player->play();
    return true;
}

void AudioEngine::fadeOutSfxByPrefix(const QString &prefix, const int durationMs) {
    const QString normalizedPrefix = prefix.trimmed().toLower();
    if (normalizedPrefix.isEmpty()) {
        return;
    }

    const auto players = findChildren<QMediaPlayer *>();
    for (QMediaPlayer *player : players) {
        if (!player) {
            continue;
        }

        const QString key = player->property("sfxKey").toString().toLower();
        if (!key.startsWith(normalizedPrefix)) {
            continue;
        }

        QAudioOutput *output = player->audioOutput();
        if (!output || durationMs <= 0) {
            player->stop();
            player->deleteLater();
            continue;
        }

        auto *fade = new QPropertyAnimation(output, "volume", output);
        fade->setDuration(durationMs);
        fade->setStartValue(output->volume());
        fade->setEndValue(0.0);
        connect(fade, &QPropertyAnimation::finished, this, [player]() {
            player->stop();
            player->deleteLater();
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void AudioEngine::fadeSfxVolumeByPrefix(const QString &prefix, const qreal targetVolume, const int durationMs) const {
    const QString normalizedPrefix = prefix.trimmed().toLower();
    if (normalizedPrefix.isEmpty()) {
        return;
    }

    const qreal clampedTarget = qBound(0.0, targetVolume, 1.0);
    const auto players = findChildren<QMediaPlayer *>();
    for (QMediaPlayer *player : players) {
        if (!player) {
            continue;
        }

        const QString key = player->property("sfxKey").toString().toLower();
        if (!key.startsWith(normalizedPrefix)) {
            continue;
        }

        QAudioOutput *output = player->audioOutput();
        if (!output) {
            continue;
        }

        if (durationMs <= 0) {
            output->setVolume(clampedTarget);
            continue;
        }

        auto *fade = new QPropertyAnimation(output, "volume", output);
        fade->setDuration(durationMs);
        fade->setStartValue(output->volume());
        fade->setEndValue(clampedTarget);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void AudioEngine::stopSfxByPrefix(const QString &prefix) const {
    const QString normalizedPrefix = prefix.trimmed().toLower();
    if (normalizedPrefix.isEmpty()) {
        return;
    }

    const auto players = findChildren<QMediaPlayer *>();
    for (QMediaPlayer *player : players) {
        if (!player) {
            continue;
        }

        const QString key = player->property("sfxKey").toString().toLower();
        if (key.startsWith(normalizedPrefix)) {
            player->stop();
            player->deleteLater();
        }
    }
}

void AudioEngine::rebuildSfxIndex() {
    m_sfxByKey.clear();
    m_availableSfx.clear();

    const QDir dir(":/sounds");
    const QStringList files = dir.entryList(QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        const QUrl url(QStringLiteral("qrc:/sounds/") + fileName);
        const QFileInfo info(fileName);
        const QString baseName = info.completeBaseName();

        m_sfxByKey.insert(fileName.toLower(), url);
        m_sfxByKey.insert(baseName.toLower(), url);
        m_availableSfx.append(fileName);
    }

    emit availableSfxChanged();
}

QUrl AudioEngine::resolveSfxUrl(const QString &name) const {
    const QString key = name.trimmed().toLower();
    if (key.isEmpty()) {
        return {};
    }

    auto it = m_sfxByKey.constFind(key);
    if (it != m_sfxByKey.constEnd()) {
        return it.value();
    }

    if (!key.contains('.')) {
        it = m_sfxByKey.constFind(key + ".mp3");
        if (it != m_sfxByKey.constEnd()) {
            return it.value();
        }
    }

    return {};
}
