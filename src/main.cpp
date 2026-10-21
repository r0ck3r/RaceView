#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QDir>
#include <QCoreApplication>
#include <clocale>
#include <cstdlib>
#if !defined(Q_OS_WIN)
#include <langinfo.h>
#endif

#include "SyncController.h"
#include "ControlPanel.h"
#include "Mpris.h"

// Qt6 decodes file-system names with the C-locale codec. If the host locale is
// not UTF-8 (common when an AppImage is launched outside a UTF-8 terminal),
// non-ASCII names turn into mojibake. Force a UTF-8 codeset while keeping the
// user's language, so QLocale::system() still detects the right UI language.
static void ensureUtf8Locale() {
#if defined(Q_OS_WIN)
    // Windows uses UTF-16 file APIs, so the C-locale codec issue does not apply.
#else
    setlocale(LC_ALL, "");
    const char *cs = nl_langinfo(CODESET);
    if (cs && qstricmp(cs, "UTF-8") == 0)
        return;

    QByteArray base = qgetenv("LC_ALL");
    if (base.isEmpty())
        base = qgetenv("LANG");
    if (base.isEmpty())
        base = QByteArray("C");
    const int at = base.indexOf('@');
    if (at >= 0)
        base = base.left(at);
    const int dot = base.indexOf('.');
    if (dot >= 0)
        base = base.left(dot);

    QByteArray target = base + ".UTF-8";
    if (!setlocale(LC_ALL, target.constData())) {
        target = "C.UTF-8";
        if (!setlocale(LC_ALL, target.constData()))
            target = "C";
    }
    setenv("LC_ALL", target.constData(), 1);
    setlocale(LC_ALL, "");
#endif
}

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
    ensureUtf8Locale();
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
