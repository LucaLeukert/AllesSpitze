#include "Symbol.h"

Symbol::Symbol(const QString &imagePath, const Type type, const int probability)
    : m_probability(probability), m_type(type) {
    m_pixmap.load(imagePath);
}

QString Symbol::typeToString(Type type) {
    switch (type) {
        case Type::Coin: return "coin";
        case Type::Kleeblatt: return "kleeblatt";
        case Type::Marienkaefer: return "marienkaefer";
        case Type::Sonne: return "sonne";
        case Type::Teufel: return "teufel";
        default: return "unknown";
    }
}