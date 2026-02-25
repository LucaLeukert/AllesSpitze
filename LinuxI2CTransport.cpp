#include "LinuxI2CTransport.h"

bool LinuxI2CTransport::open(QString *errorMessage) {
    Q_UNUSED(errorMessage);
    return false;
}

void LinuxI2CTransport::close() {}

bool LinuxI2CTransport::isOpen() const {
    return false;
}

bool LinuxI2CTransport::writePacket(const QByteArray &packet, QString *errorMessage) {
    Q_UNUSED(packet);
    Q_UNUSED(errorMessage);
    return false;
}

QByteArray LinuxI2CTransport::readPacket(int timeoutMs, QString *errorMessage) {
    Q_UNUSED(timeoutMs);
    Q_UNUSED(errorMessage);
    return {};
}
