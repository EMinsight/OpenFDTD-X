# ADR-0001: 二層 C++ 規格 (GUI C++17 / 音響コア C++14)

## Status

Accepted (2026-07-16) — 実装済み (`CMakeLists.txt` の
`ofdx_acoustic_core` / `ofdx_acoustic_c_api` ターゲット)。

## Context

- 音響分析コアは、フェーズ5 の外部音響ソルバー (別バイナリ・別
  リポジトリ) や他プロジェクトからの再利用を想定する。再利用先には
  古いツールチェーン (CUDA、組込み、長期サポートコンパイラ) が含まれ
  得るため、要求する言語規格は低いほどよい。目標は C++14。
- 一方、プロジェクト全体を C++14 に下げることは**不可能**である:
  Qt6 が C++17 を強制する (フェーズ0 調査 §6.2 の実測 — Qt 6.4.2 の
  `qglobal.h:106` は `-std=c++14` に対し
  `#error "Qt requires a C++17 compiler"` を出す)。
- 「C++17 でビルドしつつ C++14 の範囲で書く」運用は、逸脱 (うっかり
  `std::optional` や構造化束縛を使う) を機械的に検知できない
  (フェーズ0 調査 §7.1)。
- 既存コード (`src/core/` 等) は QString/QVector を使い Qt に密結合で
  あり、そのままでは Qt 非依存コアの土台にならない。

## Decision

1. GUI・既存モデル層はグローバル設定のまま **C++17** (Qt6 の要求)。
2. 音響コアは**別 STATIC ライブラリターゲット** `ofdx_acoustic_core`
   に分離し、ターゲット単位で
   `CXX_STANDARD 14; CXX_STANDARD_REQUIRED ON; CXX_EXTENSIONS OFF`
   を設定する。**Qt には一切リンクしない** (Qt ヘッダ include も禁止)。
3. コアの公開型は `std::vector` / `std::string` / POD のみ。例外は
   送出せず `AcousticResult<T>` + エラーコード 16 値で通知する
   (既存 core の「戻り値 + err」スタイルに整合)。
4. Qt 型との変換は `src/acoustics/qt/QtAcousticAdapter` の 1 箇所に
   集約する (このアダプターは C++17/Qt 側)。
5. さらに安定 C ABI (`ofdx_acoustic_c_api`, C99 から利用可能、
   `struct_size`/`api_version` による前方互換検査) を被せ、C++ ABI に
   依存しない再利用経路も提供する。テスト `test_c_api.c` は
   **C コンパイラ (-std=c99)** でビルドされること自体が検証項目。

## Consequences

- (+) C++17 構文がコアに混入すると**その場でビルドエラー**になり、
  C++14 準拠がレビュー頼みでなく機械的に保証される。
- (+) コアは Qt の LGPL 義務・バージョン制約から独立し、外部カーネル・
  他プロジェクト・純 C クライアントへ持ち出せる。
- (+) テスト (tests/acoustics) も Qt 非依存・フレームワーク非依存で、
  `g++ -std=c++14` 単独でもビルド・実行できる (検証記録で実証)。
- (−) `std::optional` / `std::string_view` / 構造化束縛等が使えない。
  結果型 (`AcousticResult<T>`)・ビュー型 (`ArrayView<T>`) を自前定義
  するコストを払った (実装済み、計 200 行程度)。
- (−) Qt 型 ⇔ std 型の変換層 (QtAcousticAdapter) という間接層が
  1 枚増える。変換箇所を 1 ファイルに限定することで管理する。
- (−) CMake のグローバル `CMAKE_CXX_STANDARD 17` とターゲット個別
  設定が併存するため、新規音響ソースを誤って GUI ターゲットに足すと
  規格チェックが効かない。音響コアのソース追加は必ず
  `ofdx_acoustic_core` に行うこと。
