#include "SlotReel.h"
#include <QPainterPath>
#include <cmath>

SlotReel::SlotReel(QQuickItem *parent)
    : QQuickPaintedItem(parent)
      , m_rotation(0.0)
      , m_spinning(false)
      , m_missProbability(0.0) {
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
            if (const int prob = probabilities[symbolNames[i]].toInt(); prob > 0) {
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

        if (QRectF symbolRect(0, symbolY, width(), currentSymbolHeight); symbolRect.intersects(boundingRect())) {
            paintSymbol(painter, symbol, symbolRect);
        }
    }
}

void SlotReel::paintSymbol(QPainter *painter, const Symbol symbol, const QRectF &rect) {
    if (m_symbolImages.contains(symbol) && !m_symbolImages[symbol].isNull()) {
        const qreal padding = qMin(rect.width(), rect.height()) * 0.1;
        QRectF imageRect = rect.adjusted(padding, padding, -padding, -padding);

        // Berechne das skalierte Rechteck unter Beibehaltung des Seitenverhältnisses
        const QSizeF pixmapSize = m_symbolImages[symbol].size();
        const qreal sourceAspectRatio = pixmapSize.width() / pixmapSize.height();

        if (const qreal targetAspectRatio = imageRect.width() / imageRect.height();
            sourceAspectRatio > targetAspectRatio) {
            // Bild ist breiter als Zielbereich - an Breite anpassen

            const qreal newHeight = imageRect.width() / sourceAspectRatio;
            const qreal yOffset = (imageRect.height() - newHeight) / 2;
            imageRect.adjust(0, yOffset, 0, -yOffset);
        } else {
            // Bild ist höher als Zielbereich - an Höhe anpassen

            const qreal newWidth = imageRect.height() * sourceAspectRatio;
            const qreal xOffset = (imageRect.width() - newWidth) / 2;
            imageRect.adjust(xOffset, 0, -xOffset, 0);
        }

        painter->drawPixmap(imageRect.toRect(), m_symbolImages[symbol]);
    }
}

void SlotReel::setRotation(const qreal rotation) {
    if (qFuzzyCompare(m_rotation, rotation))
        return;

    m_rotation = rotation;
    emit rotationChanged();
    update();
}

void SlotReel::setMissProbability(const qreal probability) {
    if (qFuzzyCompare(m_missProbability, probability))
        return;

    m_missProbability = qBound(0.0, probability, 1.0);
    emit missProbabilityChanged();
}

void SlotReel::spin() {
    if (m_spinning)
        return;

    m_spinning = true;
    emit spinningChanged();

    const qreal currentSymbolHeight = symbolHeight();
    if (currentSymbolHeight <= 0) return;

    const qreal currentRot = m_rotation;
    const qreal spins = 2 + QRandomGenerator::global()->bounded(3); // 2-4 volle Umdrehungen

    const double rand = QRandomGenerator::global()->generateDouble();
    qDebug() << "Random value for miss check:" << rand << " (miss probability:" << m_missProbability << ")";
    // Bestimme, ob wir auf einer Niete landen
    m_landOnMiss = rand < m_missProbability;
    qDebug() << "Landing on miss set to:" << m_landOnMiss;

    // Wähle zufällige Position
    const int finalSymbolIndex = QRandomGenerator::global()->bounded(SEQUENCE_LENGTH);
    qD

    qreal targetRotation = currentRot + (spins * currentSymbolHeight * SEQUENCE_LENGTH);

    if (m_landOnMiss) {
        // Füge einen halben Symbolabstand hinzu für die Niete
        targetRotation += (finalSymbolIndex + 0.5) * currentSymbolHeight;
    } else {
        // Lande direkt auf einem Symbol
        targetRotation += finalSymbolIndex * currentSymbolHeight;
    }

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
