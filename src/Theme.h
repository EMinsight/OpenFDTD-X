// Theme.h — デザインモックの CSS 変数テーマを Qt6 QSS として生成する
// (元: project/styles.css の :root / [data-style] / [data-theme] /
//  [data-density] / [data-domain] ブロック)
//
// 静的な resources/styles/openfdtd.qss を置き換えるもの。
// Palette (CSS 変数相当) を組み立ててから QSS 文字列を生成するので、
// スタイル / テーマ / 密度 / ドメインの組み合わせが実際に見た目へ反映される。
#pragma once
#include <QString>
#include "core/Domain.h"

namespace ofd {

enum class UiStyle  { Classic, Modern, Scientific };   // data-style
enum class UiTheme  { Light, Dark };                    // data-theme
enum class Density  { Compact, Normal, Comfortable };    // data-density

class Theme {
public:
    // Build the full application stylesheet for this combination.
    // (この組み合わせのアプリ全体スタイルシートを生成する)
    static QString qss(UiStyle style, UiTheme theme, Density density, Domain domain);

    // Persisted settings helpers ("ui/style", "ui/theme", "ui/density").
    static UiStyle styleFromKey(const QString &key);     // "classic"|"modern"|"scientific"
    static QString styleKey(UiStyle s);
    static UiTheme themeFromKey(const QString &key);     // "light"|"dark"
    static QString themeKey(UiTheme t);
    static Density densityFromKey(const QString &key);   // "compact"|"normal"|"comfortable"
    static QString densityKey(Density d);

    // True when the resolved palette is dark (scientific counts as dark) —
    // callers use it to pick viewport/plot ink colours.
    // (ビューポート / プロットの線色を決めるのに使う)
    static bool isDarkPalette(UiStyle style, UiTheme theme);
};

} // namespace ofd
