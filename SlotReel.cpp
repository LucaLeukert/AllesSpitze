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

    // Stop any running animation first (safely)
    if (m_spin_animation->state() == QAbstractAnimation::Running) {
        m_spin_animation->stop();
    }

    m_spinning = true;
    emit spinning_changed();

    const qreal currentSymbolHeight = symbol_height();
    if (currentSymbolHeight <= 0) return;

    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;

    // Simple normalization - same as paint() uses
    qreal normalizedRotation = fmod(m_rotation, sequenceHeight);
    if (normalizedRotation < 0) normalizedRotation += sequenceHeight;
    m_rotation = normalizedRotation;

    const qreal randomValue = QRandomGenerator::global()->generateDouble();
    const qreal should_miss = (randomValue < m_miss_probability) ? 0.5 : 0.0;

    m_target_miss_offset = should_miss;

    // Spin 3-5 symbols
    const int symbolsToSpin = 3 + QRandomGenerator::global()->bounded(3);
    const qreal spinDistance = symbolsToSpin * currentSymbolHeight + (m_target_miss_offset * currentSymbolHeight);

    const qreal targetRotation = m_rotation + spinDistance;

    m_spin_animation->setStartValue(m_rotation);
    m_spin_animation->setEndValue(targetRotation);
    m_spin_animation->start();

#ifdef QT_DEBUG
    DebugLogger::instance().verbose(
        QString("Spin - Start: %1, Target: %2, Miss offset: %3")
            .arg(m_rotation)
            .arg(targetRotation)
            .arg(m_target_miss_offset)
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

    // DON'T modify m_rotation here - keep it exactly as the animation left it
    // The visual state should match what updateCurrentSymbol calculates

    updateCurrentSymbol();

    emit spinning_changed();
    update();
}

void SlotReel::updateCurrentSymbol() {
    const qreal currentSymbolHeight = symbol_height();
    if (currentSymbolHeight <= 0) return;

    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;

    // Use EXACTLY the same calculation as paint()
    qreal currentOffset = fmod(m_rotation, sequenceHeight);
    if (currentOffset < 0) currentOffset += sequenceHeight;

    const int startIndex = static_cast<int>(currentOffset / currentSymbolHeight);

    // Calculate symbolY for startIndex - same as paint() does for i=0
    // symbolY = startIndex * currentSymbolHeight - currentOffset
    // This is always <= 0 (symbol is at or above viewport top)
    const qreal symbolY_for_startIndex = startIndex * currentSymbolHeight - currentOffset;

    // symbolY_for_startIndex ranges from 0 (perfectly aligned) to -currentSymbolHeight (next symbol aligned)
    // When symbolY is 0: startIndex fills the viewport
    // When symbolY is -height/2: we're between startIndex and startIndex+1 (MISS)
    // When symbolY is -height: startIndex+1 fills the viewport

    // Calculate how much of startIndex is visible (0 to 1)
    // visible = 1 + (symbolY / height)  [since symbolY is negative or 0]
    const qreal visibleFraction = 1.0 + (symbolY_for_startIndex / currentSymbolHeight);

    // If more than 75% of a symbol is visible, it's the result
    // If between 25% and 75%, it's a miss (between symbols)
    const qreal alignmentThreshold = 0.75;

    int visibleSymbolIndex;
    bool isBetweenSymbols;

    if (visibleFraction >= alignmentThreshold) {
        // startIndex is mostly visible
        visibleSymbolIndex = startIndex;
        isBetweenSymbols = false;
    } else if (visibleFraction <= (1.0 - alignmentThreshold)) {
        // startIndex+1 is mostly visible
        visibleSymbolIndex = startIndex + 1;
        isBetweenSymbols = false;
    } else {
        // Between symbols - this is a miss
        visibleSymbolIndex = -1;
        isBetweenSymbols = true;
    }

    // Normalize index
    if (visibleSymbolIndex >= 0) {
        visibleSymbolIndex = visibleSymbolIndex % SEQUENCE_LENGTH;
        if (visibleSymbolIndex < 0) visibleSymbolIndex += SEQUENCE_LENGTH;
    }

    // Result is based ONLY on visual position, not on m_target_miss_offset
    if (isBetweenSymbols) {
        m_is_miss = true;
        m_current_symbol_type = Symbol::Type::Unknown;

        DebugLogger::instance().info(QString("Spin result: MISS (offset: %1, symbolY: %2, visible: %3)")
            .arg(currentOffset).arg(symbolY_for_startIndex).arg(visibleFraction));
    } else {
        m_is_miss = false;
        m_current_symbol_type = m_symbol_sequence[visibleSymbolIndex].type();

        DebugLogger::instance().info(
            QString("Spin result: %1 (index: %2, offset: %3, symbolY: %4, visible: %5)")
                .arg(Symbol::typeToString(m_current_symbol_type))
                .arg(visibleSymbolIndex)
                .arg(currentOffset)
                .arg(symbolY_for_startIndex)
                .arg(visibleFraction)
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