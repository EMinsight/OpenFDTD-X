# オペラ歌手向け室内音響分析 — 既存実装調査 (フェーズ0)

対象: OpenFDTD-X リポジトリ `claude/opera-acoustics` ブランチ (= main, 19f751c)。
本書は新機能追加前の事前調査であり、実コード
(`src/core/RoomAcoustics.{h,cpp}`, `src/core/Project.{h,cpp}`, `src/io/OfdIO.cpp`,
`src/kernel/Runner.{h,cpp}`, `src/tabs/AcousticTab.*`, `src/tabs/RoomAcousticsTab.*`,
`src/widgets/MiniPlot.*`, `tests/selftest.cpp`, `src/MainWindow.cpp`, `src/I18n.cpp`,
`CMakeLists.txt`, `.github/workflows/ci.yml`) を読んで事実のみを記載する。

## 1. 現在の音響機能一覧と計算方法

音響計算はすべて `src/core/RoomAcoustics.{h,cpp}` (namespace `ofd::roomac`) に
集約されている。GUI 非依存の純関数群だが、`QString`/`QVector` を使うため Qt に
リンクする。帯域は 125/250/500/1k/2k/4k Hz の6帯域 (`kBandHz[6]`)。
ヘッダコメントに明記の通り、いずれも「FDTD 実行前の設計段階で使う統計的推定」で
あり、カーネル計算の代替ではない。

### 1.1 吸音バジェットと総吸音力 A — `totalAbsorption(a, band)`

- `AbsorptionRow` (Project.h) の enabled 行のみ合算。
- 通常行: `A += clamp(α[band], 0, 1) × area`。
- 客席行 (`role==Audience`): α に占有率係数 `occupancyFactor(occupancy)` を乗算
  (空席 0.70 / 半分 0.85 / 満席 1.0)。
- 空気吸収行 (`role==Air`): 1kHz の吸音力 `airA` [Sabin] を入力し、帯域値は
  固定比 `kAirRatio[6] = {0.10, 0.20, 0.55, 1.0, 2.1, 6.2}` (20°C/50%RH 代表値) で換算。

### 1.2 残響時間 RT60 — `rt60(a, band, formula)`

- Sabine (`formula==0`): `RT = 0.161 · V / A`。
- Eyring (`formula==1`): 面吸音とAir行を分離し、
  `ᾱ = clamp((A − A_air)/S, 0, 0.999)`、`RT = 0.161 · V / (−S·ln(1−ᾱ) + A_air)`。
- V は `a.volume` (下限1)、S は `a.surface` (下限1)。既定式は `a.rtFormula` (既定 Eyring)。

### 1.3 席位置指標 G / C80 / C50 / D50 / STI — `seatMetrics(r, T, V)` (Barron 修正理論)

音源距離 r [m]、残響時間 T [s]、室容積 V [m³] から:

- 直接音エネルギー `d = 100 / r²`
- 残響エネルギー `rev = (31200 · T / V) · e^(−0.04·r/T)`
- 初期割合 `split(t) = 1 − e^(−13.8·(t/1000)/T)` で 80ms / 50ms 分割
  (`e80 = rev·split(80)`, `l80 = rev − e80`; 50ms も同様)
- `G = 10·log₁₀(d + rev)`
- `C80 = 10·log₁₀((d + e80)/l80)`、`C50 = 10·log₁₀((d + e50)/l50)`
- `D50 = (d + e50)/(d + rev)`
- `RT = T` (使用した帯域値をそのまま返す)

STI 概算 (Houtgast–Steeneken の MTF 法、無騒音・単帯域):

- `m(F) = |D + R/(1+jx)| / (D + R)`、`x = 2πF·T/13.8`
  (実装は実部 `D + R/(1+x²)`、虚部 `R·x/(1+x²)` の絶対値)
- 変調周波数 `F = 0.63 × 2^(k/3)`, k=0..13 (0.63〜12.7 Hz, 1/3oct 14点)
- `SNR = 10·log₁₀(m/(1−m))` を ±15 dB でクランプ、`TI = (SNR+15)/30`
- STI = 14点 TI の単純平均。帯域重み・聴覚マスキング・暗騒音は考慮しない (概算)。

### 1.4 エコーグラム / ITDG — `echogram(a, src, rcv)` (シューボックス1次鏡像法)

- 室はシューボックス `roomL × roomW × roomH`、音速 c₀ = 343 m/s 固定。
- 6面 (床/天井/側壁L/側壁R/舞台側/後壁) それぞれの1次鏡像音源のみ (2次以上なし)。
- 遅延: `Δt = (rᵢ − r_d)/c₀ × 1000` [ms] (直接音基準)。
- レベル: `20·log₁₀(r_d/rᵢ) + 10·log₁₀(1 − α@1k)` [dB] (直接音 0 dB 基準)。
  面のαは `faceAlpha1k()` — 該当 role の最初の enabled 行の α[3] (1kHz)、
  客席は占有率係数を乗算、行が無ければ既定 0.2。
- `early` フラグは 80ms 以内。到達時刻でソートし先頭は直接音 (time=0)。
- `itdgMs()`: ソート済み反射列の最初の反射時刻 = 初期時間遅れ間隙。

### 1.5 NC 評価 — `ncRating(levels[7])` / `ncCurve(nc)`

- 入力は 63/125/250/500/1k/2k/4k Hz の7帯域騒音レベル [dB] (8kHz は省略)。
- Beranek の NC-15〜NC-70 (5刻み12曲線) の内蔵テーブル `kNcTable[12][7]`。
- タンジェント法: 各帯域で測定値を挟む隣接曲線間を線形補間し、全帯域の最大値を
  切り上げて NC 値とする (0〜70 にクランプ)。
- `ncCurve(nc)` は基準曲線の7帯域値をプロット用に返す。

### 1.6 音響障害診断 — `detectDefects(a, src, rcv)`

- フラッターエコー: 対向面ペア (側壁L-R / 床-天井 / 舞台側-後壁) の両方が
  α@1k < 0.2 なら検出。両方 < 0.12 で severity=高、それ以外は中。
- ロングディレイエコー: 1次鏡像法の反射のうち Δt > 50ms かつ相対レベル > −10dB。
  > −6dB で severity=高。

### 1.7 客席カバレッジ (UI 側計算) — `CoverageMap` (RoomAcousticsTab.cpp)

- 扇形ホール平面に 11×11 セルを配置 (奥行き比 t で半幅 `(0.15+0.35t)·roomW` が広がる)。
- 各セルの舞台原点距離 r から `seatMetrics()` で G/C80/STI/RT を計算し
  HSL 色 (赤→緑) にマップ (RT は短いほど緑に反転)。帯域は単一または6帯域平均。
- 平均・標準偏差を集計し、均一性判定 (STI: σ<0.08、その他: σ<3.0)。
- 受音点は固定4点 (kReceivers: 中央前列/左サイド/右サイド/後方中央、高さ1.2m)、
  音源は舞台 (0.05L, 0.5W, 1.5m) 固定。

## 2. UI 構成

### 2.1 タブ一覧 (MainWindow.cpp)

共通タブ (常時): 全般 / メッシュ / 物性値・集中定数 / 物体形状 / 波源・観測点 /
ポスト処理(1) / ポスト処理(2)。

ドメイン切替 (`DomainBar` → `Project::setActiveDomain` →
`MainWindow::onDomainChanged` で動的に追加/削除):

| ドメイン | 追加タブ |
|---|---|
| 電磁波 (EM) | なし |
| 光 | 光解析 / 🔷 ガラスカタログ / tidy3d連携 |
| 室内音響 | 音響解析 (`AcousticTab`) / 🏛 ホール解析 (`RoomAcousticsTab`) |
| 水中音響 | 水中音響 |

中央は `Viewport3D` (QPainter ワイヤフレーム) と `PlotPanel` のスタック +
`EvViewer`。右ドックにプロジェクトツリー + 実行ログ。ステータスバーに
cells/mem/Δt(CFL)/進捗。

### 2.2 AcousticTab と RoomAcousticsTab の役割分担

- **AcousticTab** (115行): FDTD 実行時の音響解析「設定」タブ。指標チェック
  (RT60/C80/D50/STI/EDT)、インパルス応答、可聴化、サンプリング周波数
  (44.1/48/96k)、音源指向性 (omni/cardioid/speaker)、音源SPL、マイク本数を
  `AcousticOpts` に書き込むだけで、計算は行わない。
- **RoomAcousticsTab** (817行): FDTD 前の統計モデルによる「ホール解析」タブ。
  5サブタブ構成: 客席カバレッジ / エコーグラム / 残響計算 (寸法・V・S・占有率・
  式選択・吸音バジェット表・帯域別RT60プロット) / 暗騒音 NC / 音響障害診断。
  加えて Markdown レポート出力 (`exportReport`) と カバレッジ PNG 出力。
  編集は即座に `AcousticOpts` へ反映され `Project::touch()` で通知される。
  吸音バジェット表は α125/α500/α1k/α4k のみ編集可能で、250Hz と 2kHz は隣接
  帯域の平均で補間される (`applyBudgetTable`)。

### 2.3 MiniPlot の能力と制約 (widgets/MiniPlot.{h,cpp}, 130行)

能力:
- 複数系列の XY 折れ線 (色/破線/マーカー/ラベル凡例)。
- 固定 Y レンジ (`setYRange`) または自動 (8% パディング)。
- インパルスモード (`setImpulseMode`) — 各点を縦線描画 (エコーグラム用)。
- `setXTickPow10` — x に log10 値を渡すと目盛りだけ 10^x 表示 (疑似 log-x 軸)。

制約:
- QPainter 直描きのみ。ズーム・パン・カーソル・ツールチップ等の対話機能なし。
- 目盛りは X/Y とも固定4分割の線形グリッドのみ。真の対数軸・独立した目盛り
  制御・第2軸なし。
- 棒グラフ/面塗り/ヒートマップなし (ヒートマップは CoverageMap が別実装)。
- 画像出力 API なし (RoomAcousticsTab は `QWidget::grab()` で代用)。
- スペクトログラムや長時系列 (数万点) 向けの間引き・高速化なし。

### 2.4 I18n (I18n.cpp, 558行)

シングルトンの日英対訳テーブル (`add(key, ja, en)` 約391キー)。言語は
ja / en / both (併記)。新 UI 文言はキー追加が必須。

## 3. データモデルと永続化

### 3.1 AcousticOpts の全フィールド (Project.h より転記)

```
struct AbsorptionRow {
    enum Role { Audience, Ceiling, SideWall, RearWall, Floor, Air, Other };
    bool    enabled = true;
    int     role = Other;
    QString name;
    double  area = 0;                            // m² (Air 行は未使用)
    double  alpha[6] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 };
    double  airA = 0;                            // Air 行: A [Sabin] 直接指定
};

struct AcousticOpts {
    bool    rt60 = true, c80 = true, d50 = false, sti = false, edt = false;
    bool    impulseResponse = true;
    bool    auralization = false;
    int     sampleRate = 48000;      // WAV出力サンプリング周波数
    QString srcDirectivity = "omni"; // omni / cardioid / speaker
    double  srcSPL_dB = 94.0;
    int     micCount = 1;
    // ── ホール解析 (RoomAcousticsTab) ──
    double  roomL = 30.0, roomW = 20.0, roomH = 12.0;  // シューボックス [m]
    double  volume = 12000.0;        // 室容積 V [m³]
    double  surface = 3800.0;        // 総表面積 S [m²]
    int     occupancy = 2;           // 0=空席, 1=半分, 2=満席
    int     rtFormula = 1;           // 0=Sabine, 1=Eyring
    QVector<AbsorptionRow> absorption;
    double  noiseLevels[7] = { 42, 38, 33, 28, 24, 21, 18 };  // 63..4kHz [dB]
};
```

`Project::clear()` はコンサートホール例の既定吸音バジェット6行
(客席680m²/天井900m²/側壁620m²/後壁180m²/床420m²/空気38Sabin) を投入する。

### 3.2 .ofdx の `acoustic` オブジェクトの全キー (OfdIO.cpp save/load より)

トップレベル: `schemaVersion`("1.0"), `domain`, `optical`, `acoustic`,
`underwater`, `tidy3d`。

`acoustic` オブジェクト:

| キー | 型 | 対応フィールド |
|---|---|---|
| `rt60` / `c80` / `d50` / `sti` / `edt` | bool | 指標チェック |
| `impulse_response` | bool | impulseResponse |
| `auralization` | bool | auralization |
| `sample_rate` | int | sampleRate |
| `src_directivity` | string | srcDirectivity |
| `src_spl_db` | double | srcSPL_dB |
| `mic_count` | int | micCount |
| `room_l` / `room_w` / `room_h` | double | roomL/W/H |
| `volume` / `surface` | double | volume / surface |
| `occupancy` | int | occupancy |
| `rt_formula` | int | rtFormula |
| `absorption` | array | 各要素 `{enabled, role, name, area, alpha[6], air_a}` |
| `noise_levels` | array[7] | noiseLevels |

ロード側は全キーを `value(key).toXxx(現在値)` で読むため、**欠落キーは既定値の
まま・未知キーは無視**される (前方/後方互換の要)。`role` は enum の整数値を
そのまま永続化している点に注意 (enum の並び替え禁止)。
`.ofd` 本体は音響情報を一切持たない (本家カーネル互換のため)。未知の .ofd キーは
`Project::extraLines()` に保持され保存時に書き戻される。

## 4. Runner の対応カーネル (kernel/Runner.{h,cpp})

- enum: `Engine { CPU, CPU_MPI, GPU, GPU_MPI }`,
  `Kernel { FDTD, RCWA, BPM }`, `RunMode { Solver, Post, Both }`。
- バイナリ名: prefix は FDTD=`ofd` / RCWA=`orcwa` / BPM=`obpm`。
  Engine で `_mpi` / `_cuda` / `_cuda_mpi` を付加。post は `<prefix>_post`。
- 探索順 (`resolveBinary`): ① `cfg.binaryDir` → ② 環境変数
  (`OPENFDTD_HOME` / `OPENRCWA_HOME` / `OPENBPM_HOME`) → ③ アプリ実行
  ディレクトリ配下 `kernel/` → ④ アプリ実行ディレクトリ → ⑤ PATH 委任。
  Windows では `.exe` を付加。
- 実行: プロジェクトを作業ディレクトリに `.ofd`+`.ofdx` 保存後、QProcess 起動。
  MPI は `mpiexec -n <processes> <bin>`、OpenMP は環境変数 `OMP_NUM_THREADS`。
  Both モードは solver 完了後に post を連続実行。
  進捗は stdout の `"%7d %f %f"` 形式行を正規表現で解析。
- **音響専用カーネルは存在しない**。音響ドメインでも `currentRunConfig()` は
  Kernel::FDTD (=`ofd`) のままで、AcousticOpts は .ofdx に書かれるだけである
  (本家 `ofd` は .ofdx を無視する)。

## 5. 現在のテスト範囲 (tests/selftest.cpp, 386行)

実測: `24 files loaded, 1560 checks, 0 failures` (exit 0)。CI は失敗数のみ判定
(チェック総数は非検証)。内訳:

1. **.ofd ラウンドトリップ** (24サンプル × `compareProjects`):
   title/solver/abc/pbc/frequency1・2、3軸メッシュ (ノード数・分割・座標値)、
   material (type/値/分散値)、geometry (材質/形状/座標)、feed
   (dir/位置/パラメータ)、planewave、point数、load数、PostOpts
   (plotiter/plotsmith/zin/ref/far1d/far2d/near1d/near2d/far1ddb)、
   extraLines の完全一致。数値は相対 1e-9 の `nearlyEq`。
2. **testVoxelizer**: 10³格子で立方体をボクセル化し占有216セル・36ラン・
   原点被覆を確認。
3. **testGlassCatalog**: 内蔵カタログ≥19銘柄、N-BK7 Sellmeier nd 再現、
   全銘柄の nd 再現不変条件、CSV 短行スキップ、AGF 取込ラウンドトリップ。
4. **testRoomAcoustics**: Sabine 既知値 (V=12000, A=1200 → 1.61s)、
   Eyring < Sabine、Barron の距離単調性 (C80/D50/G 低下)、STI∈(0,1]、
   NC (無音≈0 / NC-25曲線=25 / 過大=70クランプ)、エコーグラム
   (直接音+6反射・遅延正・レベル負・ITDG>0)、AcousticOpts の .ofdx
   ラウンドトリップ (room/occupancy/formula/noise/absorption)。

**カバーされていないもの**: GUI 操作 (CI のオフスクリーンスクリーンショットのみ)、
Runner のプロセス起動、C50/D50/STI の数値検証 (単調性のみ)、Eyring の絶対値、
NC タンジェント法の補間値、レポート出力、I18n。

## 6. Qt 依存範囲 / C++17 依存箇所

### 6.1 Qt 依存

- ビルド必須は Qt6 Widgets のみ (CMakeLists.txt: `find_package(Qt6 REQUIRED
  COMPONENTS Widgets)`、Qt 6.2+ / CMake 3.21+ / C++17)。
- **core/ を含む全ソースが Qt ヘッダに依存**する: core は
  QString/QVector/QObject、io は QFile/QJson*、kernel は QProcess、tests も
  QCoreApplication/QTemporaryFile。selftest ターゲットも Qt6::Widgets にリンク。
- Qt 非依存のファイルは存在しない。

### 6.2 C++17 依存箇所 (grep による実測)

- `CMakeLists.txt:8` — `set(CMAKE_CXX_STANDARD 17)` + `REQUIRED ON` (全ターゲット共通)。
- `src/MainWindow.cpp:107` — 構造化束縛
  `for (const auto &[code, label] : std::initializer_list<std::pair<...>>{...})`
  (言語メニュー構築)。**アプリコード中の C++17 言語機能はこの1箇所のみ**。
- Qt6 ヘッダ自体が C++17 を強制する: 実測で Qt 6.4.2 の `qglobal.h:106` が
  `-std=c++14` に対し `#error "Qt requires a C++17 compiler"` を出す。
- 以下は grep の結果 **不使用**: `if constexpr` / `std::string_view` /
  `std::optional` / `std::variant` / `std::any` / `std::filesystem` /
  `std::clamp` / `[[nodiscard]]` / `[[maybe_unused]]` / `[[fallthrough]]` /
  inline 変数 / ネスト名前空間定義 (`namespace a::b`) / if/switch 初期化文 /
  fold expression / `std::apply` / `std::invoke`。

## 7. C++14 コア追加時の制約と後方互換性リスク

### 7.1 C++14 コアの制約

1. **Qt ヘッダを一切 include できない** (Qt6 は C++17 必須、§6.2 で実測)。
   C++14 準拠のコアは `std::vector` / `std::string` / `double` 配列だけで書き、
   既存 core とは Qt型⇔std型の薄い変換層で接続する必要がある。
2. CMake の `CMAKE_CXX_STANDARD 17` はグローバルなので、C++14 準拠を機械的に
   保証するには新規コアを別ライブラリターゲットに分離し、そのターゲットだけ
   `CXX_STANDARD 14` を設定する (Qt にリンクしない前提)。C++17 でビルドしつつ
   「C++14 の範囲で書く」だけなら現行構成のままでよいが、逸脱検知はできない。
3. 例外・RTTI・スレッドの前提は既存コードに合わせる (既存 core は例外を投げず
   戻り値 + `err` 文字列で通知するスタイル)。
4. M_PI は `<cmath>` の POSIX 拡張に依存 (RoomAcoustics.cpp:103 で使用中。
   MSVC は `/utf-8 /W3` のみで `_USE_MATH_DEFINES` 未定義だが Qt ヘッダ経由で
   通っている点に注意 — 新規 Qt 非依存コアでは自前定義が安全)。

### 7.2 後方互換性リスク

1. **.ofd は不可侵**: 本家カーネル完全互換 + 24サンプルのラウンドトリップが
   selftest で固定されている。音響拡張は必ず .ofdx 側に置く (既存設計判断②)。
2. **.ofdx は「追加キーのみ」なら安全**: ロードは欠落キー=既定値・未知キー=無視
   なので、新キー追加は旧バージョンとの双方向互換を保つ。既存キーの改名・削除・
   型変更、`AbsorptionRow::Role` の enum 値並び替え、`alpha`(6帯域)/
   `noise_levels`(7帯域) の要素数変更は旧ファイルを壊す。`schemaVersion` は
   現在読み捨てで分岐に使われていない。
3. **selftest のチェック総数 1560 は変動する**: テスト追加自体は CI 上安全
   (exit code のみ判定) だが、基準記録 (baseline) の数値は更新が必要になる。
   既存 1560 チェックを1件も落とさないことが受け入れ条件。
4. **AcousticOpts への フィールド追加**は既定メンバ初期化子を付ければ既存
   コードに影響しない。`Project::clear()` の既定バジェットを変える場合は
   selftest の `testRoomAcoustics` (absorption[0].area 等を直接触る) に注意。
5. **UI**: 音響ドメインのタブ追加は `MainWindow::onDomainChanged()` の
   Acoustic 分岐と I18n キー追加が必要。既存2タブの並び・文言を変えると CI の
   スクリーンショットスモーク (--domain acoustic) の見た目が変わる (機械判定は
   ファイル生成のみなので致命ではない)。
6. **Qt バージョン下限**: README は Qt 6.2+、CI 実測は Linux 6.4.2 /
   Windows 6.8 LTS。下限は Linux の 6.4.2 — 6.5 以降の API は使用不可。
7. **Runner**: 音響用の新カーネルを増やす場合は `Kernel` enum・binary prefix・
   HOME 環境変数の3点セット (Runner.cpp `kernelPrefix`/`resolveBinary`) を拡張
   する設計になっている。既存 enum 値の意味変更は RunConfig 経由の呼び出し元
   (MainWindow::currentRunConfig) を壊す。
