#pragma once

#include <QByteArray>
#include <QBuffer>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QSize>
#include <Qt>

namespace icons {

inline const char *volOn() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
           "<path fill='#202020' d='M3 9v6h4l5 5V4L7 9H3z'/>"
           "<path fill='none' stroke='#202020' stroke-width='2' stroke-linecap='round' "
           "d='M16 8.5a4 4 0 0 1 0 7M18.5 6a7 7 0 0 1 0 12'/></svg>";
}

inline const char *volOff() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
           "<path fill='#8a8a8a' d='M3 9v6h4l5 5V4L7 9H3z'/>"
           "<path fill='none' stroke='#8a8a8a' stroke-width='2' stroke-linecap='round' "
           "d='M16 9l6 6M22 9l-6 6'/></svg>";
}

inline const char *play() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
           "<path fill='#202020' d='M8 5v14l11-7z'/></svg>";
}

inline const char *pause() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
           "<path fill='#202020' d='M7 5h4v14H7zM13 5h4v14h-4z'/></svg>";
}

inline const char *audio() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
           "<path fill='#202020' d='M12 3v10.55A4 4 0 1 0 14 17V7h4V3h-6z'/></svg>";
}

inline const char *fullOn() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
           "<path fill='none' stroke='#202020' stroke-width='2' stroke-linecap='round' "
           "stroke-linejoin='round' d='M4 9V4h5M15 4h5v5M20 15v5h-5M9 20H4v-5'/></svg>";
}

inline const char *fullOff() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
           "<path fill='none' stroke='#202020' stroke-width='2' stroke-linecap='round' "
           "stroke-linejoin='round' d='M9 4v5H4M20 9h-5V4M15 20v-5h5M4 15h5v5'/></svg>";
}

inline QIcon svg(const char *svg, int size) {
    QByteArray data(svg);
    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf, "svg");
    reader.setBackgroundColor(Qt::transparent);
    QImage img = reader.read();
    if (img.isNull())
        return QIcon();
    img = img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QIcon(QPixmap::fromImage(img));
}

} // namespace icons
