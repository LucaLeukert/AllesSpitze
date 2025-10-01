#ifndef SLOTREEL_H
#define SLOTREEL_H

#include <QQuickPaintedItem>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QRandomGenerator>

class SlotReel : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(bool spinning READ spinning NOTIFY spinningChanged)

public:
    explicit SlotReel(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal rotation() const { return m_rotation; }
    void setRotation(qreal rotation);
    bool spinning() const { return m_spinning; }

    Q_INVOKABLE void spin();
    Q_INVOKABLE void setProbabilities(const QVariantMap &probabilities);

    signals:
        void rotationChanged();
    void spinningChanged();

private slots:
    void onSpinFinished();

private:
    enum Symbol {
        Coin = 0,
        Kleeblatt = 1,
        Marienkaefer = 2,
        Sonne = 3,
        Teufel = 4
    };

    void loadImages();
    void paintSymbol(QPainter *painter, Symbol symbol, const QRectF &rect);
    Symbol getRandomSymbol();
    void buildSymbolSequence();
    qreal symbolHeight() const { return height(); } // Dynamic symbol height

    qreal m_rotation;
    bool m_spinning;
    QPropertyAnimation *m_spinAnimation;

    QMap<Symbol, QPixmap> m_symbolImages;
    QMap<Symbol, int> m_symbolProbabilities;
    QVector<Symbol> m_symbolSequence;

    static constexpr int SEQUENCE_LENGTH = 10; // Length of pre-built symbol sequence
};

#endif // SLOTREEL_H