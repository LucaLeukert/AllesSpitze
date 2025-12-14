#pragma once

#include <QObject>
#include "Symbol.h"

class Tower : public QObject {
    Q_OBJECT
    Q_PROPERTY(int level READ level NOTIFY levelChanged)
    Q_PROPERTY(int symbolType READ symbolType CONSTANT)
    Q_PROPERTY(int towerId READ towerId CONSTANT)

public:
    explicit Tower(Symbol::Type symbolType, int towerId, QObject *parent = nullptr);

    int level() const { return m_level; }
    int symbolType() const { return static_cast<int>(m_symbol_type); }
    Symbol::Type symbolTypeEnum() const { return m_symbol_type; }
    int towerId() const { return m_tower_id; }

    Q_INVOKABLE bool increase();
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool isFull() const { return m_level >= 5; }

    signals:
        void levelChanged();
    void towerFull();

private:
    Symbol::Type m_symbol_type;
    int m_tower_id;
    int m_level = 0;
    static constexpr int MAX_LEVEL = 5;
};