#include "Tower.h"
#include "DebugLogger.h"

Tower::Tower(Symbol::Type symbolType, int towerId, QObject *parent)
    : QObject(parent), m_symbol_type(symbolType), m_tower_id(towerId) {}

bool Tower::increase() {
    if (m_level >= MAX_LEVEL) {
        return false;
    }

    m_level++;
    emit levelChanged();

    DebugLogger::instance().info(
        QString("Tower %1 (%2) increased to level %3")
            .arg(m_tower_id)
            .arg(Symbol::typeToString(m_symbol_type))
            .arg(m_level)
    );

    if (isFull()) {
        DebugLogger::instance().info(
            QString("Tower %1 (%2) is now FULL!")
                .arg(m_tower_id)
                .arg(Symbol::typeToString(m_symbol_type))
        );
        emit towerFull();
    }

    return true;
}

void Tower::reset() {
    if (m_level == 0) return;

    m_level = 0;
    emit levelChanged();

    DebugLogger::instance().info(
        QString("Tower %1 (%2) reset to 0")
            .arg(m_tower_id)
            .arg(Symbol::typeToString(m_symbol_type))
    );
}