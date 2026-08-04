// main.cpp — OpenFDTD-X application entry
#include <QApplication>
#include <QFont>
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

    // アプリ既定フォントを実在ファミリへ固定する (--ff-ui 相当)。
    // QPA の既定は "Sans Serif" のような非実在名のことがあり、そのままだと
    // Qt のフォント別名解決で起動ごとに 60〜100 ms を失う。
    // スタイル適用直後・ウィンドウ生成前に置くこと (最初の解決より先に効かせる)。
    if (const QString ff = ofd::Theme::uiFontFamily(); !ff.isEmpty()) {
        QFont appFont = QApplication::font();
        appFont.setFamily(ff);
        QApplication::setFont(appFont);
    }

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

    // テーマ (スタイル×テーマ×密度) を解決して QSS を生成・適用する。
    // ウィンドウ生成より先に貼ることで最初の描画から正しい配色になる。
    // 静的な resources/styles/openfdtd.qss は Theme に置き換わった。
    // --ui-* はセッション限りの上書きで、QSettings は書き換えない (--lang と同様)。
    const bool themeOverridden =
        cli.isSet(styleOpt) || cli.isSet(themeOpt) || cli.isSet(densOpt);
    ofd::UiStyle uiStyle;
    ofd::UiTheme uiTheme;
    ofd::Density uiDens;
    {
        QSettings st;
        uiStyle = ofd::Theme::styleFromKey(cli.isSet(styleOpt) ? cli.value(styleOpt)
            : st.value("ui/style", "classic").toString());
        uiTheme = ofd::Theme::themeFromKey(cli.isSet(themeOpt) ? cli.value(themeOpt)
            : st.value("ui/theme", "light").toString());
        uiDens  = ofd::Theme::densityFromKey(cli.isSet(densOpt) ? cli.value(densOpt)
            : st.value("ui/density", "normal").toString());
    }
    const ofd::Domain startDomain = cli.isSet(domainOpt)
        ? ofd::domainFromKey(cli.value(domainOpt)) : ofd::Domain::EM;
    // QSS + パレット + カラースキームをまとめて適用する。QSS だけだと
    // スタイルが自前で描く部分が OS の外観 (macOS のライト/ダーク) に従い、
    // 暗色テーマに白い枠が混ざる。
    ofd::Theme::apply(uiStyle, uiTheme, uiDens, startDomain);

    ofd::MainWindow w;
    if (themeOverridden) w.setThemeOverride(uiStyle, uiTheme, uiDens);
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
