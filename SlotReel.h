#pragma once

#include <QQuickPaintedItem>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QRandomGenerator>

class SlotReel : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE set_rotation NOTIFY rotation_changed)
    Q_PROPERTY(bool spinning READ spinning NOTIFY spinning_changed)
    Q_PROPERTY(qreal miss_probability READ miss_probability WRITE set_miss_probability NOTIFY miss_probability_changed)

public:
    explicit SlotReel(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    [[nodiscard]] qreal rotation() const { return m_rotation; }
    [[nodiscard]] bool spinning() const { return m_spinning; }
    [[nodiscard]] qreal miss_probability() const { return m_miss_probability; }

    void set_rotation(qreal rotation);

    Q_INVOKABLE void set_miss_probability(qreal probability);

    Q_INVOKABLE void spin();

    Q_INVOKABLE void set_probabilities(const QVariantMap &probabilities);

signals:
    void rotation_changed();

    void spinning_changed();

    void miss_probability_changed();

private slots:
    void on_spin_finished();

private:
    enum Symbol {
        Coin = 0,
        Kleeblatt = 1,
        Marienkaefer = 2,
        Sonne = 3,
        Teufel = 4
    };

    void load_images();

    void paint_symbol(QPainter *painter, Symbol symbol, const QRectF &rect);

    Symbol get_random_symbol();

    void build_symbol_sequence();

    [[nodiscard]] qreal symbol_height() const { return height(); }

    bool m_spinning;
    qreal m_rotation;
    qreal m_miss_probability;
    qreal m_current_miss_offset; // 0.0 for hit, 0.5 for miss
    qreal m_target_miss_offset; // Target offset after spin

    QPropertyAnimation *m_spin_animation;
    QMap<Symbol, QPixmap> m_symbol_images;
    QMap<Symbol, int> m_symbol_probabilities;
    QVector<Symbol> m_symbol_sequence;

    static constexpr int SEQUENCE_LENGTH = 20;
};
