# OpenFDTD-X — Claude Code プロジェクトメモリ

Qt6 Widgets 製マルチドメイン FDTD GUI (電磁 / 光 / 室内音響 / 水中音響)。
`claude.ai/design` の HTML/CSS/JS モックを実 C++ に起こしたもの。
**このリポジトリは GUI のみ** — ソルバーカーネル (ofd/orcwa/obpm) は別リポジトリで、
`kernel/Runner` が subprocess として起動する (疎結合)。

## ビルド & テスト (変更後は必ず全部実行 / 最初にこれが通ること)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ./build/ofdx_selftest        # 自己テスト (0 failures 必須)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure   # 音響コア/ランナーのテスト
```

- 必要環境: Qt 6.2+ (Widgets のみ), CMake 3.21+, C++17。macOS は `brew install qt`。
- `ofdx_selftest` は .ofd ラウンドトリップ + voxelizer + .ofdx サイドカーの自己テスト。
  **チェック総数は減ってはならない** (回帰の兆候)。
- GUI 実行不要のヘッドレス環境では常に `QT_QPA_PLATFORM=offscreen`。
- GUI スモーク (ヘッドレス):
  `QT_QPA_PLATFORM=offscreen ./build/openfdtd_x tests/data/dipole.ofd --screenshot /tmp/x.png`
- 新規ソースは **CMakeLists.txt の GUI_SOURCES / CORE_SOURCES に手で追加** (glob していない)。

## アーキテクチャの掟

1. **データモデル中心**: タブは全て `core/Project` の View。編集後は `Project::touch()`。
   `changed()` / `loaded()` シグナルでビューポート・ツリー・ステータスバーが自動更新。
2. **`.ofd` は本家完全互換**: `io/OfdIO` が読み書き。GUI が知らないキーは
   `Project::extraLines()` に保持し保存時にそのまま書き戻す (ラウンドトリップ保証)。
   `.ofd` の書式を変える変更は禁止。拡張は `.ofdx` (JSON サイドカー) へ。
   互換性規則は `.claude/rules/io-compat.md` / `.claude/rules/ofd-format.md`。
3. **タブの作法**: `QScrollArea` 継承、ctor は `(Project*, QWidget*)`、
   `apply()` (widgets→model) / `refresh()` (model→widgets, `m_updating` ガード付き)。
   見出し付きグループは `widgets/SectionBox`、小型プロットは `widgets/MiniPlot`。
4. **i18n**: 全 UI 文字列は `I18n::tr("key")`。共通キーは `I18n.cpp` の `loadTables()`、
   タブ固有語彙は各 .cpp 冒頭の file-local `I18n::reg()` (接頭辞をタブ毎に固有に)。
   日本語がベース、`--lang en|both` で切替。
5. **依存を増やさない**: Qt6 Widgets のみ。OpenGL / QML / Qt Charts / 外部ライブラリ禁止
   (HDF5 と libigl はオプションで既にゲート済み)。描画は QPainter。
6. **ドメイン別タブ**: 表示のオン/オフは `MainWindow::onDomainChanged()` で行う。
   tidy3d は独立ドメインではなく光ドメイン専用のクラウドバックエンド。

## 2 層構造 (混ぜない)

| 層 | 場所 | 言語 | 制約 |
|---|---|---|---|
| GUI 層 | `src/` (tabs, io, kernel, widgets) | C++17 + Qt6 | Qt API は 6.4.2 まで (CI Linux の下限) |
| 音響コア | `src/acoustics/core/`, `c_api/` | C++14 | Qt 禁止。詳細は `.claude/rules/acoustics-core.md` |

- カーネル連携: `src/kernel/Runner` (ofd/orcwa/obpm/bellhopcxx)、`AcousticRunner`
  (metadata.json + rir.wav → metrics.json + solver.log の QProcess 契約)。
  水中音響は `io/BellhopIO` が BELLHOP の .env を生成して bellhopcxx を起動
  (環境変数 `BELLHOPCUDA_HOME`)。光の FMM は RCWA と同一手法のため orcwa を共用。
- 永続化: `.ofd` (カーネル入力、`src/io/OfdIO`) + `.ofdx` (JSON sidecar)。

## コーディングスタイル

- コメントは日本語ベース (既存ファイル参照)。ヘッダ冒頭に「// Xxx.h — 役割 (元jsx名)」。
- Qt6 スタイルの connect (ポインタメンバ関数)。`QStringLiteral` は任意。
- UTF-8 リテラル直書き OK (MSVC は `/utf-8` 済み)。
- デザインリファレンス: claude.ai/design プロジェクト「OpenFDTD対応」のモック
  (app.jsx / tabs.jsx ほか)。画面を変える前にモックを確認し、忠実に再現する。

## 絶対規則 / やってはいけないこと

1. **既存の `.ofd` / `.ofdx` キーを削除・改名・順序変更しない**。追加のみ。
2. **無効化された新機能はシリアライズ出力を 1 バイトも変えない**
   (後方互換。selftest でバイト一致を検証する)。
3. **ビルド成果物をコミットしない**: `build/` に加え `*.gch` (12MB の
   プリコンパイル済みヘッダ混入事故の前例あり)、`*.o`, moc 生成物。
4. **ライセンス不明のコードをコピーしない** (外部ライブラリ追加は要相談。
   FFT/畳み込み等は自前実装が既にある — 車輪を再発明しない)。
5. **未完成機能を UI 上で動作済みと表示しない** (「未実装」と明示する)。
6. 校正なしで絶対 SPL を表示しない / 動的レンジ不足の T30 を有効値として
   表示しない (音響指標の表示規則 — `docs/adr/` 参照)。
7. コミットメッセージ・コードに AI モデル ID を書かない。
8. `tests/data/*.ofd` を書き換えない (本家 OpenFDTD のサンプルそのもの)。
9. カーネルソースへの依存を追加しない (subprocess 起動のみ)。
10. `ofdx_selftest` を通さないままコミットしない。

## 検証許容値 (docs/opera-acoustics-validation.md)

- EDT/T20/T30: ±5%、C50/C80: ±0.2 dB、D50: ±0.01、Ts: ±1 ms
- ONN 活性化カーブ (TPA): 解析解 T=1/(1+β(P/A_eff)L) に対し ±7%

## CI (.github/workflows/ci.yml)

- Linux: ubuntu-latest + apt qt6-base-dev (**Qt 6.4.2 = API 下限を規定**)
- macOS: macos-latest + Homebrew qt
- Windows: windows-latest + Qt 6.8 LTS (msvc2022)。Qt 6.5 以前は
  MSVC 14.43 の stdext 削除でビルド不能 — バージョンを下げない。

## ドキュメント

設計判断は `docs/adr/`、既知の負債・未実装は
`docs/opera-acoustics-development-status.md` §3 が正。実装状態を変えたら
status 文書も更新する。計画文書だけ作って実装を終了しない。
