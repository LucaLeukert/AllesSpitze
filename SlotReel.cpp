#include "SlotReel.h"
#include <QPainterPath>
#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>
#include <cmath>

SlotReel::SlotReel(QQuickItem *parent)
    : QQuickPaintedItem(parent)
      , m_spinning(false)
      , m_rotation(0.0)
      , m_miss_probability(0.0)
      , m_current_miss_offset(0.0)
      , m_target_miss_offset(0.0) {
    setWidth(300);
    setHeight(300);

    // Initialize symbols with their images and probabilities
    m_symbols = {
        Symbol(":/images/coin.png", 30),
        Symbol(":/images/kleeblatt.png", 25),
        Symbol(":/images/marienkaefer.png", 20),
        Symbol(":/images/sonne.png", 15),
        Symbol(":/images/teufel.png", 10)
    };

    // Verify all symbols loaded correctly
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

void SlotReel::build_symbol_sequence() {
    m_symbol_sequence.clear();
    m_symbol_sequence.reserve(SEQUENCE_LENGTH);

    // Create weighted sequence based on probabilities
    QVector<Symbol> weightedSymbols;
    for (const auto &symbol : m_symbols) {
        for (int i = 0; i < symbol.probability(); ++i) {
            weightedSymbols.append(symbol);
        }
    }

    // Build random sequence
    for (int i = 0; i < SEQUENCE_LENGTH; ++i) {
        const int randomIndex = QRandomGenerator::global()->bounded(weightedSymbols.size());
        m_symbol_sequence.append(weightedSymbols[randomIndex]);
    }
}

void SlotReel::set_probabilities(const QVariantMap &probabilities) {
    QStringList symbolNames = {"coin", "kleeblatt", "marienkaefer", "sonne", "teufel"};

    for (int i = 0; i < m_symbols.size() && i < symbolNames.size(); ++i) {
        if (probabilities.contains(symbolNames[i])) {
            if (const int prob = probabilities[symbolNames[i]].toInt(); prob > 0) {
                m_symbols[i] = Symbol(":/images/" + symbolNames[i] + ".png", prob);
            }
        }
    }

    build_symbol_sequence();
}

void SlotReel::set_miss_probability(const qreal probability) {
    const qreal clampedProb = qBound(0.0, probability, 1.0);
    if (qFuzzyCompare(m_miss_probability, clampedProb))
        return;

    m_miss_probability = clampedProb;
    emit miss_probability_changed();
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

void SlotReel::set_rotation(const qreal rotation) {
    if (qFuzzyCompare(m_rotation, rotation))
        return;

    m_rotation = rotation;
    emit rotation_changed();
    update();
}

void SlotReel::spin() {
    if (m_spinning)
        return;

    m_spinning = true;
    emit spinning_changed();

    const qreal currentSymbolHeight = symbol_height();
    if (currentSymbolHeight <= 0) return;

    const qreal randomValue = QRandomGenerator::global()->generateDouble();
    const qreal should_miss = (randomValue < m_miss_probability) ? 0.5 : 0.0;

    qDebug() << "Spin initiated. Random value:" << randomValue
            << "Miss probability:" << m_miss_probability
            << "Resulting in" << (should_miss == 0.5 ? "a miss." : "a hit.");

    m_target_miss_offset = should_miss;

    const int spins = 1 + QRandomGenerator::global()->bounded(2);
    const int finalSymbolIndex = QRandomGenerator::global()->bounded(SEQUENCE_LENGTH);
    const qreal baseTarget = (spins * SEQUENCE_LENGTH + finalSymbolIndex) * currentSymbolHeight;
    const qreal currentBaseRotation = m_rotation - (m_current_miss_offset * currentSymbolHeight);
    const qreal targetRotation = currentBaseRotation + baseTarget + (m_target_miss_offset * currentSymbolHeight);

    m_spin_animation->setStartValue(m_rotation);
    m_spin_animation->setEndValue(targetRotation);
    m_spin_animation->start();
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

    emit spinning_changed();
    update();
}
