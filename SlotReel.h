#pragma once

#include <QQuickPaintedItem>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QRandomGenerator>

class SlotReel : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(qreal missProbability READ missProbability WRITE setMissProbability NOTIFY missProbabilityChanged)
    Q_PROPERTY(bool spinning READ spinning NOTIFY spinningChanged)

public:
    explicit SlotReel(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    [[nodiscard]] qreal rotation() const { return m_rotation; }
    [[nodiscard]] bool spinning() const { return m_spinning; }
    [[nodiscard]] double missProbability() const { return m_missProbability; }

    void setRotation(qreal rotation);

    Q_INVOKABLE void setMissProbability(qreal probability);

    Q_INVOKABLE void spin();

    Q_INVOKABLE void setProbabilities(const QVariantMap &probabilities);

signals:
    void rotationChanged();

    void spinningChanged();

    void missProbabilityChanged();

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

    [[nodiscard]] qreal symbolHeight() const { return height(); }

    qreal m_rotation;
    bool m_spinning;
    QPropertyAnimation *m_spinAnimation;

    QMap<Symbol, QPixmap> m_symbolImages;
    QMap<Symbol, int> m_symbolProbabilities;
    QVector<Symbol> m_symbolSequence;

    qreal m_missProbability = 0.3; // 30% Chance für eine Niete
    bool m_landOnMiss = false; // Zeigt an, ob das Reel auf einer Niete landen soll

    static constexpr int SEQUENCE_LENGTH = 10;
};