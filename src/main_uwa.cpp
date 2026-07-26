// main_uwa.cpp — OpenUWA (水中音響 分離アプリ) のエントリ。
// 元 OpenUWA-Underwater.html / underwater-app.jsx。
//
// 本体 (openfdtd_x) と同じ CLI フックを持たせ、CI から同様に検証できるように
// している (--lang / --screenshot / --left-tab / --ui-theme / --ui-density)。
#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

#include "UnderwaterWindow.h"
#include "I18n.h"
#include "Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("OpenFDTD");
    QApplication::setApplicationName("OpenUWA");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QCommandLineParser cli;
    cli.setApplicationDescription("OpenUWA — Underwater Acoustics (OpenFDTD-X derivative)");
    cli.addHelpOption();
    cli.addVersionOption();
    cli.addPositionalArgument("file", "Open a .ofd/.ofdx project file", "[file]");
    QCommandLineOption langOpt({ "l", "lang" }, "UI language (ja|en|both)", "lang");
    cli.addOption(langOpt);
    QCommandLineOption shotOpt("screenshot",
        "Save a window screenshot to <path> and exit (for CI)", "path");
    cli.addOption(shotOpt);
    QCommandLineOption tabOpt("left-tab",
        "Select the tab whose title contains <text> (for CI)", "text");
    cli.addOption(tabOpt);
    QCommandLineOption themeOpt("ui-theme", "UI theme (light|dark)", "theme");
    cli.addOption(themeOpt);
    QCommandLineOption densOpt("ui-density",
        "UI density (compact|normal|comfortable)", "density");
    cli.addOption(densOpt);
    cli.process(app);

    const QString lang = cli.isSet(langOpt)
        ? cli.value(langOpt)
        : QSettings().value("ui/language", "ja").toString();
    ofd::I18n::instance().setLanguage(lang);

    // モックの useTweaks 既定は ダーク + Comfortable。--ui-* はセッション限りの
    // 上書きで QSettings は書き換えない (本体と同じ扱い)。
    QSettings st;
    const ofd::UiTheme uiTheme = ofd::Theme::themeFromKey(
        cli.isSet(themeOpt) ? cli.value(themeOpt)
                            : st.value("ui/theme", "dark").toString());
    const ofd::Density uiDens = ofd::Theme::densityFromKey(
        cli.isSet(densOpt) ? cli.value(densOpt)
                           : st.value("ui/density", "comfortable").toString());
    // モックは data-style="fusion" 固定 (= Classic)
    app.setStyleSheet(ofd::Theme::qss(ofd::UiStyle::Classic, uiTheme, uiDens,
                                      ofd::Domain::Underwater));

    ofd::UnderwaterWindow w;
    w.show();

    const QStringList args = cli.positionalArguments();
    if (!args.isEmpty())
        w.openProject(args.first());
    if (cli.isSet(tabOpt))
        w.selectTab(cli.value(tabOpt));

    if (cli.isSet(shotOpt)) {
        const QString path = cli.value(shotOpt);
        QTimer::singleShot(800, &w, [&w, path] {
            w.grab().save(path);
            QApplication::quit();
        });
    }

    return app.exec();
}
