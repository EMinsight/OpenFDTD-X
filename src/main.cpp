// main.cpp — OpenFDTD-X application entry
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

#include "MainWindow.h"
#include "I18n.h"
#include "Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("OpenFDTD");
    QApplication::setApplicationName("OpenFDTD-X");
    QApplication::setApplicationVersion("1.0.0");

    // Force Fusion style on all platforms so the look matches the mock
    // (Windows Vista / macOS native styles diverge too much).
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QCommandLineParser cli;
    cli.setApplicationDescription("OpenFDTD-X — Multi-Domain FDTD GUI");
    cli.addHelpOption();
    cli.addVersionOption();
    cli.addPositionalArgument("file", "Open a .ofd project file", "[file]");
    QCommandLineOption langOpt({ "l", "lang" }, "UI language (ja|en|both)", "lang");
    cli.addOption(langOpt);
    QCommandLineOption domainOpt({ "d", "domain" },
        "Start in domain (em|optical|acoustic|underwater)", "domain");
    cli.addOption(domainOpt);
    QCommandLineOption shotOpt("screenshot",
        "Save a window screenshot to <path> and exit (for CI)", "path");
    cli.addOption(shotOpt);
    QCommandLineOption styleOpt("ui-style",
        "UI style (classic|modern|scientific)", "style");
    cli.addOption(styleOpt);
    QCommandLineOption themeOpt("ui-theme", "UI theme (light|dark)", "theme");
    cli.addOption(themeOpt);
    QCommandLineOption densOpt("ui-density",
        "UI density (compact|normal|comfortable)", "density");
    cli.addOption(densOpt);
    QCommandLineOption viewOpt("view-style",
        "3D view style (wire|solid|field|rays)", "style");
    cli.addOption(viewOpt);
    QCommandLineOption tabOpt("left-tab",
        "Select the left tab whose title contains <text> (for CI)", "text");
    cli.addOption(tabOpt);
    cli.process(app);

    // i18n: CLI option > saved setting > ja
    const QString lang = cli.isSet(langOpt)
        ? cli.value(langOpt)
        : QSettings().value("ui/language", "ja").toString();
    ofd::I18n::instance().setLanguage(lang);

    // モックの CSS 変数テーマ (スタイル×テーマ×密度) を QSS へ生成して適用。
    // 静的な resources/styles/openfdtd.qss は Theme に置き換わった。
    {
        QSettings st;
        const auto uiStyle = ofd::Theme::styleFromKey(
            cli.isSet(styleOpt) ? cli.value(styleOpt)
                                : st.value("ui/style", "classic").toString());
        const auto uiTheme = ofd::Theme::themeFromKey(
            cli.isSet(themeOpt) ? cli.value(themeOpt)
                                : st.value("ui/theme", "light").toString());
        const auto uiDens = ofd::Theme::densityFromKey(
            cli.isSet(densOpt) ? cli.value(densOpt)
                               : st.value("ui/density", "normal").toString());
        if (cli.isSet(styleOpt)) st.setValue("ui/style", ofd::Theme::styleKey(uiStyle));
        if (cli.isSet(themeOpt)) st.setValue("ui/theme", ofd::Theme::themeKey(uiTheme));
        if (cli.isSet(densOpt))  st.setValue("ui/density", ofd::Theme::densityKey(uiDens));
        const ofd::Domain d0 = cli.isSet(domainOpt)
            ? ofd::domainFromKey(cli.value(domainOpt)) : ofd::Domain::EM;
        app.setStyleSheet(ofd::Theme::qss(uiStyle, uiTheme, uiDens, d0));
    }

    ofd::MainWindow w;
    w.show();

    const QStringList args = cli.positionalArguments();
    if (!args.isEmpty())
        w.openProject(args.first());

    if (cli.isSet(domainOpt))
        w.setDomain(ofd::domainFromKey(cli.value(domainOpt)));
    if (cli.isSet(tabOpt))
        w.selectLeftTab(cli.value(tabOpt));
    if (cli.isSet(viewOpt)) {
        const QString v = cli.value(viewOpt);
        const int idx = (v == "wire") ? 0 : (v == "field") ? 2
                      : (v == "rays") ? 3 : 1;   // 既定 solid
        w.setViewStyle(idx);
    }

    if (cli.isSet(shotOpt)) {
        const QString path = cli.value(shotOpt);
        QTimer::singleShot(800, &w, [&w, path] {
            w.grab().save(path);
            QApplication::quit();
        });
    }

    return app.exec();
}
