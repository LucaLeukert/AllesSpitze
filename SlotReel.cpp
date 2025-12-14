#include "SlotReel.h"
#include "DebugLogger.h"
#include <QPainterPath>
#include <QCoreApplication>
#include <QDebug>
#include <cmath>

SlotReel::SlotReel(QQuickItem *parent)
    : QQuickPaintedItem(parent)
      , m_spinning(false)
      , m_rotation(0.0)
      , m_miss_probability(0.70)
      , m_current_miss_offset(0.0)
      , m_target_miss_offset(0.0) {
    // Make it much larger to fill screen height
    setWidth(600);
    setHeight(600);

    m_symbols = {
        Symbol(":/images/marienkaefer.png", Symbol::Type::Marienkaefer, 3),
        Symbol(":/images/coin.png",        Symbol::Type::Coin,         14),
        Symbol(":/images/kleeblatt.png",   Symbol::Type::Kleeblatt,    24),
        Symbol(":/images/sonne.png",       Symbol::Type::Sonne,         2),
        Symbol(":/images/teufel.png",      Symbol::Type::Teufel,        9)
    };


    for (const auto &symbol : m_symbols) {
        if (!symbol.isValid()) {
            qWarning() << "Failed to load symbol image";
            QCoreApplication::exit(-1);
        }
    }

    build_symbol_sequence();

    m_spin_animation = new QPropertyAnimation(this, "rotation", this);
    m_spin_animation->setDuration(2000);
    m_spin_animation->setEasingCurve(QEasingCurve::OutQuart);

    connect(m_spin_animation, &QPropertyAnimation::finished,
            this, &SlotReel::on_spin_finished);
}

void SlotReel::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setClipRect(boundingRect());

    const qreal currentSymbolHeight = symbol_height();
    if (currentSymbolHeight <= 0) return;

    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;
    const qreal currentOffset = fmod(m_rotation, sequenceHeight);
    const int startIndex = static_cast<int>(currentOffset / currentSymbolHeight);

    for (int i = -1; i <= 1; ++i) {
        int symbolIndex = (startIndex + i) % SEQUENCE_LENGTH;
        if (symbolIndex < 0) symbolIndex += SEQUENCE_LENGTH;

        const Symbol &symbol = m_symbol_sequence[symbolIndex];
        const qreal symbolY = (startIndex + i) * currentSymbolHeight - currentOffset;

        if (QRectF symbolRect(0, symbolY, width(), currentSymbolHeight);
            symbolRect.intersects(boundingRect())) {
            paint_symbol(painter, symbol, symbolRect);
        }
    }
}

void SlotReel::set_rotation(const qreal rotation) {
    if (qFuzzyCompare(m_rotation, rotation))
        return;

    m_rotation = rotation;
    emit rotation_changed();
    update();
}

void SlotReel::set_miss_probability(const qreal probability) {
    const qreal clampedProb = qBound(0.0, probability, 1.0);
    if (qFuzzyCompare(m_miss_probability, clampedProb))
        return;

    m_miss_probability = clampedProb;
    emit miss_probability_changed();
}

void SlotReel::spin() {
    if (m_spinning)
        return;

    m_spinning = true;
    emit spinning_changed();

    const qreal currentSymbolHeight = symbol_height();
    if (currentSymbolHeight <= 0) return;

    // Normalize rotation BEFORE spinning to prevent overflow
    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;
    const qreal baseRotation = m_rotation - (m_current_miss_offset * currentSymbolHeight);
    const qreal normalizedBase = fmod(baseRotation, sequenceHeight);
    const qreal normalizedRotation = (normalizedBase < 0 ? normalizedBase + sequenceHeight : normalizedBase)
                                     + (m_current_miss_offset * currentSymbolHeight);

    // Update current rotation to normalized value
    m_rotation = normalizedRotation;

    const qreal randomValue = QRandomGenerator::global()->generateDouble();
    const qreal should_miss = (randomValue < m_miss_probability) ? 0.5 : 0.0;

    m_target_miss_offset = should_miss;

    // Reduced spin distance - only 3-5 symbols
    const int symbolsToSpin = 3 + QRandomGenerator::global()->bounded(3);
    const qreal baseTarget = symbolsToSpin * currentSymbolHeight;

    // Calculate target from normalized rotation
    const qreal currentBaseRotation = m_rotation - (m_current_miss_offset * currentSymbolHeight);
    const qreal targetRotation = currentBaseRotation + baseTarget + (m_target_miss_offset * currentSymbolHeight);

    m_spin_animation->setStartValue(m_rotation);
    m_spin_animation->setEndValue(targetRotation);
    m_spin_animation->start();

#ifdef QT_DEBUG
    DebugLogger::instance().verbose(
        QString("Spin - Normalized rotation: %1, Target: %2")
            .arg(m_rotation)
            .arg(targetRotation)
    );
#endif
}

void SlotReel::set_probabilities(const QVariantMap &probabilities) {
    struct SymbolConfig {
        QString key;
        Symbol::Type type;
        QString path;
    };

    QVector<SymbolConfig> configs = {
        {"coin", Symbol::Type::Coin, ":/images/coin.png"},
        {"kleeblatt", Symbol::Type::Kleeblatt, ":/images/kleeblatt.png"},
        {"marienkaefer", Symbol::Type::Marienkaefer, ":/images/marienkaefer.png"},
        {"sonne", Symbol::Type::Sonne, ":/images/sonne.png"},
        {"teufel", Symbol::Type::Teufel, ":/images/teufel.png"}
    };

    m_symbols.clear();
    for (const auto &config : configs) {
        int prob = probabilities.contains(config.key)
            ? probabilities[config.key].toInt()
            : 20;
        if (prob > 0) {
            m_symbols.append(Symbol(config.path, config.type, prob));
        }
    }

    build_symbol_sequence();
}

void SlotReel::on_spin_finished() {
    m_spinning = false;
    m_current_miss_offset = m_target_miss_offset;

    const qreal currentSymbolHeight = symbol_height();
    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;

    const qreal baseRotation = m_rotation - (m_current_miss_offset * currentSymbolHeight);
    const qreal normalizedBase = fmod(baseRotation, sequenceHeight);
    m_rotation = (normalizedBase < 0 ? normalizedBase + sequenceHeight : normalizedBase)
                 + (m_current_miss_offset * currentSymbolHeight);

    updateCurrentSymbol();

    emit spinning_changed();
    update();
}

void SlotReel::updateCurrentSymbol() {
    const qreal currentSymbolHeight = symbol_height();

    if (m_target_miss_offset > 0.0) {
        m_is_miss = true;
        m_current_symbol_type = Symbol::Type::Unknown;

        DebugLogger::instance().info("Spin result: MISS");
    } else {
        m_is_miss = false;

        const qreal rotation = m_rotation - (m_current_miss_offset * currentSymbolHeight);
        const int symbolIndex = static_cast<int>(rotation / currentSymbolHeight) % SEQUENCE_LENGTH;

        m_current_symbol_type = m_symbol_sequence[symbolIndex].type();

        DebugLogger::instance().info(
            QString("Spin result: %1")
                .arg(Symbol::typeToString(m_current_symbol_type))
        );
    }

    emit currentSymbolTypeChanged();
    emit isMissChanged();
}

void SlotReel::paint_symbol(QPainter *painter, const Symbol &symbol, const QRectF &rect) {
    if (const auto pixmap = symbol.pixmap(); pixmap && !pixmap->isNull()) {
        const qreal padding = qMin(rect.width(), rect.height()) * 0.1;
        QRectF imageRect = rect.adjusted(padding, padding, -padding, -padding);

        const QSizeF pixmapSize = pixmap->size();
        const qreal sourceAspectRatio = pixmapSize.width() / pixmapSize.height();

        if (const qreal targetAspectRatio = imageRect.width() / imageRect.height();
            sourceAspectRatio > targetAspectRatio) {
            const qreal newHeight = imageRect.width() / sourceAspectRatio;
            const qreal yOffset = (imageRect.height() - newHeight) / 2;
            imageRect.adjust(0, yOffset, 0, -yOffset);
        } else {
            const qreal newWidth = imageRect.height() * sourceAspectRatio;
            const qreal xOffset = (imageRect.width() - newWidth) / 2;
            imageRect.adjust(xOffset, 0, -xOffset, 0);
        }

        painter->drawPixmap(imageRect.toRect(), *pixmap);
    }
}

void SlotReel::build_symbol_sequence() {
    m_symbol_sequence.clear();
    m_symbol_sequence.reserve(SEQUENCE_LENGTH);

    QVector<Symbol> weightedSymbols;
    for (const auto &symbol : m_symbols) {
        for (int i = 0; i < symbol.probability(); ++i) {
            weightedSymbols.append(symbol);
        }
    }

    Symbol::Type lastType = Symbol::Type::Unknown;

    for (int i = 0; i < SEQUENCE_LENGTH; ++i) {
        // Create a filtered pool excluding the last symbol type
        QVector<Symbol> availableSymbols;
        if (lastType == Symbol::Type::Unknown) {
            availableSymbols = weightedSymbols;
        } else {
            for (const auto &symbol : weightedSymbols) {
                if (symbol.type() != lastType) {
                    availableSymbols.append(symbol);
                }
            }
        }

        // Safety fallback (shouldn't happen with 5 different symbol types)
        if (availableSymbols.isEmpty()) {
            availableSymbols = weightedSymbols;
        }

        const int randomIndex = QRandomGenerator::global()->bounded(availableSymbols.size());
        Symbol selectedSymbol = availableSymbols[randomIndex];

        m_symbol_sequence.append(selectedSymbol);
        lastType = selectedSymbol.type();
    }
}