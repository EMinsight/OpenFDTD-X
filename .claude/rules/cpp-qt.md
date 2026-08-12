---
paths:
  - "src/**/*.cpp"
  - "src/**/*.h"
---

# C++ / Qt ルール

- Qt6 Widgets のみ。QML / Qt Quick / Qt Charts / OpenGL は追加しない。描画は QPainter。
- connect は Qt6 ポインタメンバ関数構文。文字列ベース SIGNAL/SLOT 禁止。
- 新しいタブ = `QScrollArea` 派生 + `(Project*, QWidget*)` ctor + `apply()`/`refresh()`
  + `m_updating` 再入ガード。手本: `src/tabs/OpticalTab.cpp`。
- UI 文字列は必ず `I18n::tr()`。タブ固有キーは file-local `I18n::reg()` で登録し、
  接頭辞をタブ毎に固有にする (例: `mon_`, `sreg_`)。
- 新規 .cpp/.h は CMakeLists.txt の GUI_SOURCES / CORE_SOURCES に追加する (glob 無し)。
- **Qt を include しないソース (core/ の一部, optics/, acoustics/core/) で M_PI を
  使わない。** MSVC は `<cmath>` だけでは定義せず、Windows CI だけが落ちる
  (2026-08-12 に optics/PhaseNoise.cpp で実際に踏んだ)。ファイル内に
  `const double kPi = 3.14159265358979323846;` を置く (optics/PhotonicCircuit.cpp
  の流儀)。Qt ヘッダを include するタブ側は M_PI で通る。
- core/ は QObject 非依存の値オブジェクト + Project のみ。GUI から core への参照は可、
  逆は不可。io/ は QWidget を include しない。
- メモリ管理は Qt の親子所有。生 new + parent 渡しが流儀。delete を手書きしない。
