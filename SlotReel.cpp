#include "SlotReel.h"
#include <QPainterPath>
#include <cmath>

SlotReel::SlotReel(QQuickItem *parent)
    : QQuickPaintedItem(parent)
      , m_rotation(0.0)
      , m_spinning(false)
      , m_miss_probability(0.0)
      , m_current_miss_offset(0.0)
      , m_target_miss_offset(0.0) {
    setWidth(300);
    setHeight(300);

    // Default probabilities (higher number = more likely)
    m_symbol_probabilities[Coin] = 30;
    m_symbol_probabilities[Kleeblatt] = 25;
    m_symbol_probabilities[Marienkaefer] = 20;
    m_symbol_probabilities[Sonne] = 15;
    m_symbol_probabilities[Teufel] = 10;

    load_images();
    build_symbol_sequence();

    m_spin_animation = new QPropertyAnimation(this, "rotation", this);
    m_spin_animation->setDuration(2000);
    m_spin_animation->setEasingCurve(QEasingCurve::OutQuart);

    connect(m_spin_animation, &QPropertyAnimation::finished,
            this, &SlotReel::on_spin_finished);
}

void SlotReel::load_images() {
    // Load symbol images from resources (with /images prefix)
    m_symbol_images[Coin] = QPixmap(":/images/coin.png");
    m_symbol_images[Kleeblatt] = QPixmap(":/images/kleeblatt.png");
    m_symbol_images[Marienkaefer] = QPixmap(":/images/marienkaefer.png");
    m_symbol_images[Sonne] = QPixmap(":/images/sonne.png");
    m_symbol_images[Teufel] = QPixmap(":/images/teufel.png");

    // Check if images loaded successfully
    QStringList symbolNames = {"Coin", "Kleeblatt", "Marienkaefer", "Sonne", "Teufel"};
    for (auto it = m_symbol_images.begin(); it != m_symbol_images.end(); ++it) {
        if (it.value().isNull()) {
            qWarning() << "Failed to load" << symbolNames[it.key()] << "image";
            QCoreApplication::exit(-1);
        } else {
            qDebug() << "Successfully loaded" << symbolNames[it.key()] << "image";
        }
    }
}

void SlotReel::build_symbol_sequence() {
    m_symbol_sequence.clear();
    m_symbol_sequence.reserve(SEQUENCE_LENGTH);

    // Create weighted sequence based on probabilities
    QVector<Symbol> weightedSymbols;
    for (auto it = m_symbol_probabilities.begin(); it != m_symbol_probabilities.end(); ++it) {
        const Symbol symbol = it.key();
        const int weight = it.value();
        for (int i = 0; i < weight; ++i) {
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
    // Update probabilities from QML
    QStringList symbolNames = {"coin", "kleeblatt", "marienkaefer", "sonne", "teufel"};
    QList symbols = {Coin, Kleeblatt, Marienkaefer, Sonne, Teufel};

    for (int i = 0; i < symbolNames.size(); ++i) {
        if (probabilities.contains(symbolNames[i])) {
            if (const int prob = probabilities[symbolNames[i]].toInt(); prob > 0) {
                m_symbol_probabilities[symbols[i]] = prob;
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

    // Set up clipping to show only the widget area
    painter->setClipRect(boundingRect());

    // Use dynamic symbol height (equals widget height)
    const qreal currentSymbolHeight = symbol_height();
    if (currentSymbolHeight <= 0) return; // Avoid division by zero

    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;
    const qreal currentOffset = fmod(m_rotation, sequenceHeight);

    // Calculate which symbols to draw
    const int startIndex = static_cast<int>(currentOffset / currentSymbolHeight);

    // Draw symbols that are visible (with buffer for smooth scrolling)
    for (int i = -1; i <= 1; ++i) {
        int symbolIndex = (startIndex + i) % SEQUENCE_LENGTH;
        if (symbolIndex < 0) symbolIndex += SEQUENCE_LENGTH;

        const Symbol symbol = m_symbol_sequence[symbolIndex];
        const qreal symbolY = (startIndex + i) * currentSymbolHeight - currentOffset;

        if (QRectF symbolRect(0, symbolY, width(), currentSymbolHeight);
            symbolRect.intersects(boundingRect())) {
            paint_symbol(painter, symbol, symbolRect);
        }
    }
}

void SlotReel::paint_symbol(QPainter *painter, const Symbol symbol, const QRectF &rect) {
    if (m_symbol_images.contains(symbol) && !m_symbol_images[symbol].isNull()) {
        const qreal padding = qMin(rect.width(), rect.height()) * 0.1;
        QRectF imageRect = rect.adjusted(padding, padding, -padding, -padding);

        // Calculate scaled rectangle maintaining aspect ratio
        const QSizeF pixmapSize = m_symbol_images[symbol].size();
        const qreal sourceAspectRatio = pixmapSize.width() / pixmapSize.height();

        if (const qreal targetAspectRatio = imageRect.width() / imageRect.height();
            sourceAspectRatio > targetAspectRatio) {
            // Image is wider - fit to width
            const qreal newHeight = imageRect.width() / sourceAspectRatio;
            const qreal yOffset = (imageRect.height() - newHeight) / 2;
            imageRect.adjust(0, yOffset, 0, -yOffset);
        } else {
            // Image is taller - fit to height
            const qreal newWidth = imageRect.height() * sourceAspectRatio;
            const qreal xOffset = (imageRect.width() - newWidth) / 2;
            imageRect.adjust(xOffset, 0, -xOffset, 0);
        }

        painter->drawPixmap(imageRect.toRect(), m_symbol_images[symbol]);
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

    // Decide if this spin should result in a miss
    const qreal randomValue = QRandomGenerator::global()->generateDouble();
    const qreal should_miss = (randomValue < m_miss_probability) ? 0.5 : 0.0;

    // log the decision
    qDebug() << "Spin initiated. Random value:" << randomValue
            << "Miss probability:" << m_miss_probability
            << "Resulting in" << (should_miss == 0.5 ? "a miss." : "a hit.");

    m_target_miss_offset = should_miss;

    // Calculate base spins (1-3 full rotations)
    const int spins = 1 + QRandomGenerator::global()->bounded(2);

    // Choose a random symbol index
    const int finalSymbolIndex = QRandomGenerator::global()->bounded(SEQUENCE_LENGTH);

    // Calculate the base target position (aligned to symbol grid)
    const qreal baseTarget = (spins * SEQUENCE_LENGTH + finalSymbolIndex) * currentSymbolHeight;

    // Adjust for current miss offset to maintain alignment
    const qreal currentBaseRotation = m_rotation - (m_current_miss_offset * currentSymbolHeight);

    // Calculate target rotation with new miss offset
    const qreal targetRotation = currentBaseRotation + baseTarget + (m_target_miss_offset * currentSymbolHeight);

    m_spin_animation->setStartValue(m_rotation);
    m_spin_animation->setEndValue(targetRotation);
    m_spin_animation->start();
}

void SlotReel::on_spin_finished() {
    m_spinning = false;

    // Update current miss offset
    m_current_miss_offset = m_target_miss_offset;

    // Normalize the rotation to keep it within bounds
    const qreal currentSymbolHeight = symbol_height();
    const qreal sequenceHeight = currentSymbolHeight * SEQUENCE_LENGTH;

    // Normalize considering the miss offset
    const qreal baseRotation = m_rotation - (m_current_miss_offset * currentSymbolHeight);
    const qreal normalizedBase = fmod(baseRotation, sequenceHeight);
    m_rotation = (normalizedBase < 0 ? normalizedBase + sequenceHeight : normalizedBase)
                 + (m_current_miss_offset * currentSymbolHeight);

    emit spinning_changed();
    update();
}

SlotReel::Symbol SlotReel::get_random_symbol() {
    return m_symbol_sequence[QRandomGenerator::global()->bounded(SEQUENCE_LENGTH)];
}
