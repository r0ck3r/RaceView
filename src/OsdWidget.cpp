#include "OsdWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>

namespace {
constexpr int PAD = 26;
constexpr int GAP = 16;
constexpr int BARW = 460;
constexpr int BARH = 16;
constexpr int MAIN_PX = 52;
constexpr int SUB_PX = 26;
constexpr int ICON_GAP = 18;
} // namespace

OsdWidget::OsdWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void OsdWidget::showSeek(const QString &time, const QString &delta,
                         double frac) {
    m_main = time;
    m_sub = delta;
    m_frac = qBound(0.0, frac, 1.0);
    m_hasBar = true;
    m_icon = QPixmap();
    recalc();
}

void OsdWidget::showVolume(const QString &text, double frac) {
    m_main = text;
    m_sub.clear();
    m_frac = qBound(0.0, frac, 1.0);
    m_hasBar = true;
    m_icon = QPixmap();
    recalc();
}

void OsdWidget::showState(const QString &text, const QPixmap &icon) {
    m_main = text;
    m_sub.clear();
    m_hasBar = false;
    m_icon = icon;
    recalc();
}

QSize OsdWidget::sizeHint() const { return m_size; }

void OsdWidget::recalc() {
    QFont f1 = font();
    f1.setPixelSize(MAIN_PX);
    f1.setBold(true);
    QFont f2 = font();
    f2.setPixelSize(SUB_PX);

    QFontMetrics m1(f1), m2(f2);
    int w = m1.horizontalAdvance(m_main);
    int h = m1.height();
    if (!m_icon.isNull()) {
        w += ICON_GAP + m_icon.width();
        h = qMax(h, m_icon.height());
    }
    if (!m_sub.isEmpty()) {
        w = qMax(w, m2.horizontalAdvance(m_sub));
        h += GAP + m2.height();
    }
    if (m_hasBar) {
        w = qMax(w, BARW);
        h += GAP + BARH;
    }
    w += 2 * PAD;
    h += 2 * PAD;
    m_size = QSize(w, h);
    setFixedSize(m_size);
    update();
}

void OsdWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    QPainterPath bg;
    bg.addRoundedRect(rect(), 14, 14);
    p.fillPath(bg, QColor(0, 0, 0, 200));

    QFont f1 = font();
    f1.setPixelSize(MAIN_PX);
    f1.setBold(true);
    QFont f2 = font();
    f2.setPixelSize(SUB_PX);
    QFontMetrics m1(f1), m2(f2);

    int y = PAD;
    p.setPen(Qt::white);
    p.setFont(f1);
    const int textW = m1.horizontalAdvance(m_main);
    const int iconW = m_icon.isNull() ? 0 : m_icon.width();
    const int blockW = textW + (iconW ? ICON_GAP + iconW : 0);
    int x = (width() - blockW) / 2;
    if (iconW) {
        p.drawPixmap(x, y + (m1.height() - m_icon.height()) / 2, m_icon);
        x += iconW + ICON_GAP;
    }
    p.drawText(QRect(x, y, textW, m1.height()),
               Qt::AlignLeft | Qt::AlignVCenter, m_main);
    y += m1.height();

    if (!m_sub.isEmpty()) {
        y += GAP;
        p.setFont(f2);
        p.setPen(QColor(225, 225, 225));
        p.drawText(QRect(0, y, width(), m2.height()),
                   Qt::AlignHCenter | Qt::AlignVCenter, m_sub);
        y += m2.height();
    }

    if (m_hasBar) {
        y += GAP;
        const int bx = (width() - BARW) / 2;
        const QRect track(bx, y, BARW, BARH);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 70));
        p.drawRoundedRect(track, BARH / 2.0, BARH / 2.0);
        const int fill = int(m_frac * BARW);
        if (fill > 0) {
            p.setBrush(Qt::white);
            p.drawRoundedRect(QRect(bx, y, fill, BARH), BARH / 2.0,
                              BARH / 2.0);
        }
    }
}
