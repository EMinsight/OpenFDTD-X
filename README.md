# OpenFDTD-X — Qt6 マルチドメインGUI

**電磁 / 光 / 室内音響 / 水中音響** の各ドメインを統一 UI で扱う Qt6 Widgets
デスクトップアプリ。`claude.ai/design` で作成された HTML/CSS/JS モックを実コードへ
起こしたもの。

このリポジトリは **画面 (GUI) のみ** を持つ。FDTD/RCWA/BPM の **処理ロジック
(ソルバーカーネル) の実体は別リポジトリ** にあり、本アプリはそれらを subprocess
として起動するだけでソースには依存しない (疎結合):

| 役割 | リポジトリ | 起動バイナリ |
|---|---|---|
| 電磁・光 FDTD | [Sirokujira/OpenFDTD](https://github.com/Sirokujira/OpenFDTD) | `ofd` / `ofd_mpi` / `ofd_cuda` + `ofd_post` |
| 周期構造 RCWA | [Sirokujira/OpenRCWA](https://github.com/Sirokujira/OpenRCWA) | `orcwa` / `orcwa_mpi` / `orcwa_cuda` + `orcwa_post` |
| 導波路 BPM | [Sirokujira/OpenBPM](https://github.com/Sirokujira/OpenBPM) | `obpm` / `obpm_mpi` / `obpm_cuda` + `obpm_post` |

加えて tidy3d (Flexcompute 社の光FDTDクラウド) への Python スクリプト書き出しにも
対応する。GUI が出力する `.ofd` は本家カーネルがそのまま読める完全互換形式。

## ビルド

### 必要環境
- Qt 6.2+ (Widgets モジュールのみ必須)
- CMake 3.21+
- C++17 コンパイラ

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### オプション
| オプション | 既定 | 説明 |
|---|---|---|
| `-DUSE_HDF5=ON`   | OFF | HDF5 時系列/プロジェクト出力 (`io/H5Writer`) |
| `-DUSE_LIBIGL=ON` | OFF | より高精度な共形/winding-number ボクセル化 (`docs/libigl-integration.md`)。標準ビルドでも `io/Voxelizer` の階段近似ボクセル化は有効 |
| `-DBUILD_TESTS=ON`| ON  | `.ofd` ラウンドトリップ + ボクセル化 自己テスト (`ofdx_selftest`) |
| `-DBUILD_OPENUWA=ON`| ON | 水中音響の分離アプリ `openuwa`。OFF でターゲットごと外せる (本体 `openfdtd_x` には影響しない) |

### 実行
本体 `openfdtd_x` と、水中音響の分離アプリ `openuwa` の 2 つが生成される
(共有 GUI は `ofdx_gui` 静的ライブラリとして 1 度だけコンパイルされる。
`openuwa` は `-DBUILD_OPENUWA=OFF` で生成対象から外せる)。

```bash
# 既存プロジェクトを開く
./build/openfdtd_x tests/data/dipole.ofd

# OpenUWA (水中音響 分離アプリ) — ドメイン切替を持たず水中固定。
# 海洋環境 / 伝搬解析 (SSP/Bellhop/PE) / 音源・指向性 / H5アニメ /
# ツール連携 の 5 タブを本体と同じ実装で再利用する。
./build/openuwa tests/data/dipole.ofd

# 起動ドメイン・言語を指定 (ja|en|both)
./build/openfdtd_x --domain optical --lang both

# 処理ロジック (ソルバーカーネル) の場所 (なければ PATH を探索)
export OPENFDTD_HOME=/path/to/OpenFDTD   # ofd, ofd_mpi, ofd_cuda ...
export OPENRCWA_HOME=/path/to/OpenRCWA   # orcwa, orcwa_post ...
export OPENBPM_HOME=/path/to/OpenBPM     # obpm, obpm_post ...
```

#### macOS での実行

macOS ではアプリが `.app` バンドル (`openfdtd_x.app` / `openuwa.app`) として
生成されるため、上記の `./build/openfdtd_x` というパスは存在しない。
バンドルはディレクトリなので `./build/openfdtd_x.app` を直接実行しても
`permission denied` になる (chmod では解決しない)。実体のバイナリは
バンドル内の `Contents/MacOS/` にある:

```bash
# コマンドライン引数付きで実行する場合はバンドル内の実体を指定する (CI と同じ)
./build/openfdtd_x.app/Contents/MacOS/openfdtd_x tests/data/dipole.ofd
./build/openuwa.app/Contents/MacOS/openuwa tests/data/dipole.ofd

# 引数なしで GUI を開くだけなら open でもよい
# (open 経由では作業ディレクトリが変わるため、相対パス引数は使えない)
open ./build/openfdtd_x.app
```

`ofdx_selftest` はバンドルではない通常の実行ファイルなので、macOS でも
`./build/ofdx_selftest` のまま実行できる。

### 自己テスト
`tests/data` に同梱した OpenFDTD サンプル `.ofd` をロード → シリアライズ → 再パースし、
構造が完全一致することを確認する (本家フォーマット互換性の保証)。
```bash
./build/ofdx_selftest                 # 24 files + voxelizer, 0 failures
```

## アーキテクチャ

データモデル中心。タブはすべて `Project` の View であり、編集すると
`Project::changed()` / `loaded()` シグナルでビューポート・ツリー・ステータスバーが
自動更新される。

```
src/
├── main.cpp                  アプリエントリ (CLI: file / --lang / --domain / --screenshot)
├── MainWindow.{h,cpp}        メインシェル (メニュー/ツールバー/ドック/ステータスバー)
├── DomainBar.{h,cpp}         電磁/光/室内音響/水中 切替タブ
├── RightDock.{h,cpp}         プロジェクトツリー + 実行ログ
├── I18n.{h,cpp}              日英バイリンガル翻訳テーブル
│
├── core/                     データモデル (値オブジェクト + Project)
│   ├── Project.{h,cpp}         全体モデル + CFL/メモリ推定 + 永続化
│   ├── Domain.h               物理ドメイン enum + アクセントカラー
│   ├── Material.h             material 行 (通常/分散性) + load 行
│   ├── Geometry.h             geometry 行 (本家 shape コード)
│   ├── Source.h               feed / planewave / point 行
│   ├── MeshAxis.h             xmesh/ymesh/zmesh (座標+分割)
│   ├── PostOpts.h             plot* ポスト処理キー一式
│   ├── GlassCatalog.{h,cpp}   光学ガラスDB + Sellmeier + AGF/CSV取込
│   └── RoomAcoustics.{h,cpp}  Sabine/Eyring + Barron統計 + 鏡像法 + NC評価
│
├── io/
│   ├── OfdIO.{h,cpp}          .ofd テキスト読み書き (本家完全互換) + .ofdx JSON サイドカー
│   ├── Touchstone.{h,cpp}     S パラメータ .s1p/.s2p 出力
│   ├── Tidy3dExporter.{h,cpp} 光プロジェクト → tidy3d Python スクリプト生成
│   ├── H5Writer.{h,cpp}       HDF5 出力 (USE_HDF5 時)
│   ├── StlImporter.{h,cpp}    STL (バイナリ/ASCII) 取込
│   ├── Voxelizer.{h,cpp}      STL→Yee格子 階段近似ボクセル化 (libigl非依存)
│   └── EvReader.h             ev2/ev3 ネイティブパーサ骨格
│
├── kernel/
│   └── Runner.{h,cpp}         QProcess で ofd/orcwa/obpm (+_mpi/_cuda) と *_post を起動
│
├── widgets/
│   ├── SectionBox.{h,cpp}     見出し付きグループボックス
│   ├── MiniPlot.{h,cpp}       共用XYミニプロット (分散曲線/RT60/NC/エコーグラム)
│   ├── UnitNav.{h,cpp}        ユニット番号ナビ (◀ n/総数 ▶)
│   ├── Viewport3D.{h,cpp}     QPainter 製 3Dワイヤフレームビュー (OpenGL不要)
│   ├── PlotPanel.{h,cpp}      波源波形プレビュー + 収束履歴プロット
│   ├── LogConsole.{h,cpp}     等幅ログペイン
│   └── EvViewer.{h,cpp}       ev2/ev3 図形を3戦略 (HTML/外部exe/ネイティブ) で表示
│
└── tabs/                     左ドック各タブ (本家章立て + ドメイン拡張)
    ├── GeneralTab    全般 (solver/abc/pbc/frequency)
    ├── MeshTab       メッシュ (xmesh/ymesh/zmesh)
    ├── MaterialTab   物性値・集中定数 (material/load)
    ├── GeometryTab   物体形状 (geometry + STL取込)
    ├── SourceTab     波源・観測点 (feed/planewave/point)
    ├── Post1Tab      ポスト処理(1) 周波数特性
    ├── Post2Tab      ポスト処理(2) 遠方界・近傍界
    ├── OpticalTab    光解析 (FDTD/RCWA/BPM/FMM 切替) ※光ドメイン時のみ
    ├── AcousticTab   室内音響 (RT60/C80/STI/可聴化)  ※音響ドメイン時のみ
    ├── UnderwaterTab 水中音響 (SSP/SOFAR/ソナー)     ※水中ドメイン時のみ
    ├── Tidy3dTab     tidy3d クラウド連携             ※光ドメイン時のみ
    ├── GlassCatalogTab 🔷 ガラスカタログ (Sellmeier/アッベ図/AGF取込) ※光
    └── RoomAcousticsTab 🏛 ホール解析 (カバレッジ/エコーグラム/Sabine/NC/障害診断) ※音響
```

## 設計判断

1. **本家 `.ofd` 形式に完全互換** — `OfdIO` は `sol/input_data.c` と
   `post/post_data.c` が解釈する全キーを 1:1 で読み書きする。GUI が
   モデル化しないキーは `Project::extraLines()` に保持し、保存時にそのまま
   書き戻すので手編集ファイルがラウンドトリップで壊れない。

2. **拡張ドメインは `.ofdx` (JSON) に分離** — 光・音響・水中・tidy3d の設定は
   `.ofd` と同じ basename のサイドカー JSON に保存。本家カーネルは無視するので
   **下位互換 100%**。

3. **既存カーネルを subprocess として再利用** — `Runner` が CPU/GPU × MPI の
   4種バイナリと姉妹ソルバー (FDTD=`ofd`, RCWA=`orcwa`, BPM=`obpm`) を
   `QProcess` で起動。OpenMP は `OMP_NUM_THREADS`、MPI は `mpiexec -n N`。
   solver→post の2段実行 (一括モード) にも対応。

4. **tidy3d は光FDTD専用のクラウドバックエンド** — 独立した物理ドメインではなく
   光ドメインのタブとして配置。クラウド送信ボタンは光ドメイン選択時のみ有効。

5. **ビューポートは OpenGL 非依存** — `Viewport3D` は `QPainter` の正射影
   ワイヤフレーム。ヘッドレス/リモート環境でも動作する。

## デザインリファレンス
元の HTML/CSS/JS モック (`claude.ai/design` バンドル) と各画面が対応する。
`docs/` に `.ofd` 処理パイプライン・ev2/ev3 形式・libigl 統合の設計メモを収録。
