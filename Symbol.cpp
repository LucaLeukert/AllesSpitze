#include "Symbol.h"

Symbol::Symbol(const QString &imagePath, const int probability)
    : m_probability(probability) {
    m_pixmap = QPixmap(imagePath);
}
