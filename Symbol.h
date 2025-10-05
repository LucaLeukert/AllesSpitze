#pragma once

#include <QPixmap>
#include <memory>

class Symbol {
public:
    Symbol(const QString &imagePath, int probability);

    std::shared_ptr<QPixmap> pixmap() const { return std::make_shared<QPixmap>(m_pixmap); }
    int probability() const { return m_probability; }
    bool isValid() const { return !m_pixmap.isNull(); }

private:
    QPixmap m_pixmap;
    int m_probability;
};
