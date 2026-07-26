// Theme.cpp — Palette (CSS 変数) → QSS 変換の実装
//
// カスケード順序はモック styles.css と同じ:
//   :root (classic)  →  [data-style]  →  [data-theme="dark"]  →  [data-density]
//   →  [data-domain] (アクセント色のみ)
// dark は style より後に来るので、modern / scientific のどちらの上でも
// 配色を上書きする (scientific 自体が暗色なので dark 適用後も破綻しない)。
#include "Theme.h"

#include <QColor>

namespace ofd {

namespace {

// ── Palette ─────────────────────────────────────────────────────────────────
// styles.css の CSS 変数と 1:1 対応する内部表現。
struct Palette {
    // 背景 (--bg-*)
    QString bgApp, bgPanel, bgInput, bgTab, bgTabActive, bgTitlebar,
            bgMenubar, bgStatusbar, bgViewport, bgButton, bgButtonHover,
            bgButtonPressed, bgSection, bgRowAlt, bgHover, bgSelected;
    // 前景 (--fg-*)
    QString fgApp, fgMuted, fgDim, fgTitlebar, fgSection;
    // 罫線 (--border / --border-soft / --border-strong / --border-focus)
    QString border, borderSoft, borderStrong, borderFocus;
    // アクセント (--acc / --acc-fg) — ドメインで決まる
    QString accent, accentHover, accentPressed, accentFg;
    // 角丸 (--r-*) / 余白 (--pad-*) / 行高 (--row-h) / 文字サイズ (--fs-*)
    int radiusSm = 2, radiusMd = 3, radiusLg = 4;
    int padSm = 4, padMd = 6, padLg = 10;
    int rowH = 22;
    int fsApp = 12, fsSm = 11, fsXs = 10;
};

// ── 小道具 ──────────────────────────────────────────────────────────────────
QString px(int v)
{
    return QString::number(v) + QStringLiteral("px");
}

// α 合成して不透明色に畳み込む。
// (モックの rgba(...) をそのまま QSS へ出すと Qt 側の解釈差が出るため)
QString mix(const QString &base, const QString &over, double a)
{
    const QColor b(base);
    const QColor o(over);
    const int r = qRound(b.red()   * (1.0 - a) + o.red()   * a);
    const int g = qRound(b.green() * (1.0 - a) + o.green() * a);
    const int l = qRound(b.blue()  * (1.0 - a) + o.blue()  * a);
    return QColor(r, g, l).name(QColor::HexRgb).toUpper();
}

// filter: brightness(...) 相当 (アクセントの hover / pressed 用)
QString lighten(const QString &hex, int factor)
{
    return QColor(hex).lighter(factor).name(QColor::HexRgb).toUpper();
}

QString darken(const QString &hex, int factor)
{
    return QColor(hex).darker(factor).name(QColor::HexRgb).toUpper();
}

// ── :root — Classic (Qt Fusion 相当・既定) ──────────────────────────────────
Palette classicPalette()
{
    Palette p;
    p.bgApp           = "#ECECEC";
    p.bgPanel         = "#F4F4F4";
    p.bgInput         = "#FFFFFF";
    p.bgTab           = "#DCDCDC";
    p.bgTabActive     = "#F4F4F4";
    p.bgTitlebar      = "#2C2C2C";
    p.bgMenubar       = "#E8E8E8";
    p.bgStatusbar     = "#DCDCDC";
    p.bgViewport      = "#FAFAFA";
    p.bgButton        = "#F0F0F0";
    p.bgButtonHover   = "#E5F1FB";
    p.bgButtonPressed = "#CCE4F7";
    p.bgSection       = "#FFFFFF";
    p.bgRowAlt        = "#F7F7F7";
    p.bgHover         = mix("#FFFFFF", "#005A9E", 0.08); // rgba(0,90,158,.08)
    p.bgSelected      = "#CCE4F7";

    p.fgApp           = "#1F1F1F";
    p.fgMuted         = "#5A5A5A";
    p.fgDim           = "#8C8C8C";
    p.fgTitlebar      = "#F5F5F5";
    p.fgSection       = "#003C73";

    p.border          = "#ABABAB";
    p.borderSoft      = "#C8C8C8";
    p.borderStrong    = "#707070";
    p.borderFocus     = "#0078D4";

    p.accent          = "#0078D4";   // 既定 (--acc-em)。domain で上書き
    p.accentFg        = "#FFFFFF";

    p.radiusSm = 2;  p.radiusMd = 3;  p.radiusLg = 4;
    p.padSm    = 4;  p.padMd    = 6;  p.padLg    = 10;
    p.rowH     = 22;
    p.fsApp    = 12; p.fsSm     = 11; p.fsXs     = 10;
    return p;
}

// ── [data-style="modern"] ───────────────────────────────────────────────────
void applyModern(Palette &p)
{
    p.bgApp           = "#F7F8FA";
    p.bgPanel         = "#FFFFFF";
    p.bgInput         = "#FFFFFF";
    p.bgTab           = "#F0F2F5";
    p.bgTabActive     = "#FFFFFF";
    p.bgTitlebar      = "#FFFFFF";
    p.bgMenubar       = "#FFFFFF";
    p.bgStatusbar     = "#F0F2F5";
    p.bgViewport      = "#FAFBFC";
    p.bgButton        = "#FFFFFF";
    p.bgButtonHover   = "#F0F4FA";
    p.bgButtonPressed = "#E0E8F3";
    p.bgSection       = "#FFFFFF";
    p.bgRowAlt        = "#FAFBFC";
    p.bgHover         = mix("#FFFFFF", "#0F4C9E", 0.06); // rgba(15,76,158,.06)
    p.bgSelected      = "#E3EDFB";

    p.fgApp           = "#111419";
    p.fgMuted         = "#5C6573";
    p.fgTitlebar      = "#111419";
    p.fgSection       = "#2A3344";
    // --fg-dim は modern では未定義 → classic を継承

    p.border          = "#D5D9E0";
    p.borderSoft      = "#E4E7EB";
    p.borderStrong    = "#B6BCC4";

    p.radiusSm = 4;  p.radiusMd = 6;  p.radiusLg = 10;
    p.padSm    = 6;  p.padMd    = 10; p.padLg    = 14;
    p.rowH     = 28;
    p.fsApp    = 13;
}

// ── [data-style="scientific"] (ParaView / COMSOL 系・暗色) ──────────────────
void applyScientific(Palette &p)
{
    p.bgApp           = "#2D3037";
    p.bgPanel         = "#383B43";
    p.bgInput         = "#1F2229";
    p.bgTab           = "#2D3037";
    p.bgTabActive     = "#44474F";
    p.bgTitlebar      = "#1A1C20";
    p.bgMenubar       = "#383B43";
    p.bgStatusbar     = "#1A1C20";
    p.bgViewport      = "#11151B";
    p.bgButton        = "#44474F";
    p.bgButtonHover   = "#565A63";
    p.bgButtonPressed = "#2A2D33";
    p.bgSection       = "#383B43";
    p.bgRowAlt        = "#3E414A";
    p.bgHover         = mix("#383B43", "#7AB2FF", 0.12); // rgba(122,178,255,.12)
    p.bgSelected      = mix("#383B43", "#0078D4", 0.35); // rgba(0,120,212,.35)

    p.fgApp           = "#E6E8EC";
    p.fgMuted         = "#A0A4AC";
    p.fgDim           = "#6B6F78";
    p.fgTitlebar      = "#E6E8EC";
    p.fgSection       = "#7AB2FF";

    p.border          = "#1A1C20";
    p.borderSoft      = "#4A4D55";
    p.borderStrong    = "#0A0B0E";

    p.radiusSm = 3;  p.radiusMd = 4;  p.radiusLg = 6;
    p.fsApp    = 12;
    // --pad-* / --row-h は scientific では未定義 → classic を継承
}

// ── [data-theme="dark"] — style の後に来るので配色をすべて上書き ────────────
void applyDark(Palette &p)
{
    p.bgApp           = "#1F2228";
    p.bgPanel         = "#2A2E36";
    p.bgInput         = "#14171C";
    p.bgTab           = "#1F2228";
    p.bgTabActive     = "#2A2E36";
    p.bgTitlebar      = "#14171C";
    p.bgMenubar       = "#2A2E36";
    p.bgStatusbar     = "#14171C";
    p.bgViewport      = "#0D1015";
    p.bgButton        = "#353A44";
    p.bgButtonHover   = "#424955";
    p.bgButtonPressed = "#2A2F38";
    p.bgSection       = "#2A2E36";
    p.bgRowAlt        = "#2F333C";
    p.bgHover         = mix("#2A2E36", "#78B4FF", 0.10); // rgba(120,180,255,.10)
    p.bgSelected      = mix("#2A2E36", "#0078D4", 0.30); // rgba(0,120,212,.30)

    p.fgApp           = "#E2E4E8";
    p.fgMuted         = "#9298A2";
    p.fgDim           = "#5E646E";
    p.fgTitlebar      = "#E2E4E8";
    p.fgSection       = "#7AB2FF";

    p.border          = "#0E1116";
    p.borderSoft      = "#3A3F49";
    p.borderStrong    = "#050608";
    // 角丸 / 余白 / 文字サイズは dark ブロックでは未定義 → style の値を保つ
}

// ── [data-density=...] — style / dark の後 (最後) に適用 ────────────────────
void applyDensity(Palette &p, Density d)
{
    switch (d) {
        case Density::Compact:
            p.rowH  = 20;
            p.padSm = 3;  p.padMd = 5;  p.padLg = 8;
            p.fsApp = 11;
            break;
        case Density::Comfortable:
            p.rowH  = 30;
            p.padSm = 8;  p.padMd = 12; p.padLg = 16;
            p.fsApp = 13;
            break;
        case Density::Normal:
            break;   // 既定値 (style のまま)
    }
}

// ── 合成: classic → style → dark → density → domain accent ─────────────────
Palette makePalette(UiStyle style, UiTheme theme, Density density, Domain domain)
{
    Palette p = classicPalette();

    switch (style) {
        case UiStyle::Modern:     applyModern(p);     break;
        case UiStyle::Scientific: applyScientific(p); break;
        case UiStyle::Classic:                        break;
    }
    if (theme == UiTheme::Dark)
        applyDark(p);
    applyDensity(p, density);

    // [data-domain=...] — アクセントのみドメインで差し替え (--acc-*)
    p.accent        = accentColor(domain);
    p.accentHover   = lighten(p.accent, 115);
    p.accentPressed = darken(p.accent, 115);
    p.accentFg      = "#FFFFFF";
    return p;
}

// ── QSS 生成 ────────────────────────────────────────────────────────────────
// 文字列は素の連結のみで組む (QString::arg は QSS 中の '%' と紛らわしいので不使用)。
QString buildQss(const Palette &p)
{
    const QString rSm = px(p.radiusSm);
    const QString rMd = px(p.radiusMd);
    const QString rLg = px(p.radiusLg);
    const QString pSm = px(p.padSm);
    const QString pMd = px(p.padMd);
    const QString pLg = px(p.padLg);
    const QString fsApp = px(p.fsApp);
    const QString fsSm  = px(p.fsSm);
    const QString fsXs  = px(p.fsXs);
    // 入力系の総高が --row-h になるよう content 高を出す (上下 border 1px 分を引く)
    const QString inputH = px(p.rowH > 6 ? p.rowH - 2 : p.rowH);
    const QString mono =
        "'Cascadia Mono','Consolas','SF Mono','Menlo','MS Gothic',monospace";

    QString s;
    s.reserve(20000);

    // ── base ────────────────────────────────────────────────────────────────
    s += "/* OpenFDTD-X — generated by ofd::Theme (do not edit by hand) */\n";
    s += "QWidget {\n"
         "    background: " + p.bgApp + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    font-size: " + fsApp + ";\n"
         "}\n";
    s += "QMainWindow, QDialog {\n"
         "    background: " + p.bgApp + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QMainWindow::separator {\n"
         "    background: " + p.borderSoft + ";\n"
         "    width: 2px;\n"
         "    height: 2px;\n"
         "}\n";
    s += "QMainWindow::separator:hover { background: " + p.accent + "; }\n";
    // 文字系ウィジェットは親の背景を透かす (QWidget 一括指定の副作用回避)
    s += "QLabel, QCheckBox, QRadioButton, QTabBar, QSplitter, QToolBar QLabel,\n"
         "QGroupBox::title, QDialogButtonBox {\n"
         "    background: transparent;\n"
         "}\n";
    s += "QLabel:disabled, QCheckBox:disabled, QRadioButton:disabled {\n"
         "    color: " + p.fgDim + ";\n"
         "}\n";
    s += "QToolTip {\n"
         "    background: " + p.bgPanel + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.border + ";\n"
         "    border-radius: " + rMd + ";\n"
         "    padding: 3px 5px;\n"
         "}\n";
    // sep-h / VLine 相当のヘアライン
    s += "QFrame[frameShape=\"4\"] {\n"
         "    background: " + p.borderSoft + ";\n"
         "    border: none;\n"
         "    max-height: 1px;\n"
         "}\n";
    s += "QFrame[frameShape=\"5\"] {\n"
         "    background: " + p.borderSoft + ";\n"
         "    border: none;\n"
         "    max-width: 1px;\n"
         "}\n";

    // ── menubar / menu ──────────────────────────────────────────────────────
    s += "QMenuBar {\n"
         "    background: " + p.bgMenubar + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: none;\n"
         "    border-bottom: 1px solid " + p.borderSoft + ";\n"
         "    padding: 0 4px;\n"
         "}\n";
    s += "QMenuBar::item {\n"
         "    background: transparent;\n"
         "    padding: 3px 8px;\n"
         "    border-radius: " + rSm + ";\n"
         "}\n";
    s += "QMenuBar::item:selected {\n"
         "    background: " + p.bgSelected + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QMenuBar::item:pressed {\n"
         "    background: " + p.bgButtonPressed + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QMenu {\n"
         "    background: " + p.bgPanel + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.border + ";\n"
         "    border-radius: " + rMd + ";\n"
         "    padding: 3px;\n"
         "}\n";
    s += "QMenu::item {\n"
         "    background: transparent;\n"
         "    padding: 4px 24px 4px 22px;\n"
         "    border-radius: " + rSm + ";\n"
         "}\n";
    s += "QMenu::item:selected {\n"
         "    background: " + p.bgSelected + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QMenu::item:disabled { color: " + p.fgDim + "; }\n";
    s += "QMenu::separator {\n"
         "    height: 1px;\n"
         "    background: " + p.borderSoft + ";\n"
         "    margin: 4px 8px;\n"
         "}\n";

    // ── toolbar / tool button ───────────────────────────────────────────────
    s += "QToolBar {\n"
         "    background: " + p.bgMenubar + ";\n"
         "    border: none;\n"
         "    border-bottom: 1px solid " + p.borderSoft + ";\n"
         "    spacing: 2px;\n"
         "    padding: 3px 6px;\n"
         "}\n";
    s += "QToolBar::separator {\n"
         "    background: " + p.borderSoft + ";\n"
         "    width: 1px;\n"
         "    height: 18px;\n"
         "    margin: 0 4px;\n"
         "}\n";
    s += "QToolButton {\n"
         "    background: transparent;\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid transparent;\n"
         "    border-radius: " + rSm + ";\n"
         "    padding: " + pSm + " " + pMd + ";\n"
         "}\n";
    s += "QToolButton:hover {\n"
         "    background: " + p.bgButtonHover + ";\n"
         "    border-color: " + p.borderSoft + ";\n"
         "}\n";
    s += "QToolButton:pressed {\n"
         "    background: " + p.bgButtonPressed + ";\n"
         "    border-color: " + p.borderSoft + ";\n"
         "}\n";
    s += "QToolButton:checked {\n"
         "    background: " + p.bgSelected + ";\n"
         "    border-color: " + p.borderSoft + ";\n"
         "    font-weight: 600;\n"
         "}\n";
    s += "QToolButton:disabled { color: " + p.fgDim + "; }\n";
    // tb-btn.primary — MainWindow 側の setStyleSheet は不要になる
    s += "QToolButton#primaryAction {\n"
         "    background: " + p.accent + ";\n"
         "    color: " + p.accentFg + ";\n"
         "    border: 1px solid transparent;\n"
         "    border-radius: " + rSm + ";\n"
         "    padding: " + pSm + " " + pLg + ";\n"
         "    font-weight: 600;\n"
         "}\n";
    s += "QToolButton#primaryAction:hover {\n"
         "    background: " + p.accentHover + ";\n"
         "    border-color: transparent;\n"
         "}\n";
    s += "QToolButton#primaryAction:pressed {\n"
         "    background: " + p.accentPressed + ";\n"
         "    border-color: transparent;\n"
         "}\n";
    // domain-tabs — 濃色バー (mock: background var(--bg-titlebar))
    s += "#DomainBar {\n"
         "    background: " + p.bgTitlebar + ";\n"
         "    border-bottom: 1px solid " + p.borderStrong + ";\n"
         "}\n";
    s += "#DomainBar QToolButton { color: " + p.fgTitlebar + "; }\n";

    // ── push button (q-btn / q-seg) ─────────────────────────────────────────
    s += "QPushButton {\n"
         "    background: " + p.bgButton + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.border + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    padding: 0 " + pLg + ";\n"
         "    min-height: " + inputH + ";\n"
         "}\n";
    s += "QPushButton:hover { background: " + p.bgButtonHover + "; }\n";
    s += "QPushButton:pressed { background: " + p.bgButtonPressed + "; }\n";
    s += "QPushButton:focus { border-color: " + p.borderFocus + "; }\n";
    s += "QPushButton:default { border-color: " + p.accent + "; }\n";
    // 排他 checkable ボタン = mock の .q-seg button.active
    s += "QPushButton:checked {\n"
         "    background: " + p.accent + ";\n"
         "    color: " + p.accentFg + ";\n"
         "    border-color: transparent;\n"
         "    font-weight: 600;\n"
         "}\n";
    s += "QPushButton:disabled {\n"
         "    background: " + p.bgButton + ";\n"
         "    color: " + p.fgDim + ";\n"
         "    border-color: " + p.borderSoft + ";\n"
         "}\n";

    // ── inputs (q-input / q-num / q-select) ─────────────────────────────────
    s += "QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox,\n"
         "QComboBox, QDateEdit, QTimeEdit, QDateTimeEdit {\n"
         "    background: " + p.bgInput + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.border + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    padding: 0 " + pSm + ";\n"
         "    min-height: " + inputH + ";\n"
         "    selection-background-color: " + p.accent + ";\n"
         "    selection-color: " + p.accentFg + ";\n"
         "}\n";
    s += "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus,\n"
         "QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {\n"
         "    border-color: " + p.borderFocus + ";\n"
         "}\n";
    s += "QLineEdit:disabled, QTextEdit:disabled, QPlainTextEdit:disabled,\n"
         "QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {\n"
         "    background: " + p.bgApp + ";\n"
         "    color: " + p.fgDim + ";\n"
         "    border-color: " + p.borderSoft + ";\n"
         "}\n";
    s += "QLineEdit:read-only {\n"
         "    background: " + p.bgApp + ";\n"
         "    color: " + p.fgMuted + ";\n"
         "}\n";
    // spin box: 右側にボタン分の余白を確保
    s += "QSpinBox, QDoubleSpinBox { padding: 0 17px 0 " + pSm + "; }\n";
    s += "QSpinBox::up-button, QDoubleSpinBox::up-button {\n"
         "    subcontrol-origin: border;\n"
         "    subcontrol-position: top right;\n"
         "    width: 15px;\n"
         "    background: " + p.bgButton + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border-left: 1px solid " + p.borderSoft + ";\n"
         "    border-top-right-radius: " + rSm + ";\n"
         "}\n";
    s += "QSpinBox::down-button, QDoubleSpinBox::down-button {\n"
         "    subcontrol-origin: border;\n"
         "    subcontrol-position: bottom right;\n"
         "    width: 15px;\n"
         "    background: " + p.bgButton + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border-left: 1px solid " + p.borderSoft + ";\n"
         "    border-bottom-right-radius: " + rSm + ";\n"
         "}\n";
    s += "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,\n"
         "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {\n"
         "    background: " + p.bgButtonHover + ";\n"
         "}\n";
    s += "QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,\n"
         "QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed {\n"
         "    background: " + p.bgButtonPressed + ";\n"
         "}\n";
    // combo box
    s += "QComboBox { padding: 0 20px 0 " + pSm + "; }\n";
    s += "QComboBox::drop-down {\n"
         "    subcontrol-origin: border;\n"
         "    subcontrol-position: top right;\n"
         "    width: 16px;\n"
         "    background: " + p.bgButton + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border-left: 1px solid " + p.borderSoft + ";\n"
         "    border-top-right-radius: " + rSm + ";\n"
         "    border-bottom-right-radius: " + rSm + ";\n"
         "}\n";
    s += "QComboBox::drop-down:hover { background: " + p.bgButtonHover + "; }\n";
    s += "QComboBox QAbstractItemView {\n"
         "    background: " + p.bgInput + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.border + ";\n"
         "    selection-background-color: " + p.bgSelected + ";\n"
         "    selection-color: " + p.fgApp + ";\n"
         "    outline: 0;\n"
         "}\n";

    // ── check / radio ───────────────────────────────────────────────────────
    s += "QCheckBox, QRadioButton {\n"
         "    color: " + p.fgApp + ";\n"
         "    spacing: 5px;\n"
         "}\n";
    s += "QCheckBox::indicator, QGroupBox::indicator {\n"
         "    width: 13px;\n"
         "    height: 13px;\n"
         "    background: " + p.bgInput + ";\n"
         "    border: 1px solid " + p.borderStrong + ";\n"
         "    border-radius: " + rSm + ";\n"
         "}\n";
    s += "QCheckBox::indicator:hover, QGroupBox::indicator:hover {\n"
         "    border-color: " + p.accent + ";\n"
         "}\n";
    s += "QCheckBox::indicator:checked, QGroupBox::indicator:checked {\n"
         "    background: " + p.accent + ";\n"
         "    border-color: " + p.accent + ";\n"
         "}\n";
    s += "QCheckBox::indicator:indeterminate {\n"
         "    background: " + p.fgDim + ";\n"
         "    border-color: " + p.borderStrong + ";\n"
         "}\n";
    s += "QCheckBox::indicator:disabled, QGroupBox::indicator:disabled {\n"
         "    background: " + p.bgApp + ";\n"
         "    border-color: " + p.borderSoft + ";\n"
         "}\n";
    s += "QRadioButton::indicator {\n"
         "    width: 13px;\n"
         "    height: 13px;\n"
         "    background: " + p.bgInput + ";\n"
         "    border: 1px solid " + p.borderStrong + ";\n"
         "    border-radius: 7px;\n"
         "}\n";
    s += "QRadioButton::indicator:hover { border-color: " + p.accent + "; }\n";
    s += "QRadioButton::indicator:checked {\n"
         "    background: " + p.bgInput + ";\n"
         "    border: 4px solid " + p.accent + ";\n"
         "    border-radius: 7px;\n"
         "}\n";
    s += "QRadioButton::indicator:disabled {\n"
         "    background: " + p.bgApp + ";\n"
         "    border-color: " + p.borderSoft + ";\n"
         "}\n";

    // ── group box (q-section) ───────────────────────────────────────────────
    s += "QGroupBox {\n"
         "    background: " + p.bgSection + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.borderSoft + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    margin-top: 10px;\n"
         "    padding: " + pLg + " " + pMd + " " + pMd + " " + pMd + ";\n"
         "}\n";   // font-weight は ::title のみ (QGroupBox 本体に指定すると子へ伝播する)
    s += "QGroupBox::title {\n"
         "    subcontrol-origin: margin;\n"
         "    subcontrol-position: top left;\n"
         "    left: 8px;\n"
         "    padding: 0 6px;\n"
         "    color: " + p.fgSection + ";\n"
         "    font-size: " + fsApp + ";\n"
         "    font-weight: 600;\n"
         "}\n";

    // ── table (q-table) ────────────────────────────────────────────────────
    s += "QTableView, QTableWidget {\n"
         "    background: " + p.bgInput + ";\n"
         "    alternate-background-color: " + p.bgRowAlt + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    gridline-color: " + p.borderSoft + ";\n"
         "    border: 1px solid " + p.borderSoft + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    selection-background-color: " + p.bgSelected + ";\n"
         "    selection-color: " + p.fgApp + ";\n"
         "    outline: 0;\n"
         "}\n";
    s += "QTableView::item, QTableWidget::item { padding: 2px 5px; }\n";
    s += "QTableView::item:hover, QTableWidget::item:hover {\n"
         "    background: " + p.bgHover + ";\n"
         "}\n";
    s += "QTableView::item:selected, QTableWidget::item:selected {\n"
         "    background: " + p.bgSelected + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QHeaderView { background: " + p.bgTab + "; border: none; }\n";
    s += "QHeaderView::section {\n"
         "    background: " + p.bgTab + ";\n"
         "    color: " + p.fgMuted + ";\n"
         "    font-size: " + fsSm + ";\n"
         "    font-weight: 600;\n"
         "    padding: 3px 5px;\n"
         "    border: none;\n"
         "    border-right: 1px solid " + p.borderSoft + ";\n"
         "    border-bottom: 1px solid " + p.borderSoft + ";\n"
         "}\n";
    s += "QHeaderView::section:hover {\n"
         "    background: " + p.bgButtonHover + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QHeaderView::section:checked {\n"
         "    background: " + p.bgSelected + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QTableCornerButton::section {\n"
         "    background: " + p.bgTab + ";\n"
         "    border: none;\n"
         "    border-right: 1px solid " + p.borderSoft + ";\n"
         "    border-bottom: 1px solid " + p.borderSoft + ";\n"
         "}\n";

    // ── tree / list (rd-tree) ──────────────────────────────────────────────
    s += "QTreeView, QTreeWidget, QListView, QListWidget {\n"
         "    background: " + p.bgInput + ";\n"
         "    alternate-background-color: " + p.bgRowAlt + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.borderSoft + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    selection-background-color: " + p.bgSelected + ";\n"
         "    selection-color: " + p.fgApp + ";\n"
         "    outline: 0;\n"
         "}\n";
    s += "QTreeView::item, QTreeWidget::item, QListView::item, QListWidget::item {\n"
         "    padding: 2px 4px;\n"
         "    border: none;\n"
         "}\n";
    s += "QTreeView::item:hover, QTreeWidget::item:hover,\n"
         "QListView::item:hover, QListWidget::item:hover {\n"
         "    background: " + p.bgHover + ";\n"
         "}\n";
    s += "QTreeView::item:selected, QTreeWidget::item:selected,\n"
         "QListView::item:selected, QListWidget::item:selected {\n"
         "    background: " + p.bgSelected + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";

    // ── tabs (q-subtabs / vp-tabs) ─────────────────────────────────────────
    s += "QTabWidget::pane {\n"
         "    background: " + p.bgTabActive + ";\n"
         "    border: 1px solid " + p.borderSoft + ";\n"
         "    top: -1px;\n"
         "}\n";
    s += "QTabWidget::tab-bar { left: 4px; }\n";
    s += "QTabBar::tab {\n"
         "    background: " + p.bgTab + ";\n"
         "    color: " + p.fgMuted + ";\n"
         "    font-size: " + fsSm + ";\n"
         "    padding: " + pSm + " " + pMd + ";\n"
         "    margin-right: -1px;\n"
         "    border: 1px solid " + p.borderSoft + ";\n"
         "    border-bottom: none;\n"
         "    border-top: 2px solid transparent;\n"
         "}\n";
    s += "QTabBar::tab:hover:!selected {\n"
         "    background: " + p.bgButtonHover + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QTabBar::tab:selected {\n"
         "    background: " + p.bgTabActive + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border-top-color: " + p.accent + ";\n"
         "    font-weight: 600;\n"
         "}\n";
    s += "QTabBar::tab:disabled { color: " + p.fgDim + "; }\n";

    // ── scroll area (qt-tabbody) / scroll bars ─────────────────────────────
    s += "QScrollArea, QScrollArea > QWidget, QScrollArea > QWidget > QWidget {\n"
         "    background: " + p.bgTabActive + ";\n"
         "}\n";
    s += "QScrollArea { border: none; }\n";
    s += "QScrollBar:vertical {\n"
         "    background: " + p.bgTab + ";\n"
         "    width: 12px;\n"
         "    margin: 0px;\n"
         "    border: none;\n"
         "}\n";
    s += "QScrollBar::handle:vertical {\n"
         "    background: " + p.borderSoft + ";\n"
         "    border: 3px solid " + p.bgTab + ";\n"
         "    border-radius: " + rLg + ";\n"
         "    min-height: 28px;\n"
         "}\n";
    s += "QScrollBar::handle:vertical:hover { background: " + p.border + "; }\n";
    s += "QScrollBar:horizontal {\n"
         "    background: " + p.bgTab + ";\n"
         "    height: 12px;\n"
         "    margin: 0px;\n"
         "    border: none;\n"
         "}\n";
    s += "QScrollBar::handle:horizontal {\n"
         "    background: " + p.borderSoft + ";\n"
         "    border: 3px solid " + p.bgTab + ";\n"
         "    border-radius: " + rLg + ";\n"
         "    min-width: 28px;\n"
         "}\n";
    s += "QScrollBar::handle:horizontal:hover { background: " + p.border + "; }\n";
    s += "QScrollBar::add-line, QScrollBar::sub-line {\n"
         "    width: 0px;\n"
         "    height: 0px;\n"
         "    background: none;\n"
         "    border: none;\n"
         "}\n";
    s += "QScrollBar::add-page, QScrollBar::sub-page { background: none; }\n";

    // ── dock / splitter ────────────────────────────────────────────────────
    // font-size は ::title だけに指定する (本体へ指定すると dock の中身全部に伝播)
    s += "QDockWidget {\n"
         "    background: " + p.bgPanel + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    s += "QDockWidget > QWidget { background: " + p.bgPanel + "; }\n";
    s += "QDockWidget::title {\n"
         "    background: " + p.bgTab + ";\n"
         "    color: " + p.fgMuted + ";\n"
         "    font-size: " + fsSm + ";\n"
         "    font-weight: 600;\n"
         "    padding: 5px 8px;\n"
         "    border-bottom: 1px solid " + p.borderSoft + ";\n"
         "    text-align: left;\n"
         "}\n";
    s += "QSplitter::handle { background: " + p.borderSoft + "; }\n";
    s += "QSplitter::handle:horizontal { width: 2px; }\n";
    s += "QSplitter::handle:vertical { height: 2px; }\n";
    s += "QSplitter::handle:hover { background: " + p.accent + "; }\n";

    // ── status bar ─────────────────────────────────────────────────────────
    s += "QStatusBar {\n"
         "    background: " + p.bgStatusbar + ";\n"
         "    color: " + p.fgMuted + ";\n"
         "    border-top: 1px solid " + p.borderSoft + ";\n"
         "    font-size: " + fsSm + ";\n"
         "}\n";
    s += "QStatusBar::item { border: none; }\n";
    s += "QStatusBar QLabel {\n"
         "    background: transparent;\n"
         "    color: " + p.fgMuted + ";\n"
         "    padding: 0 8px;\n"
         "    border-right: 1px solid " + p.borderSoft + ";\n"
         "}\n";

    // ── slider / progress bar (q-progress) ─────────────────────────────────
    s += "QSlider::groove:horizontal {\n"
         "    background: " + p.borderSoft + ";\n"
         "    height: 4px;\n"
         "    border-radius: 2px;\n"
         "}\n";
    s += "QSlider::sub-page:horizontal {\n"
         "    background: " + p.accent + ";\n"
         "    border-radius: 2px;\n"
         "}\n";
    s += "QSlider::handle:horizontal {\n"
         "    background: " + p.bgButton + ";\n"
         "    border: 1px solid " + p.borderStrong + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    width: 11px;\n"
         "    margin: -5px 0;\n"
         "}\n";
    s += "QSlider::groove:vertical {\n"
         "    background: " + p.borderSoft + ";\n"
         "    width: 4px;\n"
         "    border-radius: 2px;\n"
         "}\n";
    s += "QSlider::add-page:vertical {\n"
         "    background: " + p.accent + ";\n"
         "    border-radius: 2px;\n"
         "}\n";
    s += "QSlider::handle:vertical {\n"
         "    background: " + p.bgButton + ";\n"
         "    border: 1px solid " + p.borderStrong + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    height: 11px;\n"
         "    margin: 0 -5px;\n"
         "}\n";
    s += "QSlider::handle:hover {\n"
         "    background: " + p.bgButtonHover + ";\n"
         "    border-color: " + p.accent + ";\n"
         "}\n";
    s += "QProgressBar {\n"
         "    background: " + p.bgInput + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border: 1px solid " + p.border + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    font-size: " + fsSm + ";\n"
         "    text-align: center;\n"
         "}\n";
    s += "QProgressBar::chunk {\n"
         "    background: " + p.accent + ";\n"
         "    border-radius: " + rSm + ";\n"
         "    margin: 0px;\n"
         "}\n";

    // ── TabNavigator — カテゴリ見出し付き縦タブ (qt-tabbar 相当) ───────────
    s += "#tabNavigator {\n"
         "    background: " + p.bgTab + ";\n"
         "    color: " + p.fgMuted + ";\n"
         "    border: none;\n"
         "    border-right: 1px solid " + p.borderSoft + ";\n"
         "    border-radius: 0px;\n"
         "    outline: 0;\n"
         "    font-size: " + fsApp + ";\n"
         "}\n";
    s += "#tabNavigator::item {\n"
         "    background: transparent;\n"
         "    color: " + p.fgMuted + ";\n"
         "    padding: " + pSm + " " + pMd + ";\n"
         "    border: none;\n"
         "    border-left: 3px solid transparent;\n"
         "}\n";
    s += "#tabNavigator::item:selected {\n"
         "    background: " + p.bgTabActive + ";\n"
         "    color: " + p.fgApp + ";\n"
         "    border-left: 3px solid " + p.accent + ";\n"
         "    font-weight: 500;\n"
         "}\n";
    s += "#tabNavigator::item:hover:!selected:enabled {\n"
         "    background: " + p.bgHover + ";\n"
         "    color: " + p.fgApp + ";\n"
         "}\n";
    // 選択不可項目 = カテゴリ見出し (小さめ・淡色・大文字は font 側で設定済み)
    s += "#tabNavigator::item:disabled {\n"
         "    background: transparent;\n"
         "    color: " + p.fgDim + ";\n"
         "    font-size: " + fsXs + ";\n"
         "    padding: " + pLg + " " + pSm + " " + pSm + " " + pSm + ";\n"
         "    border-left: 3px solid transparent;\n"
         "}\n";

    // ── viewport / plot 面 ─────────────────────────────────────────────────
    s += "#Viewport3D, #PlotPanel {\n"
         "    background: " + p.bgViewport + ";\n"
         "    border: 1px solid " + p.borderSoft + ";\n"
         "}\n";

    // ── LogConsole — どのテーマでも常に暗色コンソール (log-console) ────────
    s += "#LogConsole {\n"
         "    background: #0E1116;\n"
         "    color: #DDE2E8;\n"
         "    border: 1px solid #2A2F3A;\n"
         "    border-radius: " + rSm + ";\n"
         "    padding: 6px 8px;\n"
         "    font-family: " + mono + ";\n"
         "    font-size: " + fsSm + ";\n"
         "    selection-background-color: " + p.accent + ";\n"
         "    selection-color: #FFFFFF;\n"
         "}\n";
    s += "#LogConsole QScrollBar:vertical, #LogConsole QScrollBar:horizontal {\n"
         "    background: #14171C;\n"
         "    border: none;\n"
         "}\n";
    s += "#LogConsole QScrollBar::handle:vertical,\n"
         "#LogConsole QScrollBar::handle:horizontal {\n"
         "    background: #3A3F49;\n"
         "    border: 3px solid #14171C;\n"
         "    border-radius: " + rLg + ";\n"
         "}\n";

    return s;
}

} // namespace

// ── public API ──────────────────────────────────────────────────────────────
QString Theme::qss(UiStyle style, UiTheme theme, Density density, Domain domain)
{
    return buildQss(makePalette(style, theme, density, domain));
}

UiStyle Theme::styleFromKey(const QString &key)
{
    if (key == QStringLiteral("modern"))     return UiStyle::Modern;
    if (key == QStringLiteral("scientific")) return UiStyle::Scientific;
    return UiStyle::Classic;
}

QString Theme::styleKey(UiStyle s)
{
    switch (s) {
        case UiStyle::Modern:     return QStringLiteral("modern");
        case UiStyle::Scientific: return QStringLiteral("scientific");
        case UiStyle::Classic:    break;
    }
    return QStringLiteral("classic");
}

UiTheme Theme::themeFromKey(const QString &key)
{
    return key == QStringLiteral("dark") ? UiTheme::Dark : UiTheme::Light;
}

QString Theme::themeKey(UiTheme t)
{
    return t == UiTheme::Dark ? QStringLiteral("dark") : QStringLiteral("light");
}

Density Theme::densityFromKey(const QString &key)
{
    if (key == QStringLiteral("compact"))     return Density::Compact;
    if (key == QStringLiteral("comfortable")) return Density::Comfortable;
    return Density::Normal;
}

QString Theme::densityKey(Density d)
{
    switch (d) {
        case Density::Compact:     return QStringLiteral("compact");
        case Density::Comfortable: return QStringLiteral("comfortable");
        case Density::Normal:      break;
    }
    return QStringLiteral("normal");
}

bool Theme::isDarkPalette(UiStyle style, UiTheme theme)
{
    // scientific は単体で暗色パレット
    return theme == UiTheme::Dark || style == UiStyle::Scientific;
}

} // namespace ofd
