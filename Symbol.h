#pragma once

#include <QPixmap>
#include <memory>

class Symbol {
public:
    enum class Type {
        Coin = 0,
        Kleeblatt = 1,
        Marienkaefer = 2,
        Sonne = 3,
        Teufel = 4,
        Unknown = -1
    };

    Symbol(const QString &imagePath, Type type, int probability);

    [[nodiscard]] std::shared_ptr<QPixmap> pixmap() const {
        return std::make_shared<QPixmap>(m_pixmap);
    }
    [[nodiscard]] int probability() const { return m_probability; }
    [[nodiscard]] Type type() const { return m_type; }
    [[nodiscard]] bool isValid() const { return !m_pixmap.isNull(); }

    static QString typeToString(Type type);

private:
    QPixmap m_pixmap;
    int m_probability;
    Type m_type;
};