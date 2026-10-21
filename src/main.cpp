#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QDir>
#include <QCoreApplication>
#include <clocale>

#include "SyncController.h"
#include "ControlPanel.h"
#include "Mpris.h"

static QString resolveLanguage() {
    QSettings st(QStringLiteral("raceview"), QStringLiteral("raceview"));
    QString lang = st.value(QStringLiteral("language")).toString();
    if (lang.isEmpty()) {
        const QString sys = QLocale::system().name(); // e.g. ru_RU, en_US
        lang = sys.section(QLatin1Char('_'), 0, 0);
    }
    return lang;
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("raceview");
    app.setApplicationName("RaceView");
    app.setApplicationDisplayName("RaceView");
    app.setApplicationVersion(QStringLiteral(RACEVIEW_VERSION));
    app.setWindowIcon(QIcon(":/raceview.svg"));

    // Qt вызывает setlocale(LC_ALL, "") в конструкторе, а libmpv требует C
    setlocale(LC_NUMERIC, "C");

    const QString lang = resolveLanguage();
    static QTranslator translator;
    if (!lang.isEmpty() && lang != QStringLiteral("en")) {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList dirs = {
            appDir,
            appDir + QStringLiteral("/translations"),
            appDir + QStringLiteral("/../share/raceview/translations"),
        };
        for (const QString &d : dirs) {
            if (translator.load(QStringLiteral("raceview_") + lang,
                                QDir(d).absolutePath())) {
                app.installTranslator(&translator);
                break;
            }
        }
    }

    SyncController sync;
    ControlPanel panel(&sync);

    // Единая MPRIS2-регистрация на процесс (глобальные медиа-клавиши).
    Mpris mpris(&sync);
    QObject::connect(&mpris, &Mpris::raiseRequested, &panel,
                     &ControlPanel::raiseWindows);
    QObject::connect(&mpris, &Mpris::quitRequested, &panel,
                     &ControlPanel::requestQuit);
    QObject::connect(&mpris, &Mpris::openUriRequested, &panel,
                     &ControlPanel::openUri);
    mpris.registerService();

    const QStringList args = app.arguments();
    if (args.size() >= 2) {
        panel.setPaths(args.mid(1));
        panel.startOrStop();
    }

    panel.show();
    return app.exec();
}
