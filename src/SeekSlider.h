#pragma once

#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QMouseEvent>

class SeekSlider : public QSlider {
public:
    using QSlider::QSlider;

protected:
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && maximum() > minimum()) {
            setSliderDown(true);
            emit sliderPressed();
            setSliderPosition(posToValue(e->position().toPoint()));
            emit sliderMoved(sliderPosition());
            e->accept();
            return;
        }
        QSlider::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        if (isSliderDown() && maximum() > minimum()) {
            setSliderPosition(posToValue(e->position().toPoint()));
            emit sliderMoved(sliderPosition());
            e->accept();
            return;
        }
        QSlider::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent *e) override {
        if (isSliderDown() && maximum() > minimum()) {
            setSliderPosition(posToValue(e->position().toPoint()));
            setSliderDown(false);
            emit sliderReleased();
            e->accept();
            return;
        }
        QSlider::mouseReleaseEvent(e);
    }

private:
    int posToValue(const QPoint &p) const {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect groove = style()->subControlRect(
            QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(
            QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        const int range = maximum() - minimum();
        if (range <= 0)
            return minimum();
        if (orientation() == Qt::Horizontal) {
            const int avail = qMax(1, groove.width() - handle.width());
            const int x =
                qBound(0, p.x() - groove.left() - handle.width() / 2, avail);
            return QStyle::sliderValueFromPosition(minimum(), maximum(), x,
                                                   avail, false);
        }
        const int avail = qMax(1, groove.height() - handle.height());
        const int y =
            qBound(0, groove.bottom() - p.y() - handle.height() / 2, avail);
        return QStyle::sliderValueFromPosition(minimum(), maximum(), y, avail,
                                               true);
    }
};
