---
paths:
  - "src/acoustics/**"
  - "tests/acoustics/**"
---

# 音響コア (C++14 / Qt 非依存) の規則

`src/acoustics/core/` と `src/acoustics/c_api/` は外部カーネルからも
リンクされる可搬コア。GUI 層と同じ流儀で書いてはならない。

## 言語制約 (CMake の CXX_STANDARD 14 で強制されるが、書く前に守る)

- C++14 まで。**禁止**: `std::optional` / `std::variant` /
  `std::string_view` / `std::filesystem` / 構造化束縛 (`auto [a,b]`) /
  `if constexpr`
- **Qt ヘッダの include 禁止** (`#include <Q...>` が 1 つでもあれば違反)。
  Qt との橋渡しは `src/acoustics/qt/QtAcousticAdapter` のみが行う。
- C API (`c_api/openfdtd_x_acoustics.h`) は **STL 型を公開しない**。
  構造体は `struct_size` / `api_version` 規約でのみ拡張 (末尾追加のみ)。
- `tests/acoustics/test_c_api.c` は C99 でコンパイルされる —
  C API ヘッダが純 C で通ることの検証を兼ねる。壊さない。

## 数値実装の規則

- 乱数・現在時刻に依存する計算を入れない (再現性)。
- 一時パスは TMPDIR → TEMP → TMP → /tmp の順で解決
  (`std::filesystem` が使えないため。test_wav.cpp の tempPath 参照)。
- 新しい指標を追加したら `docs/opera-acoustics-validation.md` に
  合成信号による検証 (期待値と許容誤差) を追記し、対応する
  ctest を追加する。テストなしの指標追加は不可。

## 表示品質の規則 (Qt 層に伝播する契約)

- CalibrationState (Absolute/Relative/Uncalibrated) を必ず伝える。
  Uncalibrated で絶対 SPL を返す API を作らない。
- 動的レンジ不足の T30 は quality フラグで invalid を返す
  (「それらしい数値」を黙って返さない)。
- VoiceType 分析は音響量の報告に限る。医学的・教育的結論を
  生成するコード/文字列を追加しない (ADR-0006)。
