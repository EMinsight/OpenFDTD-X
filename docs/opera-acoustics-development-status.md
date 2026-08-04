# 開発状況 (フェーズ0–5)

記録日: 2026-07-16。対象 branch: `claude/opera-acoustics`
(フェーズ0/1 はコミット 093bcc4 済み、フェーズ2 は作業ツリー上で実装中、
フェーズ3/4/5 は作業ツリー上で実装済み)。

## 1. フェーズ進捗表

| フェーズ | 内容 | 状態 | 根拠 (実在する成果物) |
|---|---|---|---|
| 0 | 事前調査・基準記録・ライセンス調査 | **完了** | `docs/opera-acoustics-existing-analysis.md` / `opera-acoustics-baseline.md` (selftest 1560 checks 実測) / `licensing-review.md` |
| 1 | C++14 音響分析コア (RIR パイプライン + WAV I/O + テスト) | **完了** | `src/acoustics/core/` (RirAnalyzer ほか 9 モジュール)、`src/acoustics/io/` (WavReader/Writer)、`tests/acoustics/` 6 テスト + 生成器。検証 392 checks / 0 failures (`docs/opera-acoustics-validation.md`) |
| 2 | GUI 統合 (C API / Qt アダプター / RirAnalysisTab / .ofdx / CI) | **実装中** | 済: CMake ターゲット分離 (`ofdx_acoustic_core` C++14 固定 / `ofdx_acoustic_c_api`)、C API 実装 + 純 C テスト (`test_c_api.c`)、`QtAcousticAdapter`、`OperaAcousticSettings`、`.ofdx opera_analysis` save/load、CI への ctest 追加。未: 下記 §2 |
| 3 | 歌声信号分析 (VocalAnalyzer: YIN F0 / ピッチ安定性 / ビブラート / LTAS / 重心 / HNR / H1–H8 / 帯域エネルギー / 歌手フォルマント比 / フォルマント F1–F3 (LPC — 負債 #14)) | **実装済み** | `src/acoustics/core/` VocalAnalyzer + FormantEstimator (C++14)、`VocalAnalysisTab`。定義: `docs/opera-acoustic-metrics.md` §10、ADR-0006 |
| 4 | 可聴化 (ConvolutionEngine: 自前 radix-2 FFT + Overlap-Add、A/B = ドライ/ウェット WAV 書き出し) | **実装済み** | `src/acoustics/core/` Fft / ConvolutionEngine、`AuralizationTab`、`.ofdx` `opera_analysis.auralization`。ADR-0005。旧フェーズ4 計画分 (ST 系 / G 絶対値 / 校正ワークフロー / レポート出力) は残課題へ移動 (§3 負債 #10) |
| 5 | 外部音響ソルバー連携 (AcousticRunner QProcess 疎結合・AcousticBackend 5 値・出力契約・モックソルバー CI・AcousticFdtdEstimator) | **実装済み** | `src/acoustics/qt/AcousticRunner`、`AcousticSolverTab`、`.ofdx` `opera_analysis.solver`、CI モックソルバー統合テスト。ADR-0004 / ADR-0007 |

## 2. フェーズ2 の残作業 (実装中 → 完了の条件)

1. **RirAnalysisTab の実装**: `src/tabs/` に新規タブ (WAV 選択、設定
   編集、分析実行、指標テーブル (品質 3 値表示)、波形 / Schroeder
   減衰カーブの MiniPlot 表示、反射時間区分の表示)。
2. **MainWindow 統合**: 音響ドメインの `onDomainChanged()` 分岐に
   タブ追加 (既存 2 タブの並び・文言は不変 — baseline 運用ルール)。
3. **I18n キー追加**: 新 UI 文言の日英対訳。
4. **selftest 拡張**: `opera_analysis` の .ofdx ラウンドトリップ
   チェック追加 (チェック総数が 1560 から増えるため baseline 文書の
   数値更新もセット)。
5. **schemaVersion "1.1" 書き出し**: 現行 save は "1.0" のまま
   (`docs/opera-acoustics-file-format.md` §1)。
6. ~~**calibration_offset_db の追加** (負債 #1)~~ → **完了**
   (`OperaAcousticSettings::calibrationOffsetDb` / `.ofdx`
   `calibration_offset_db` / `RirAnalysisTab` の入力欄 /
   `QtAcousticAdapter` の Absolute 限定ゲート)。

## 3. 既知の技術的負債

| # | 内容 | 影響 | 対応方針 |
|---|---|---|---|
| 1 | ~~`calibrationOffsetDb` が `OperaAcousticSettings` / `.ofdx` / `QtAcousticAdapter::toAnalyzerConfig` に無い~~ **解消済み** | (解消前: GUI 経由で Absolute を選んでもオフセット 0 dB で絶対 SPL が dBFS のままだった) | **完了**: `OperaAcousticSettings::calibrationOffsetDb` (既定 0.0) + `.ofdx` `calibration_offset_db` (欠落時 0.0) + `RirAnalysisTab` の入力欄 (Absolute 時のみ有効) + `QtAcousticAdapter::toAnalyzerConfig` / `toVocalConfig` で **Absolute 以外は 0 を渡す**ゲート。selftest に往復 / 旧ファイル既定 / ゲート規則のチェックを追加 |
| 2 | save が `schemaVersion: "1.0"` を書く | 1.1 ファイルの識別ができない (実害は小: 読み込みはキー有無判定) | フェーズ2 残作業 |
| 3 | selftest に `opera_analysis` ラウンドトリップ未追加 | 永続化の回帰をテストが検出しない | フェーズ2 残作業 |
| 4 | `.ofdx` の未知キーが保存時に消える (既知フィールド再構成方式) | 他ツールとの .ofdx 共有で相手のキーを失う | 次期対応 (ADR-0003: メモリ保持 + 保存時マージ。今回は非実装) |
| 5 | C API が広帯域 7 指標のみ (帯域別結果・反射リスト・warning 文字列・Early/Late は未公開) | 外部カーネルからの利用範囲が限定的 | 需要が出た時点で `struct_size`/`api_version` 拡張規約に従い追加 |
| 6 | `QtAcousticAdapter::readWav` がファイル全体を QByteArray に読む | 巨大 WAV (長時間・高 fs) でメモリピーク | ストリーミング読みは必要になるまで保留 |
| 7 | ~~帯域フィルタの数値精度検証が fs=48 kHz のみ (低い fc/fs 比 — 例 63 Hz @ 96 kHz — の IIR 係数精度が未検証)~~ **解消済み** | (解消前: 高 fs 入力の低帯域で精度低下の可能性が未評価だった) | **完了**: `tests/acoustics/test_bandfilter.cpp` (563 checks) を追加。48/96 kHz の 63・125・250 Hz オクターブ帯と 100 Hz 1/3 オクターブ帯について、設計された離散フィルタの**厳密な期待振幅特性** (プリワープ後のバターワース解析解) と正弦波応答を直接比較。**精度低下は無かった** — 解析解との相対誤差は最悪 3.7e-6 (63 Hz @ 96 kHz, fc/fs = 6.6e-4)、48 kHz でも 96 kHz でも通過帯域利得 1.000000±4e-6 / エッジ −3.0103 dB / ±1 oct −13.274 dB / ±2 oct −28.99 dB。インパルス応答の末尾減衰 (≤1e-21·peak) と DC 漏れ (≤1e-13) で係数の悪条件も否定 |
| 8 | ~~クリッピング検出の陽性系ユニットテストが無い (統合テストの陰性確認のみ)~~ **解消済み** | (解消前: 検出ロジック回帰の検出漏れ) | **完了**: `tests/acoustics/test_clipping.cpp` (60 checks) を追加。`RirAnalyzer` (連続区間検出) と `ConvolutionEngine` (サンプル数カウント) の**両方**について陽性系を検証。境界値は実装仕様に合わせる — 判定は**厳密な `>`** なので閾値ちょうど (0.999) は非検出、1 ulp 上と 1.0 は検出。連続数が `clipRunLength` (既定 3) 未満は非検出、`clippedRunCount` は区間数 (サンプル数ではない)、判定は DC 除去の**前**に生サンプルで行う |
| 9 | MiniPlot に対話機能 (ズーム/カーソル) が無い (フェーズ0 調査 §2.3) | 減衰カーブの詳細確認がしづらい | フェーズ2 では現状機能で表示し、拡張は別課題 |
| 10 | ST 系 / G / 実測 STI / 校正ワークフロー拡充が未実装 (旧フェーズ4 計画分)。~~レポート出力~~ は**解消済み** | 舞台支援・声の届きの定量値が不足 | **レポート出力は完了**: `src/acoustics/qt/AcousticReportBuilder` (Widget 非依存) が実行済みの RIR 分析 + 歌声分析を一括 HTML (自己完結・外部参照なし) / CSV (`source` 列で系統を区別) にまとめる。入口はファイルメニュー「音響レポート出力…」(`MainWindow::exportAcousticReport`)。分析は再実行せず、未実行の系統は**「未実行」と明示** (絶対規則 5)。出力は現在時刻を含まず決定的 (同一入力 → 同一バイト列) で、selftest 19 checks (未実行明示 / 決定性 / HTML エスケープ / 校正オフセットの Absolute 限定) で検証。残り (ST 系 / G / 実測 STI / 校正ワークフロー) は別課題として継続 (要求 §3.2 / §4.3、仮定 §1/§2) |
| 11 | 可聴化のリアルタイム再生が無い (A/B はドライ/ウェット WAV 書き出しのみ) | 切替比較に外部プレイヤーが必要 | Partitioned Convolution + 音声出力を将来課題として記録 (ADR-0005) |
| 12 | リサンプリング未実装 (ドライと RIR の fs 不一致は明示エラー) | fs の異なる素材は外部ツールで変換が必要 | 高品質リサンプラーの追加は需要が出た時点で検討 (仮定 §21) |
| 13 | 実音響ソルバーが存在しない (CI はモックのみ) | ExternalFDTD / ExternalGeometric は契約準拠ソルバーを別途用意して初めて機能する | ソルバー本体は別リポジトリで開発 (ADR-0004 / ADR-0007)。GUI 入口は**実装済み**: `AcousticSolverTab` (🔌 音響ソルバ連携) がバックエンド 5 値の選択 (`.ofdx` `opera_analysis.solver.{backend,executable,threads,processes}` に永続化)・探索順どおりの解決結果のライブ表示・`AcousticRunner` の起動/停止/ログ/進捗・`rirReady` → `rirPath` 反映 (RIR 分析への引き渡し) を提供する |
| 14 | ~~フォルマント周波数推定 (F1/F2) が無い~~ **フォルマント推定は解消済み** (声区 (レジスター) 分析は将来課題) | (解消前: 歌手フォルマントは帯域エネルギー比のみで声楽的フィードバックの分解能が限定的だった) | **完了 (フォルマント)**: `src/acoustics/core/FormantEstimator` (LPC — 反エイリアス FIR + 1/5 間引きで内部 fs 9.6 kHz、p = 2 + round(fs/1000) = 12、プリエンファシス 0.97 + ハミング窓、Levinson-Durbin、Durand-Kerner 根 (決定的初期値・乱数不使用))。YIN の有声判定フレームのみ推定し、F ≥ 90 Hz・帯域幅 ≤ 400 Hz の極を昇順に F1/F2/F3、代表値は有効フレームの時間中央値 (MetricValue)。`VocalAnalysisTab` に F1/F2/F3 中央値 + 軌跡 MiniPlot、CSV/JSON 出力に追加。検証は `tests/acoustics/test_formant.cpp` (合成母音 ±10% — `docs/opera-acoustics-validation.md` §12)。診断的結論の禁止 (ADR-0006) は維持 — 共鳴周波数という物理量のみを報告する。**声区分析は引き続き将来課題** |

## 4. 品質基準の現在値

- 既存 baseline: `ofdx_selftest` = 24 files loaded, **4900 checks,
  0 failures** (2026-08-04 更新。減らないこと。`OFDX_BELLHOP_BIN` 設定時は bellhop 統合
  +5 checks、`OFDX_OFD_BIN` 設定時は ofd 統合 +5 checks。実行種別ゲート /
  TPA 入力検証 / 解析解の検証 / 校正オフセットの往復・ゲート検証 /
  一括レポート (負債 #10) / 音響編集エンジン (`src/audio/AudioEditEngine` —
  生成の決定性・K 特性ラウドネス・Schroeder RT・ノイズ除去) /
  音響ソルバー連携設定の往復 (`opera_analysis.solver`) を追加済み)。
- 音響コア: `ctest` の `acoustics.*` **15 テスト / 合計 1292 checks,
  0 failures** (`docs/opera-acoustics-validation.md` §9。負債 #7 / #8 の
  `test_bandfilter` 563 checks・`test_clipping` 60 checks、負債 #14 の
  `test_formant` 72 checks を含む)。
- CI: Linux job に `ctest --test-dir build --output-on-failure`、
  Windows job に `-C Release` + `TMPDIR` 設定を追加済み (作業ツリー)。

### 未実装マーカー棚卸し (2026-08-04)

GUI 全体の `markNotImplemented` / `sampleNote` / `unwiredNote` 全 304 箇所を
棚卸しした結果: **実態と乖離した表示 (stale) は 0 件** (マーカーは全て正確)、
実装可能 128 件、外部依存・カーネル契約未定義等で未実装表示のままが妥当
196 件。実装可能のうち既存インフラの再利用で完結する小規模項目
(防音 Sabine RT60 実計算 / 音源ポーラ図の指向性実計算 / WAV 実読込
プレビュー・外部プレイヤー試聴 / ソルバ領域のモック固定値→実計算置換 /
検証タブの PML 対策ボタン / H5 ループ切替・h5py/Jupyter 生成 / スクリプト
読込・保存 / 散乱の平面波配線 / ツール連携の手動パス / 形状の配置変換・計測 /
光・音響・水中設定の `.ofdx` 永続化) は実装済み。残り (中・大規模 —
音源リストのモデル化、RoomAcoustics の IR 実解析配線、ThinFilm の
スタック計算等) は棚卸し結果を優先度整理のうえ別課題として継続する。

### 光導波路モードソルバ FDE (2026-08-04)

`src/optics/FdeModeSolver` (Qt 非依存 C++17) を新設し、モードソルバ FDE タブの
表示を簡易近似式から実計算へ置き換えた。手法は断面 2D の 5 点差分 + 虚軸伝搬
(シフト逆べき乗) + ADI/Thomas 三重対角解 + Gram-Schmidt デフレーション、
スカラー / 半ベクトル TE・TM 対応。出力は neff / |E|² 分布 / 閉込め係数 Γ /
実効断面積で、ng・群速度分散 D・複屈折 Δn・dneff/dT・dneff/dw・プロセスコーナーは
λ・寸法・温度を変えた複数ソルブの差分から実算出する。材料は
`src/optics/MaterialDispersion` (公刊 Sellmeier + 熱光学係数 dn/dT、出典明記) を
材料Explorer と共用する。

検証は対称 3 層スラブの厳密解 (超越方程式を selftest 側で二分法により独立に解く)
との比較で、dy = 10/5/2.5 nm の誤差比が 3 点とも 0.250 = **2 次収束**を確認
(離散化誤差でありスキームのバグでないことの証明)。実効屈折率法との交差検証、
モード直交性、決定性、Γ の範囲も検査 (計 76 checks)。
既定条件 (Si 450×220 nm / SiO₂ / 1550 nm / TE) で **0.6 秒**、実 GUI での
実測は計算 1.2 秒・掃引 4.3 秒・コーナー 3.6 秒。

**曲げ損失は対象外** (曲がり導波路の漏れモード = 複素 neff 解析が必要) で
固定サンプル + `sampleNote` を維持。伝搬損失 [dB/cm] も実屈折率のみの FDE では
求まらないため「—」表示。モード波源・モード展開モニター・Schematic への
受け渡しは受け側モデルが未実装のため `markNotImplemented` を維持。

### 計算結果の 3D 表示 (2026-08-04)

3D シーン (`Viewport3D`) はプリプロセス表示専用で、結果は 2D 断面でしか
見られなかった。さらに ビュースタイル「+ Field」はモック由来の合成式
(v = sin(8r−0.08t)·exp(−0.6r)) を実結果のように描いており、ダミーである旨の
表示が無かった (絶対規則 5 違反)。以下で解消:

- `Viewport3D::setResultSlice()` を追加し、ソルバ出力の断面を 3D 空間の
  正しい位置・寸法の平面へ描く (jet 着色の画像を 1 回アフィン変換で貼る。
  正射影なので断面の像は厳密にアフィン)。合成式 `drawFieldOverlay()` は削除し、
  結果が無いときは「結果未読込 — 偽の界分布は表示しません」と明示する。
  「+ Rays」も合成レイなので「サンプル表示 — ソルバ結果ではありません」を常時表示
- `H5Reader::ofdGridCoords()` を追加 (新 `/geometry` / 旧 `/metadata` の
  節点座標 [m] を軸ごとにフォールバックして読む)。断面の物理位置はこれで決め、
  座標が取れない経路 (obpm の `/field/Ixz` 等) では 3D へ置かない
- ビューポートに「結果断面を重ねる」トグル。スタイルコンボと二重管理しない
- `H5ViewerTab` の「複数断面同時表示 (3 面ビュー)」を実装
  (XY/XZ/YZ 同時・カラースケール共通・3 面同時にアニメ再生)
- 旧レイアウト (`/data%06d`) を開いたとき、時系列の `E` ではなく同じグループの
  `P` (3D) が自動選択されて再生も 3 面ビューも無効のままになる不具合を修正

未対応: 等値面・ボリュームレンダリング、ev2/ev3 のネイティブ描画、
ParaView/VTK 出力 (いずれも既存のマーカーを維持)。

### 車内音響・屋外騒音の `sampleNote` 解消 (2026-08-04)

固定サンプル値だった 5 箇所を実計算・実データ・「未計算」の明示へ置き換え、
`tabhelp::sampleNote` を両タブから除去した。計算は Qt 非依存コアへ切り出し、
selftest から解析解・規格値と突き合わせている。

- 新規 `src/acoustics/core/RoomModes` (C++14, Qt 非依存) — 剛壁直方体室の
  固有周波数 f = (c/2)·√((nx/L)²+(ny/W)²+(nz/H)²) (Rayleigh 1896 /
  Morse & Ingard 1968)、軸・接線・斜めの分類、ISO 9613-1 の音速。
  `CabinAcousticsTab` の「1次 42Hz / 2次 68Hz / 3次 87Hz」固定表示を、
  入力寸法・室温・上限周波数からの実計算表へ置換
- 新規 `src/acoustics/core/EnvironmentalNoise` (C++14, Qt 非依存) —
  幾何拡散 A_div (ISO 9613-2 §7.1、点音源 20lg d + 11 / 線音源 10lg d + 8)、
  前川チャートの回折減衰 ΔL = 10lg(3+20N) (Maekawa 1968、上限 24 dB)、
  騒音に係る環境基準の基準値 (平成10年環境庁告示第64号)、断面予測と
  等レベル距離の逆算。`OutdoorNoiseTab` の「ΔL = 12.4 dB @ 500Hz」
  「予測 52 dB(A)」「環境基準クリア」の固定表示を、幾何からの実計算と
  告示の基準値との比較判定へ置換 (等高線 SVG も計算結果から描く断面図に)
- 「未計算」に倒した箇所: 車内の耳位置 SPL / AI / Loudness / Sharpness は
  連成解析が未実装で算出根拠が無いため値を「—」とし、何があれば埋まるかを
  表に併記 (校正なし絶対 SPL を出さない — 絶対規則 6)。対策検討の
  効果・重量・コストは空欄の入力セルとし、加算が成り立つ重量のみ合計する
- 未実装のまま残したもの (画面に明示): A_atm / A_gr / A_misc / C_met、
  防音壁の頂部形状 (Y型・枝付き・吸音型) の付加効果、交通量・車速から
  音響パワーを求める発生源モデル (ASJ RTN-Model / CNOSSOS-EU)、
  車室の構造振動+音響連成解析

### 超音波・連成解析・SAR の `sampleNote` 解消 (2026-08-04)

固定サンプル値だった 4 箇所 (`UltrasoundTab` ×2 / `MultiphysicsTab` /
`SarTab`) を実計算・実データ・「未計算」の明示へ置き換え、3 タブから
`tabhelp::sampleNote` を除去した。計算はいずれも Qt 非依存コアへ切り出し、
selftest から解析解・規格値・手計算値と突き合わせている。

- 新規 `src/acoustics/core/FocusedField` (C++14, Qt 非依存) —
  球面集束開口の軸上音圧の厳密閉形式 (O'Neil, JASA 21, 516 (1949))、
  焦点音圧 p = ρc·u0·k·h、強度 I = p²/(2ρc)、べき乗則吸収 α₀f^y、
  −6 dB 幅 1.028λF#、機械指標 MI (IEC 62359)、非線形係数 β = 1+B/2A・
  衝撃形成距離 x_sh = 1/(βεk)・Gol'dberg 数 Γ = 1/(αx_sh)
  (Hamilton & Blackstock 1998)、および媒質の文献値データベース
  (Duck 1990 / IT'IS V4.1 / Krautkrämer 1990)。
  `UltrasoundTab` の「~8 MPa + 非線形域バッジ」固定表示を、開口径・曲率半径・
  周波数・音響出力・選択媒質からの実計算セクションへ置換。媒質表は出典付き
  データベースとして位置づけ直し (y と Z = ρc の列を追加)、選択行を
  `Project::materials()` の ρ・c へ反映するボタンを追加した
- 新規 `src/optics/PlasmaDispersion` (Qt 非依存) — Drude の
  Δn = −ω_p²/(2nω²)・Δα と、Soref & Bennett (IEEE JQE-23, 123 (1987)) の
  c-Si 実測フィット (1.31 / 1.55 μm)。`MultiphysicsTab` の
  「Δn ~ -8.8e-22 × ΔN」固定表示を、ΔN・ΔP・λ・n を入力とする実計算へ置換
  (λ の既定はプロジェクトの光波長帯の中心)。Drude の Δα が直流移動度では
  過小評価になることも画面に明示
- 新規 `src/em/SarMetrics` (Qt 非依存) — SAR = σ|E|²/(2ρ) (IEEE C95.1-2019 /
  IEC 62704-1)、平面波 S = E_rms²/Z0、断熱温度上昇 ΔT = SAR·t/c_p、
  および ICNIRP 2020 / IEEE C95.1-2019 / FCC 47 CFR の基本制限テーブル
  (周波数範囲・平均化質量/時間・出典条項つき)
- 「未計算」に倒した箇所: `SarTab` の評価指標表。ICNIRP/FCC の「適合」を
  無条件表示していたものを、算出値「—」・判定「未評価」とし、指針値の欄だけを
  周波数と曝露区分から規格値で埋める。何を実行すれば埋まるか (ofd の SAR 分布
  + IEC 62704-1 の空間平均) を表の下に明記した。あわせて定義式で計算できる
  「点 SAR 換算」を追加し、点 SAR が 1 g/10 g 空間平均とは別量で適合判定には
  使えないことを画面に明示 (絶対規則 5・6)
- 未実装のまま残したもの (画面に明示): 時間領域の非線形伝搬 (Westervelt /
  KZK)、ビームフォーミング・出力チェックのカーネル反映、FDTD ↔ CHARGE /
  HEAT の連成計算、SAR 分布計算と適合宣言レポート

### 精度検証・ばらつき解析の `sampleNote` 解消 (2026-08-04)

固定サンプル値・乱数合成だった 5 箇所 (`VerificationTab` ×4 /
`ToleranceTab` ×1) を実計算・実データ・「未計算」の明示へ置き換え、両タブから
`tabhelp::sampleNote` を除去した。計算は Qt 非依存コアへ切り出し、
`ofdx_selftest` から解析解・公表式・極限値と直接突き合わせている
(チェック総数 6289 → 6404、+115)。

- 新規 `src/core/FdtdVerification` (Qt 非依存) — メッシュ解像度の計画値
  (セル数 / λ/Δx / 推定メモリ)、吸収境界の設計反射率
  (PML: R(θ)=R₀^cosθ — Berenger, J. Comput. Phys. 114, 185 (1994) 式(26) /
  Taflove & Hagness 3rd ed. §7.7、1 次 Mur: R(θ)=(1−cosθ)/(1+cosθ) —
  Mur, IEEE Trans. EMC-23, 377 (1981) / Engquist & Majda, Math. Comp. 31,
  629 (1977))、Courant 数 S = c·Δt·√(Σ1/Δa²)、および分解能・安定条件・
  境界余裕・波源–観測点距離の判定 (閾値の根拠は Taflove §4.5/§4.7、
  Balanis 4th ed. §2.2.4)。実行ログ (`ofd.log` / `orcwa.log` / `obpm.log`) の
  収束履歴パーサも同居する
- 新規 `src/core/ToleranceStats` (Qt 非依存) — 正規 / 一様 / レイリー分布の
  密度・モーメント・被覆区間 (NIST/SEMATECH e-Handbook §1.3.6.6、
  JCGM 100:2008 (GUM) §4.3.7 の a/√3 と k=1,2,3 の被覆確率)
- 実データにした箇所: `VerificationTab` ③ のエネルギー減衰曲線。**固定シード
  の乱数で合成していた偽の減衰曲線を廃止**し、作業ディレクトリのソルバー
  実行ログから収束履歴 (平均|E| / 平均|H|) を読んで描画、`solver` キーの
  収束判定値を破線で重ねる。未実行時はグラフごと隠して「未実行」と表示する
  (タブ表示時と「⟳ 実行ログを再読込」で読み直す)
- 実判定にした箇所: 自動診断 7 行。λ/Δx・CFL 安定条件・吸収境界の設定・
  観測点が領域内か・波源と観測点の距離・形状と境界の余裕をプロジェクト設定
  から計算する。Δt = 0 (カーネルの自動決定) は「自動」、実行ログが要る
  「収束到達」行は未実行時「未判定」と表示する
- 「未計算」に倒した箇所: ① メッシュ収束表の「結果」「誤差」列 (各解像度での
  ソルバー実行が必要 — 自動収束テストは未実装)、② 反射率の「実測」列
  (設計値と実測値を別列にして混同を避けた)、`ToleranceTab` の歩留まり
  (モンテカルロ未実装)。いずれも何をすれば埋まるかを表の下に明記した
- `ToleranceTab` のばらつき要因表は中心・σ/半幅を編集可能にし、選択した
  入力変数の確率密度と 3σ 相当の被覆区間を実計算で表示する
  (性能 FoM の分布ではないことを画面に明示)。表の保存 (.ofdx) と
  ソルバー連携は未実装のまま注記
- 未実装のまま残したもの (画面に明示): 自動収束テスト、境界反射の実測、
  クロスバリデーション実行、モンテカルロ (歩留まり・感度・ロバスト最適化)

## 5. 次の作業 (優先順)

1. フェーズ2 残作業 §2 の 5 (schemaVersion "1.1" の書き出し — 負債 #2)。
   6 (校正オフセット — 負債 #1) は完了。selftest のラウンドトリップには
   `auralization` / `solver` ネストも含めること。
2. ~~負債 #7 / #8 の追加テスト (96 kHz 帯域フィルタ、クリッピング陽性)~~
   → **完了** (`tests/acoustics/test_bandfilter.cpp` /
   `tests/acoustics/test_clipping.cpp`)。
3. フェーズ2 完了時に baseline 文書のチェック総数を更新し、
   本書のフェーズ表を更新。
4. 残課題の優先度整理: リアルタイム再生 (負債 #11)、リサンプリング
   (負債 #12)、実音響ソルバー (負債 #13)、声区分析 (負債 #14 の残り —
   フォルマント推定は解消済み)、旧フェーズ4 計画分 (負債 #10 の残り —
   レポート出力は解消済み、ST 系 / G / 実測 STI / 校正ワークフローが継続)。
