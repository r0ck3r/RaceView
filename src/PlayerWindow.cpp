#include "PlayerWindow.h"
#include "MpvPlayer.h"
#include "MpvWidget.h"
#include "SyncController.h"
#include "Icons.h"
#include "SeekSlider.h"
#include "OsdWidget.h"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QHideEvent>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QCursor>
#include <QEvent>
#include <QResizeEvent>
#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QPixmap>
#include <QIcon>
#include <QSize>

namespace {
QString fmtTime(double s) {
    if (s < 0)
        s = 0;
    int total = int(s);
    return QString("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg(total / 60 % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

PlayerWindow::PlayerWindow(int index, MpvPlayer *player, const QString &role,
                           SyncController *sync, QWidget *parent)
    : QMainWindow(parent), m_index(index), m_player(player), m_sync(sync),
      m_role(role) {
    m_container = new QWidget(this);
    m_containerLayout = new QVBoxLayout(m_container);
    m_containerLayout->setContentsMargins(0, 0, 0, 0);
    m_containerLayout->setSpacing(0);

    m_widget = new MpvWidget(player, m_container);
    m_widget->setFocusPolicy(Qt::StrongFocus);
    m_widget->setMouseTracking(true);
    m_containerLayout->addWidget(m_widget, 1);
    connect(m_widget, &MpvWidget::doubleClicked, this,
            &PlayerWindow::toggleFullscreen);

    buildControls(m_containerLayout);

    m_osd = new OsdWidget(m_container);
    m_osd->hide();

    m_osdTimer.setSingleShot(true);
    connect(&m_osdTimer, &QTimer::timeout, this,
            [this] {
                if (m_osd)
                    m_osd->hide();
                m_osdDelta.clear();
            });

    setCentralWidget(m_container);
    setWindowTitle(role);
    resize(1280, 720);

    m_container->setMouseTracking(true);
    m_container->installEventFilter(this);
    m_widget->installEventFilter(this);
    if (m_barWidget)
        m_barWidget->installEventFilter(this);

    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [this] {
        if (!m_fullscreen || !m_barWidget)
            return;
        const QPoint p = m_container->mapFromGlobal(QCursor::pos());
        if (m_barWidget->underMouse() || m_barWidget->geometry().contains(p)) {
            m_hideTimer.start(2500);
            return;
        }
        m_barWidget->hide();
    });

    connect(m_sync, &SyncController::startedChanged, this, [this](bool started) {
        if (m_playBtn)
            m_playBtn->setEnabled(started);
        if (m_seek)
            m_seek->setEnabled(started);
        if (m_mute)
            m_mute->setEnabled(started);
        if (m_volSlider) {
            m_volSlider->setEnabled(started);
            if (started)
                m_volSlider->setValue(int(m_sync->trackVolume(m_index)));
        }
        if (m_offset)
            m_offset->setEnabled(started);
        if (started) {
            m_audioTries = 0;
            m_timer.start(250);
        } else {
            m_timer.stop();
            m_audioLabels.clear();
            m_audioIds.clear();
            m_audioSel = -1;
            if (m_audioBtn) {
                m_audioBtn->setEnabled(false);
                m_audioBtn->setText(QString());
                m_audioBtn->setToolTip(QString());
            }
        }
    });
    connect(&m_timer, &QTimer::timeout, this, &PlayerWindow::updateBar);
    connect(&m_holdTimer, &QTimer::timeout, this, &PlayerWindow::holdRepeat);

    connect(m_sync, &SyncController::seekPreview, this, [this](double t) {
        showOsd(t, t - m_sync->masterTime());
    });
    connect(m_sync, &SyncController::seeked, this,
            [this](double t) { showOsd(t, 0.0); });
    connect(m_sync, &SyncController::seekProgress, this,
            [this](double t, double delta) {
                showOsd(t, delta);
                if (m_seek && m_sync->seekHolding() && !m_dragging)
                    m_seek->setValue(int(t * 1000));
            });
}

void PlayerWindow::buildControls(QVBoxLayout *lay) {
    m_barWidget = new QWidget;
    m_barWidget->setObjectName("playerBar");
    auto *bar = new QHBoxLayout(m_barWidget);
    bar->setContentsMargins(8, 6, 8, 6);

    m_playBtn = new QPushButton;
    m_playBtn->setToolTip(tr("Play/Pause"));
    m_playBtn->setIcon(icons::svg(icons::play(), 20));
    m_playBtn->setIconSize(QSize(20, 20));
    m_playBtn->setEnabled(false);
    connect(m_playBtn, &QPushButton::clicked, this,
            [this] { m_sync->togglePlay(); });
    bar->addWidget(m_playBtn);

    m_fullBtn = new QPushButton;
    m_fullBtn->setToolTip(tr("Fullscreen (F or double-click the video)"));
    m_fullBtn->setIcon(icons::svg(icons::fullOn(), 20));
    m_fullBtn->setIconSize(QSize(20, 20));
    connect(m_fullBtn, &QPushButton::clicked, this,
            &PlayerWindow::toggleFullscreen);
    bar->addWidget(m_fullBtn);

    m_seek = new SeekSlider(Qt::Horizontal);
    m_seek->setRange(0, 0);
    m_seek->setEnabled(false);
    connect(m_seek, &QSlider::sliderPressed, this, [this] { m_dragging = true; });
    connect(m_seek, &QSlider::sliderMoved, this,
            [this](int v) { m_sync->previewSeek(v / 1000.0); });
    connect(m_seek, &QSlider::sliderReleased, this, [this] {
        m_sync->seekTo(m_seek->value() / 1000.0);
        m_dragging = false;
    });
    bar->addWidget(m_seek, 1);

    m_timeLabel = new QLabel("00:00:00 / 00:00:00");
    bar->addWidget(m_timeLabel);

    m_audioBtn = new QPushButton;
    m_audioBtn->setIcon(icons::svg(icons::audio(), 18));
    m_audioBtn->setIconSize(QSize(18, 18));
    m_audioBtn->setEnabled(false);
    m_audioBtn->setToolTip(tr("Audio track"));
    connect(m_audioBtn, &QPushButton::clicked, this,
            &PlayerWindow::openAudioDialog);
    bar->addWidget(m_audioBtn);

    m_mute = new QPushButton;
    m_mute->setCheckable(true);
    m_mute->setChecked(true);
    m_mute->setToolTip(tr("Toggle sound"));
    m_mute->setIcon(icons::svg(icons::volOn(), 20));
    m_mute->setIconSize(QSize(20, 20));
    m_mute->setEnabled(false);
    connect(m_mute, &QPushButton::toggled, this, [this](bool on) {
        m_mute->setIcon(icons::svg(on ? icons::volOn() : icons::volOff(), 20));
        m_sync->setTrackMuted(m_index, !on);
    });
    bar->addWidget(m_mute);

    bar->addWidget(new QLabel(tr("Volume")));
    m_volSlider = new QSlider(Qt::Horizontal);
    m_volSlider->setRange(0, 130);
    m_volSlider->setValue(int(m_sync->trackVolume(m_index)));
    m_volSlider->setFixedWidth(90);
    m_volSlider->setToolTip(tr("Volume of this track"));
    m_volSlider->setEnabled(false);
    connect(m_volSlider, &QSlider::valueChanged, this, [this](int v) {
        m_sync->setTrackVolume(m_index, v);
        showVolumeOsd(v);
    });
    bar->addWidget(m_volSlider);

    if (m_index > 0) {
        bar->addWidget(new QLabel(tr("Offset")));
        m_offset = new QDoubleSpinBox;
        m_offset->setRange(-10.0, 10.0);
        m_offset->setSingleStep(0.1);
        m_offset->setDecimals(1);
        m_offset->setSuffix(" s");
        m_offset->setEnabled(false);
        connect(m_offset, &QDoubleSpinBox::valueChanged, this,
                [this](double v) { m_sync->setTrackOffset(m_index, v); });
        bar->addWidget(m_offset);
    }

    lay->addWidget(m_barWidget);
}

void PlayerWindow::setFilePath(const QString &path) {
    setWindowTitle(m_role + " — " + QFileInfo(path).fileName());
}

QImage PlayerWindow::grabVideo() {
    return m_widget ? m_widget->grabFramebuffer() : QImage();
}

void PlayerWindow::ensureRenderReady() {
    show();
    // mpv с vo=libmpv требует render context ДО загрузки файла, иначе
    // видеовывод не поднимается ("Video: no video"). Дожидаемся initializeGL.
    for (int i = 0; i < 100 && !m_widget->isRenderReady(); ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
}

void PlayerWindow::updateBar() {
    if (!m_sync->isStarted())
        return;

    if (m_playBtn)
        m_playBtn->setIcon(
            icons::svg(m_sync->isPlaying() ? icons::pause() : icons::play(), 20));

    double t = m_sync->masterTime();
    // While an arrow key is held the real position is frozen (we pause and do
    // not poll mpv), so drive the bar from the scrub target for live feedback.
    if (m_sync->seekHolding())
        t = m_sync->seekTarget();
    const double d = m_sync->masterDuration();
    if (m_seek && d > 0) {
        m_seek->setRange(0, int(d * 1000));
        if (!m_dragging)
            m_seek->setValue(int((t < 0 ? 0 : t) * 1000));
    }
    if (m_timeLabel)
        m_timeLabel->setText(fmtTime(t) + " / " + fmtTime(d));

    refreshAudio();
}

void PlayerWindow::holdRepeat() {
    if (m_sync->seekHolding())
        m_sync->nudgeSeek(m_holdDt);
}

void PlayerWindow::refreshAudio() {
    if (!m_audioBtn)
        return;
    if (!m_audioLabels.isEmpty())
        return; // already populated
    const auto tracks = m_sync->audioTracks(m_index);
    if (tracks.isEmpty()) {
        if (++m_audioTries > 20)
            return; // give up (file has no audio or load failed)
        return;
    }
    m_audioLabels.clear();
    m_audioIds.clear();
    m_audioSel = -1;
    for (int t = 0; t < int(tracks.size()); ++t) {
        const auto &at = tracks[t];
        QString text = QString::number(at.id);
        if (!at.lang.isEmpty())
            text += " [" + at.lang + "]";
        if (!at.title.isEmpty())
            text += " " + at.title;
        m_audioLabels << text;
        m_audioIds << at.id;
        if (at.selected)
            m_audioSel = t;
    }
    updateAudioButton();
}

void PlayerWindow::updateAudioButton() {
    if (!m_audioBtn)
        return;
    const bool multi = m_audioIds.size() > 1;
    m_audioBtn->setEnabled(multi && m_sync->isStarted());
    if (m_audioSel < 0 || m_audioSel >= m_audioLabels.size()) {
        m_audioBtn->setText(QString());
        m_audioBtn->setToolTip(tr("Audio track"));
        return;
    }
    const QString full = m_audioLabels[m_audioSel];
    // Compact label: prefer language code, then title, then id.
    const auto &id = m_audioIds[m_audioSel];
    QString shortText = QString::number(id);
    const int lb = full.indexOf('[');
    const int rb = full.indexOf(']');
    if (lb >= 0 && rb > lb)
        shortText = full.mid(lb + 1, rb - lb - 1);
    else if (rb >= 0 && !full.mid(rb + 1).trimmed().isEmpty())
        shortText = full.mid(rb + 1).trimmed();
    m_audioBtn->setText(shortText);
    m_audioBtn->setToolTip(tr("Audio track") + ": " + full);
}

void PlayerWindow::openAudioDialog() {
    if (m_audioIds.size() <= 1)
        return;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Audio track"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *list = new QListWidget(&dlg);
    for (int i = 0; i < m_audioLabels.size(); ++i)
        list->addItem(m_audioLabels[i]);
    if (m_audioSel >= 0)
        list->setCurrentRow(m_audioSel);
    lay->addWidget(list);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
    lay->addWidget(box);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const int row = list->currentRow();
    if (row < 0 || row >= m_audioIds.size())
        return;
    m_sync->setTrackAudio(m_index, m_audioIds[row]);
    m_audioSel = row;
    updateAudioButton();
}

void PlayerWindow::setFullscreen(bool on) {
    m_fullscreen = on;
    if (m_fullscreen) {
        showFullScreen();
        if (m_barWidget) {
            m_containerLayout->removeWidget(m_barWidget);
            m_barWidget->setStyleSheet(
                "#playerBar { background-color: rgba(20,20,20,210); }"
                "#playerBar QLabel { color: #f2f2f2; }"
                "#playerBar QDoubleSpinBox { color: #101010; background-color: #e8e8e8; }"
                "#playerBar QPushButton { color: #101010; background-color: #e0e0e0;"
                " border: 1px solid #888; border-radius: 3px; padding: 3px 8px; }");
            m_barWidget->setAutoFillBackground(true);
            positionBar();
            m_barWidget->raise();
            showControls();
        }
    } else {
        showNormal();
        m_hideTimer.stop();
        if (m_barWidget) {
            m_barWidget->setStyleSheet(QString());
            m_containerLayout->addWidget(m_barWidget);
            m_barWidget->show();
        }
    }
    if (m_fullBtn)
        m_fullBtn->setIcon(
            icons::svg(m_fullscreen ? icons::fullOff() : icons::fullOn(), 20));
}

void PlayerWindow::positionBar() {
    if (!m_barWidget || !m_container)
        return;
    const int h = m_barWidget->sizeHint().height();
    m_barWidget->setGeometry(0, m_container->height() - h,
                             m_container->width(), h);
}

void PlayerWindow::showOsd(double target, double delta) {
    if (!m_sync->isStarted() || !m_osd)
        return;
    QString sub;
    if (qAbs(delta) >= 0.05)
        sub = QString("%1%2")
                  .arg(delta < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                  .arg(fmtTime(qAbs(delta)));
    if (sub.isEmpty())
        sub = m_osdDelta; // keep the last offset until the whole OSD hides
    else
        m_osdDelta = sub;
    const double d = m_sync->masterDuration();
    const double frac = d > 0 ? target / d : 0.0;
    m_osd->showSeek(fmtTime(target), sub, frac);
    positionOsd();
    m_osd->show();
    m_osd->raise();
    m_osdTimer.start(1500);
}

void PlayerWindow::showVolumeOsd(int percent) {
    if (!m_sync->isStarted() || !m_osd)
        return;
    m_osdDelta.clear();
    m_osd->showVolume(tr("Volume %1%").arg(percent), percent / 130.0);
    positionOsd();
    m_osd->show();
    m_osd->raise();
    m_osdTimer.start(1500);
}

void PlayerWindow::positionOsd() {
    if (!m_osd || !m_container)
        return;
    const QSize s = m_osd->size();
    m_osd->setGeometry((m_container->width() - s.width()) / 2,
                       (m_container->height() - s.height()) / 2, s.width(),
                       s.height());
}

void PlayerWindow::showControls() {
    if (!m_fullscreen || !m_barWidget)
        return;
    positionBar();
    m_barWidget->show();
    m_barWidget->raise();
    m_hideTimer.start(2500);
}

void PlayerWindow::resizeEvent(QResizeEvent *ev) {
    QMainWindow::resizeEvent(ev);
    if (m_fullscreen)
        positionBar();
    if (m_osd && m_osd->isVisible())
        positionOsd();
}

bool PlayerWindow::eventFilter(QObject *obj, QEvent *ev) {
    if (m_fullscreen &&
        (ev->type() == QEvent::MouseMove ||
         ev->type() == QEvent::MouseButtonPress)) {
        showControls();
    }
    return QMainWindow::eventFilter(obj, ev);
}

void PlayerWindow::toggleFullscreen() {
    setFullscreen(!m_fullscreen);
}

void PlayerWindow::closeEvent(QCloseEvent *ev) {
    QMainWindow::closeEvent(ev);
    hide(); // скрыть ДО сигнала: иначе isVisible() ещё true при проверке
    emit closed();
}

void PlayerWindow::changeEvent(QEvent *ev) {
    QMainWindow::changeEvent(ev);
    if (ev->type() == QEvent::ActivationChange && !isActiveWindow())
        cancelSeekHold();
}

void PlayerWindow::hideEvent(QHideEvent *ev) {
    QMainWindow::hideEvent(ev);
    cancelSeekHold();
}

// The key-release may never arrive after focus loss (Alt+Tab, window hidden,
// closed): stop the repeat timer and commit the scrub immediately instead of
// waiting for the SyncController safety net.
void PlayerWindow::cancelSeekHold() {
    m_holdTimer.stop();
    m_sync->endSeekHold();
}

void PlayerWindow::keyPressEvent(QKeyEvent *ev) {
    switch (ev->key()) {
    case Qt::Key_Space:
        m_sync->togglePlay();
        break;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
        if (ev->isAutoRepeat()) {
            ev->ignore();
            return;
        }
        double dt = 0.0;
        switch (ev->key()) {
        case Qt::Key_Left:  dt = -10.0; break;
        case Qt::Key_Right: dt =  10.0; break;
        case Qt::Key_Up:    dt =  60.0; break;
        case Qt::Key_Down:  dt = -60.0; break;
        }
        m_holdDt = dt;
        m_sync->nudgeSeek(dt);
        m_holdTimer.start(120);
        break;
    }
    case Qt::Key_PageUp:
        if (m_volSlider && m_sync->isStarted())
            m_volSlider->setValue(m_volSlider->value() + 5);
        break;
    case Qt::Key_PageDown:
        if (m_volSlider && m_sync->isStarted())
            m_volSlider->setValue(m_volSlider->value() - 5);
        break;
    case Qt::Key_Comma:
        m_sync->frameStep(true);
        break;
    case Qt::Key_Period:
        m_sync->frameStep(false);
        break;
    case Qt::Key_BracketLeft:
        m_sync->changeSpeed(1.0 / 1.1);
        break;
    case Qt::Key_BracketRight:
        m_sync->changeSpeed(1.1);
        break;
    case Qt::Key_F:
        toggleFullscreen();
        break;
    case Qt::Key_Escape:
        if (m_fullscreen)
            toggleFullscreen();
        break;
    case Qt::Key_S:
        m_player->commandAsync({"screenshot"});
        break;
    default:
        QMainWindow::keyPressEvent(ev);
        return;
    }
}

void PlayerWindow::keyReleaseEvent(QKeyEvent *ev) {
    switch (ev->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        if (ev->isAutoRepeat()) {
            ev->ignore();
            return;
        }
        m_holdTimer.stop();
        m_sync->endSeekHold();
        break;
    default:
        break;
    }
    QMainWindow::keyReleaseEvent(ev);
}
