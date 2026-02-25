#pragma once

#include <QObject>
#include <QHash>
#include <QUrl>
#include <QStringList>

class AudioEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList availableSfx READ availableSfx NOTIFY availableSfxChanged)

public:
    explicit AudioEngine(QObject *parent = nullptr);

    [[nodiscard]] QStringList availableSfx() const { return m_availableSfx; }

    Q_INVOKABLE bool playSfx(const QString &name);
    Q_INVOKABLE bool playLoopSfx(const QString &name);
    Q_INVOKABLE void fadeOutSfxByPrefix(const QString &prefix, int durationMs = 120);
    Q_INVOKABLE void fadeSfxVolumeByPrefix(const QString &prefix, qreal targetVolume, int durationMs = 120) const;
    Q_INVOKABLE void stopSfxByPrefix(const QString &prefix) const;

signals:
    void availableSfxChanged();

private:
    void rebuildSfxIndex();
    [[nodiscard]] QUrl resolveSfxUrl(const QString &name) const;

private:
    QHash<QString, QUrl> m_sfxByKey;
    QStringList m_availableSfx;
};
