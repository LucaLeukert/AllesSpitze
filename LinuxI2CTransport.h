#pragma once

#include "IPanelTransport.h"

class LinuxI2CTransport final : public IPanelTransport {
public:
    LinuxI2CTransport() = default;
    ~LinuxI2CTransport() override = default;

    bool open(QString *errorMessage) override;
    void close() override;
    bool isOpen() const override;
    bool writePacket(const QByteArray &packet, QString *errorMessage) override;
    QByteArray readPacket(int timeoutMs, QString *errorMessage) override;
};
