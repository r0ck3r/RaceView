#pragma once

#include <QWidget>
#include <QString>
#include <QSize>
#include <QPixmap>

class OsdWidget : public QWidget {
    Q_OBJECT
public:
    explicit OsdWidget(QWidget *parent = nullptr);

    void showSeek(const QString &time, const QString &delta, double frac);
    void showVolume(const QString &text, double frac);
    void showState(const QString &text, const QPixmap &icon);

protected:
    void paintEvent(QPaintEvent *ev) override;
    QSize sizeHint() const override;

private:
    void recalc();

    QString m_main;
    QString m_sub;
    QPixmap m_icon;
    double m_frac = 0.0;
    bool m_hasBar = false;
    QSize m_size;
};
