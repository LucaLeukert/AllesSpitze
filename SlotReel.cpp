#include "SlotReel.h"
#include <QPainterPath>
#include <cmath>

SlotReel::SlotReel(QQuickItem *parent)
    : QQuickPaintedItem(parent)
      , m_rotation(0.0)
      , m_spinning(false) {
    setWidth(300);
    setHeight(300);

    // Default probabilities (higher number = more likely)
    m_symbolProbabilities[Coin] = 30; // Most common
    m_symbolProbabilities[Kleeblatt] = 25; // Common
    m_symbolProbabilities[Marienkaefer] = 20; // Medium
    m_symbolProbabilities[Sonne] = 15; // Uncommon
    m_symbolProbabilities[Teufel] = 10; // Rare

    loadImages();
    buildSymbolSequence();

    m_spinAnimation = new QPropertyAnimation(this, "rotation", this);
    m_spinAnimation->setDuration(2000);
    m_spinAnimation->setEasingCurve(QEasingCurve::OutQuart);

    connect(m_spinAnimation, &QPropertyAnimation::finished,
            this, &SlotReel::onSpinFinished);
}

void SlotReel::loadImages() {
    // Load symbol images from resources (with /images prefix)
    m_symbolImages[Coin] = QPixmap(":/images/coin.png");
    m_symbolImages[Kleeblatt] = QPixmap(":/images/kleeblatt.png");
    m_symbolImages[Marienkaefer] = QPixmap(":/images/marienkaefer.png");
    m_symbolImages[Sonne] = QPixmap(":/images/sonne.png");
    m_symbolImages[Teufel] = QPixmap(":/images/teufel.png");

    // Check if images loaded successfully
    QStringList symbolNames = {"Coin", "Kleeblatt", "Marienkaefer", "Sonne", "Teufel"};
    for (auto it = m_symbolImages.begin(); it != m_symbolImages.end(); ++it) {
        if (it.value().isNull()) {
            qWarning() << "Failed to load" << symbolNames[it.key()] << "image";
            QCoreApplication::exit(-1);
        } else {
            qDebug() << "Successfully loaded" << symbolNames[it.key()] << "image";
        }
    }
}

void SlotReel::buildSymbolSequence() {
    m_symbolSequence.clear();
    m_symbolSequence.reserve(SEQUENCE_LENGTH);

    // Create weighted sequence based on probabilities
    QVector<Symbol> weightedSymbols;
    for (auto it = m_symbolProbabilities.begin(); it != m_symbolProbabilities.end(); ++it) {
        const Symbol symbol = it.key();
        const int weight = it.value();
        for (int i = 0; i < weight; ++i) {
            weightedSymbols.append(symbol);
        }
    }

    // Build random sequence
    for (int i = 0; i < SEQUENCE_LENGTH; ++i) {
        const int randomIndex = QRandomGenerator::global()->bounded(weightedSymbols.size());
        m_symbolSequence.append(weightedSymbols[randomIndex]);
    }
}

void SlotReel::setProbabilities(const QVariantMap &probabilities) {
    // Update probabilities from QML
    QStringList symbolNames = {"coin", "kleeblatt", "marienkaefer", "sonne", "teufel"};
    QList symbols = {Coin, Kleeblatt, Marienkaefer, Sonne, Teufel};

    for (int i = 0; i < symbolNames.size(); ++i) {
        if (probabilities.contains(symbolNames[i])) {
            int prob = probabilities[symbolNames[i]].toInt();
            if (prob > 0) {
                m_symbolProbabilities[symbols[i]] = prob;
            }
        }
    }

    buildSymbolSequence();
}

void SlotReel::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    // Set up clipping to show only the widget area
    painter->setClipRect(boundingRect());

    // Use dynamic symbol height (equals widget height)
    const qreal currentSymbolHeight = symbolHeight();
    if (currentSymbolHeight <= 0) return; // Avoid division by zero

    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;
    const qreal currentOffset = fmod(m_rotation, sequenceHeight);

    // Calculate which symbols to draw
    const int startIndex = static_cast<int>(currentOffset / currentSymbolHeight);

    // Draw symbols that are visible (with buffer for smooth scrolling)
    for (int i = -1; i <= 1; ++i) {
        int symbolIndex = (startIndex + i) % SEQUENCE_LENGTH;
        if (symbolIndex < 0) symbolIndex += SEQUENCE_LENGTH;

        const Symbol symbol = m_symbolSequence[symbolIndex];
        const qreal symbolY = (startIndex + i) * currentSymbolHeight - currentOffset;

        QRectF symbolRect(0, symbolY, width(), currentSymbolHeight);
        if (symbolRect.intersects(boundingRect())) {
            paintSymbol(painter, symbol, symbolRect);
        }
    }
}

void SlotReel::paintSymbol(QPainter *painter, const Symbol symbol, const QRectF &rect) {
    if (m_symbolImages.contains(symbol) && !m_symbolImages[symbol].isNull()) {
        // Draw the image centered in the rect with some padding
        const qreal padding = qMin(rect.width(), rect.height()) * 0.1;
        const QRectF imageRect = rect.adjusted(padding, padding, -padding, -padding);
        painter->drawPixmap(imageRect.toRect(), m_symbolImages[symbol]);
    } else {
        // Fallback: draw colored rectangle with text if image not found
        const QColor fallbackColors[] = {
            QColor(0xFFD700), // Coin - Gold
            QColor(0x228B22), // Kleeblatt - Green
            QColor(0xFF0000), // Marienkaefer - Red
            QColor(0xFFA500), // Sonne - Orange
            QColor(0x8B0000)  // Teufel - Dark Red
        };

        painter->setBrush(fallbackColors[symbol]);
        painter->setPen(Qt::white);
        const qreal padding = qMin(rect.width(), rect.height()) * 0.1;
        const QRectF fallbackRect = rect.adjusted(padding, padding, -padding, -padding);
        painter->drawRect(fallbackRect);

        QStringList symbolNames = {"COIN", "KLEEBLATT", "KÄFER", "SONNE", "TEUFEL"};
        painter->drawText(fallbackRect, Qt::AlignCenter, symbolNames[symbol]);
    }
}

void SlotReel::setRotation(qreal rotation) {
    if (qFuzzyCompare(m_rotation, rotation))
        return;

    m_rotation = rotation;
    emit rotationChanged();
    update();
}

void SlotReel::spin() {
    if (m_spinning)
        return;

    m_spinning = true;
    emit spinningChanged();

    // Use dynamic symbol height for calculations
    const qreal currentSymbolHeight = symbolHeight();
    if (currentSymbolHeight <= 0) return;

    // Calculate random final position
    const qreal currentRot = m_rotation;
    const qreal spins = 2 + QRandomGenerator::global()->bounded(3); // 5-14 full rotations

    // Choose random final symbol position
    const int finalSymbolIndex = QRandomGenerator::global()->bounded(SEQUENCE_LENGTH);
    const qreal targetRotation = currentRot + (spins * currentSymbolHeight * SEQUENCE_LENGTH) +
                                 (finalSymbolIndex * currentSymbolHeight);

    m_spinAnimation->setStartValue(currentRot);
    m_spinAnimation->setEndValue(targetRotation);
    m_spinAnimation->start();
}

void SlotReel::onSpinFinished() {
    m_spinning = false;
    emit spinningChanged();
}

SlotReel::Symbol SlotReel::getRandomSymbol() {
    return m_symbolSequence[QRandomGenerator::global()->bounded(SEQUENCE_LENGTH)];
}