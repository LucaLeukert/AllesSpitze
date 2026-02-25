#pragma once

#include <QByteArray>
#include <QString>

class IPanelTransport {
public:
    virtual ~IPanelTransport() = default;
    virtual bool open(QString *errorMessage) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool writePacket(const QByteArray &packet, QString *errorMessage) = 0;
    virtual QByteArray readPacket(int timeoutMs, QString *errorMessage) = 0;
};
