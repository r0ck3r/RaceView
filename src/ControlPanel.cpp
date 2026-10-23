#include "ControlPanel.h"
#include "SyncController.h"
#include "PlayerWindow.h"
#include "Icons.h"
#include "SeekSlider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QComboBox>
#include <QProcess>
#include <QCoreApplication>
#include <QLocale>
#include <QImage>
#include <QTimer>
#include <QApplication>
#include <QCloseEvent>
#include <QUrl>
#include <QtMath>

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

double sliderToSpeed(int v) { return std::pow(2.0, (v - 50) / 25.0); }

int speedToSlider(double s) {
    return qBound(0, int(50 + 25 * std::log2(s) + 0.5), 100);
}

const QStringList &videoFilters() {
    static const QStringList f = {"*.mkv", "*.mp4",  "*.avi", "*.ts",
                                  "*.mpg", "*.mov", "*.m2ts"};
    return f;
}
} // namespace

ControlPanel::ControlPanel(SyncController *sync, QWidget *parent)
    : QWidget(parent), m_sync(sync) {
    auto *root = new QVBoxLayout(this);

    auto *filesGroup = new QGroupBox(tr("Video source"));
    auto *filesOuter = new QVBoxLayout(filesGroup);
    m_rowsLayout = new QVBoxLayout;
    filesOuter->addLayout(m_rowsLayout);

    auto *btnRow = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("+ Add track"));
    connect(addBtn, &QPushButton::clicked, this, [this] { addPathRow(); });
    auto *folderBtn = new QPushButton(tr("Choose folder (autofill)"));
    connect(folderBtn, &QPushButton::clicked, this, &ControlPanel::browseFolder);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(folderBtn);
    btnRow->addStretch();
    filesOuter->addLayout(btnRow);
    root->addWidget(filesGroup);

    for (int i = 0; i < 3; ++i)
        addPathRow();

    auto *filesRow = new QHBoxLayout;
    m_startBtn = new QPushButton(tr("Start"));
    m_startBtn->setDefault(true);
    connect(m_startBtn, &QPushButton::clicked, this, &ControlPanel::startOrStop);
    filesRow->addStretch();
    filesRow->addWidget(m_startBtn);
    root->addLayout(filesRow);

    m_controls = new QWidget;
    auto *controlsLayout = new QVBoxLayout(m_controls);

    auto *transportRow = new QHBoxLayout;
    m_playBtn = new QPushButton;
    m_playBtn->setToolTip(tr("Play/Pause"));
    m_playBtn->setIcon(icons::svg(icons::play(), 20));
    m_playBtn->setIconSize(QSize(20, 20));
    connect(m_playBtn, &QPushButton::clicked, this, &ControlPanel::togglePlay);
    transportRow->addWidget(m_playBtn);
    m_seek = new SeekSlider(Qt::Horizontal);
    m_seek->setRange(0, 0);
    connect(m_seek, &QSlider::sliderPressed, this,
            [this] { m_dragging = true; });
    connect(m_seek, &QSlider::sliderMoved, this,
            [this](int v) { m_sync->previewSeek(v / 1000.0); });
    connect(m_seek, &QSlider::sliderReleased, this, [this] {
        m_sync->seekTo(m_seek->value() / 1000.0);
        m_dragging = false;
    });
    transportRow->addWidget(m_seek, 1);
    m_timeLabel = new QLabel("00:00:00 / 00:00:00");
    transportRow->addWidget(m_timeLabel);
    m_fullAllBtn = new QPushButton(tr("All windows"));
    m_fullAllBtn->setIcon(icons::svg(icons::fullOn(), 20));
    m_fullAllBtn->setIconSize(QSize(20, 20));
    m_fullAllBtn->setToolTip(tr("Fullscreen for all video windows"));
    connect(m_fullAllBtn, &QPushButton::clicked, this,
            &ControlPanel::toggleAllFullscreen);
    transportRow->addWidget(m_fullAllBtn);
    controlsLayout->addLayout(transportRow);

    auto *speedRow = new QHBoxLayout;
    speedRow->addWidget(new QLabel(tr("Speed")));
    m_speed = new QSlider(Qt::Horizontal);
    m_speed->setRange(0, 100);
    m_speed->setValue(50);
    connect(m_speed, &QSlider::valueChanged, this, [this] { applySpeed(); });
    speedRow->addWidget(m_speed, 1);
    m_speedLabel = new QLabel("1.00x");
    speedRow->addWidget(m_speedLabel);
    controlsLayout->addLayout(speedRow);

    m_controls->setEnabled(false);
    root->addWidget(m_controls);

    auto *aboutRow = new QHBoxLayout;
    auto *aboutBtn = new QPushButton(tr("About"));
    connect(aboutBtn, &QPushButton::clicked, this, &ControlPanel::showAbout);
    aboutRow->addWidget(aboutBtn);
    aboutRow->addStretch();

    aboutRow->addWidget(new QLabel(tr("Decoder")));
    m_hwdecCombo = new QComboBox;
    // "" = built-in copy-only list (see MpvPlayer::defaultHwdec).
    m_hwdecCombo->addItem(tr("Auto (recommended)"), QString());
    m_hwdecCombo->addItem(QStringLiteral("VA-API"), QStringLiteral("vaapi-copy"));
    m_hwdecCombo->addItem(QStringLiteral("VDPAU"), QStringLiteral("vdpau-copy"));
    m_hwdecCombo->addItem(QStringLiteral("NVDEC"), QStringLiteral("nvdec-copy"));
    m_hwdecCombo->addItem(QStringLiteral("CUDA"), QStringLiteral("cuda-copy"));
    m_hwdecCombo->addItem(QStringLiteral("Vulkan"), QStringLiteral("vulkan-copy"));
#if defined(Q_OS_WIN)
    m_hwdecCombo->addItem(QStringLiteral("D3D11VA"),
                          QStringLiteral("d3d11va-copy"));
    m_hwdecCombo->addItem(QStringLiteral("DXVA2"), QStringLiteral("dxva2-copy"));
#endif
    m_hwdecCombo->addItem(tr("mpv auto"), QStringLiteral("auto-copy"));
    m_hwdecCombo->addItem(tr("Software"), QStringLiteral("no"));
    m_hwdecCombo->setToolTip(tr("Video decoder"));
    {
        QSettings st(QStringLiteral("raceview"), QStringLiteral("raceview"));
        const QString saved = st.value(QStringLiteral("hwdec")).toString();
        const int idx = m_hwdecCombo->findData(saved);
        m_hwdecCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    connect(m_hwdecCombo, &QComboBox::activated, this, [this](int) {
        const QString api = m_hwdecCombo->currentData().toString();
        QSettings st(QStringLiteral("raceview"), QStringLiteral("raceview"));
        st.setValue(QStringLiteral("hwdec"), api);
        // "" means the built-in default; defaultHwdec() now reads it back.
        m_sync->setHwdec(api.isEmpty() ? MpvPlayer::defaultHwdec() : api);
    });
    aboutRow->addWidget(m_hwdecCombo);

    aboutRow->addWidget(new QLabel(tr("Language")));
    m_langCombo = new QComboBox;
    m_langCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_langCombo->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    m_langCombo->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
    m_langCombo->addItem(QStringLiteral("Español"), QStringLiteral("es"));
    m_langCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    {
        QSettings st(QStringLiteral("raceview"), QStringLiteral("raceview"));
        QString cur = st.value(QStringLiteral("language")).toString();
        if (cur.isEmpty())
            cur = QLocale::system().name().section(QLatin1Char('_'), 0, 0);
        const int idx = m_langCombo->findData(cur);
        const int en = m_langCombo->findData(QStringLiteral("en"));
        m_langCombo->setCurrentIndex(idx >= 0 ? idx : en);
    }
    connect(m_langCombo, &QComboBox::activated, this, [this](int) {
        const QString lang = m_langCombo->currentData().toString();
        QSettings st(QStringLiteral("raceview"), QStringLiteral("raceview"));
        if (st.value(QStringLiteral("language")).toString() == lang)
            return;
        st.setValue(QStringLiteral("language"), lang);
        restartApp();
    });
    aboutRow->addWidget(m_langCombo);
    root->addLayout(aboutRow);

    connect(m_sync, &SyncController::startedChanged, this,
            &ControlPanel::setStartedUi);

    m_uiTimer.setInterval(250);
    connect(&m_uiTimer, &QTimer::timeout, this, &ControlPanel::updateUi);

    setWindowTitle(tr("RaceView — control panel"));
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    resize(680, 100);
}

ControlPanel::~ControlPanel() { qDeleteAll(m_windows); }

QString ControlPanel::trackName(int index) {
    return index == 0 ? tr("Track 1 (master)")
                      : tr("Track %1").arg(index + 1);
}

void ControlPanel::addPathRow() {
    auto *row = new QHBoxLayout;
    auto *label = new QLabel;
    auto *edit = new QLineEdit;
    edit->setPlaceholderText(tr("Path to video"));
    auto *browseBtn = new QPushButton(tr("Browse..."));
    auto *removeBtn = new QPushButton(tr("✕"));
    connect(browseBtn, &QPushButton::clicked, this,
            [this, edit] { browse(edit); });
    connect(removeBtn, &QPushButton::clicked, this, [this, edit] {
        for (int i = 0; i < m_rows.size(); ++i)
            if (m_rows[i].edit == edit) {
                removeRow(i);
                break;
            }
    });
    row->addWidget(label);
    row->addWidget(edit, 1);
    row->addWidget(browseBtn);
    row->addWidget(removeBtn);
    m_rowsLayout->addLayout(row);
    m_rows.push_back({row, label, edit, browseBtn, removeBtn});
    refreshRows();
}

void ControlPanel::removeRow(int index) {
    if (m_rows.size() <= 1 || index < 0 || index >= m_rows.size())
        return;
    FileRow &r = m_rows[index];
    m_rowsLayout->removeItem(r.layout);
    delete r.label;
    delete r.edit;
    delete r.browse;
    delete r.remove;
    delete r.layout;
    m_rows.removeAt(index);
    refreshRows();
}

void ControlPanel::refreshRows() {
    for (int i = 0; i < m_rows.size(); ++i) {
        m_rows[i].label->setText(trackName(i));
        m_rows[i].remove->setEnabled(m_rows.size() > 1);
    }
}

void ControlPanel::setPaths(const QStringList &paths) {
    while (m_rows.size() < paths.size())
        addPathRow();
    for (int i = 0; i < m_rows.size() && i < paths.size(); ++i)
        m_rows[i].edit->setText(paths[i]);
}

void ControlPanel::browse(QLineEdit *target) {
    QSettings st(QStringLiteral("raceview"), QStringLiteral("raceview"));
    const QString startDir = st.value(QStringLiteral("lastDir")).toString();
    QString file = QFileDialog::getOpenFileName(
        this, tr("Select a video file"), startDir,
        tr("Video (*.mkv *.mp4 *.avi *.ts *.mpg *.mov *.m2ts);;All files (*)"));
    if (!file.isEmpty()) {
        target->setText(file);
        st.setValue(QStringLiteral("lastDir"), QFileInfo(file).absolutePath());
    }
}

void ControlPanel::browseFolder() {
    QSettings st(QStringLiteral("raceview"), QStringLiteral("raceview"));
    const QString startDir = st.value(QStringLiteral("lastDir")).toString();
    QString dir =
        QFileDialog::getExistingDirectory(this, tr("Video folder"), startDir);
    if (dir.isEmpty())
        return;
    st.setValue(QStringLiteral("lastDir"), dir);

    QDir d(dir);
    const auto entries = d.entryList(videoFilters(), QDir::Files, QDir::Name);

    QStringList mains, aux;
    for (const QString &name : entries) {
        const QString full = d.absoluteFilePath(name);
        const QString low = name.toLower();
        if (low.contains("datachannel") || low.contains("data_channel") ||
            low.contains("driver") || low.contains("tracker"))
            aux << full;
        else
            mains << full;
    }

    QStringList ordered = mains;
    ordered << aux;
    if (ordered.isEmpty())
        return;
    setPaths(ordered);
}

void ControlPanel::ensureWindows(int count) {
    while (m_windows.size() < count) {
        int i = m_windows.size();
        auto *w = new PlayerWindow(i, m_sync->player(i), trackName(i), m_sync);
        connect(w, &PlayerWindow::closed, this, &ControlPanel::onWindowClosed);
        m_windows.append(w);
    }
}

void ControlPanel::startOrStop() {
    if (m_sync->isStarted()) {
        m_sync->stopAll();
        return;
    }

    QStringList paths;
    for (const FileRow &r : m_rows)
        paths << r.edit->text().trimmed();

    if (paths.isEmpty() || paths[0].isEmpty() || !QFileInfo::exists(paths[0])) {
        m_startBtn->setText(tr("A master track file is required!"));
        return;
    }
    for (int i = 1; i < paths.size(); ++i)
        if (!paths[i].isEmpty() && !QFileInfo::exists(paths[i]))
            paths[i].clear();

    const int n = paths.size();
    if (!m_sync->preparePlayers(n)) {
        QMessageBox::critical(this, tr("RaceView"),
                              tr("Failed to initialize libmpv"));
        return;
    }

    // mpv with vo=libmpv needs the render context BEFORE loadfile, otherwise
    // the video output never comes up ("Video: no video"): windows+GL first, then load.
    ensureWindows(n);
    for (int i = 0; i < n; ++i) {
        if (!paths[i].isEmpty()) {
            m_windows[i]->setFilePath(paths[i]);
            m_windows[i]->ensureRenderReady();
        } else {
            m_windows[i]->setVisible(false);
        }
    }
    for (int i = n; i < m_windows.size(); ++i)
        m_windows[i]->setVisible(false);

    if (!m_sync->start(paths)) {
        QMessageBox::critical(this, tr("RaceView"),
                              tr("Failed to start playback"));
        return;
    }

    if (qEnvironmentVariableIsSet("RACEVIEW_DUMP")) {
        const QString path = qEnvironmentVariable("RACEVIEW_DUMP");
        QTimer::singleShot(3000, this, [this, path] {
            if (!m_windows.isEmpty())
                m_windows[0]->grabVideo().save(path);
        });
    }
}

void ControlPanel::togglePlay() {
    m_sync->togglePlay();
    updateUi();
}

void ControlPanel::showAbout() {
    QString html = tr("<h3>RaceView</h3>"
                      "<p>Multi-track synchronized video player built with Qt6 + libmpv.</p>"
                      "<p><b>Author:</b> r0ck3r</p>"
                      "<p><b>Site:</b> <a href=\"https://github.com/r0ck3r/RaceView\">"
                      "https://github.com/r0ck3r/RaceView</a></p>");
    const QString ver = QCoreApplication::applicationVersion();
    if (!ver.isEmpty())
        html += tr("<p><b>Version:</b> %1</p>").arg(ver);
    QMessageBox::about(this, tr("About"), html);
}

void ControlPanel::applySpeed() {
    double s = sliderToSpeed(m_speed->value());
    m_sync->setSpeed(s);
    m_speedLabel->setText(QString::number(s, 'f', 2) + "x");
}

void ControlPanel::setStartedUi(bool started) {
    m_controls->setEnabled(started);
    m_startBtn->setText(started ? tr("Stop") : tr("Start"));
    if (started) {
        m_uiTimer.start();
    } else {
        m_uiTimer.stop();
        m_allFullscreen = false;
        if (m_fullAllBtn)
            m_fullAllBtn->setIcon(icons::svg(icons::fullOn(), 20));
        for (auto *w : m_windows)
            w->hide();
    }
}

void ControlPanel::onWindowClosed() {
    if (m_quitting)
        return;
    // The master track defines the session: closing its window closes all.
    if (!m_windows.isEmpty() && sender() == m_windows.first()) {
        quitApp();
        return;
    }
    for (PlayerWindow *w : m_windows)
        if (w->isVisible())
            return;
    quitApp();
}

void ControlPanel::closeEvent(QCloseEvent *ev) {
    if (!m_quitting)
        quitApp();
    ev->accept();
}

void ControlPanel::quitApp() {
    if (m_quitting)
        return;
    m_quitting = true;
    if (m_sync->isStarted())
        m_sync->stopAll();
    qApp->quit();
}

void ControlPanel::restartApp() {
    if (m_quitting)
        return;
    m_quitting = true;
    if (m_sync->isStarted())
        m_sync->stopAll();

    QStringList paths;
    for (const FileRow &r : m_rows) {
        const QString p = r.edit->text().trimmed();
        if (!p.isEmpty())
            paths << p;
    }
    QProcess::startDetached(QCoreApplication::applicationFilePath(), paths);
    qApp->quit();
}

void ControlPanel::raiseWindows() {
    show();
    raise();
    activateWindow();
    for (PlayerWindow *w : m_windows)
        if (w->isVisible()) {
            w->raise();
            w->activateWindow();
        }
}

void ControlPanel::requestQuit() { quitApp(); }

void ControlPanel::openUri(const QString &uri) {
    QString path = uri;
    if (path.startsWith(QStringLiteral("file://")))
        path = QUrl(uri).toLocalFile();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return;
    setPaths({path});
    startOrStop();
}

void ControlPanel::toggleAllFullscreen() {
    m_allFullscreen = !m_allFullscreen;
    for (PlayerWindow *w : m_windows)
        if (w->isVisible())
            w->setFullscreen(m_allFullscreen);
    m_fullAllBtn->setIcon(
        icons::svg(m_allFullscreen ? icons::fullOff() : icons::fullOn(), 20));
}

void ControlPanel::updateUi() {
    if (!m_sync->isStarted())
        return;

    const double t = m_sync->masterTime();
    const double d = m_sync->masterDuration();
    if (d > 0) {
        m_seek->setRange(0, int(d * 1000));
        if (!m_dragging)
            m_seek->setValue(int(t * 1000));
    }
    m_timeLabel->setText(fmtTime(t) + " / " + fmtTime(d));
    m_playBtn->setIcon(icons::svg(m_sync->isPlayingIntent() ? icons::pause()
                                                            : icons::play(),
                                  20));
    m_speed->blockSignals(true);
    m_speed->setValue(speedToSlider(m_sync->speed()));
    m_speed->blockSignals(false);
    m_speedLabel->setText(QString::number(m_sync->speed(), 'f', 2) + "x");
    const QString used = m_sync->hwdecCurrent();
    const QString tip = used.isEmpty() ? tr("Video decoder")
                                       : tr("Active decoder: %1").arg(used);
    if (m_hwdecCombo->toolTip() != tip)
        m_hwdecCombo->setToolTip(tip);
}
