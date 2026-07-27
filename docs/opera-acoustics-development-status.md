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
| 3 | 歌声信号分析 (VocalAnalyzer: YIN F0 / ピッチ安定性 / ビブラート / LTAS / 重心 / HNR / H1–H8 / 帯域エネルギー / 歌手フォルマント比) | **実装済み** | `src/acoustics/core/` VocalAnalyzer (C++14)、`VocalAnalysisTab`。定義: `docs/opera-acoustic-metrics.md` §10、ADR-0006 |
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
| 10 | ST 系 / G / 実測 STI / 校正ワークフロー / レポート出力が未実装 (旧フェーズ4 計画分) | 舞台支援・声の届きの定量値が不足 | 別課題として継続 (要求 §3.2 / §4.3、仮定 §1/§2) |
| 11 | 可聴化のリアルタイム再生が無い (A/B はドライ/ウェット WAV 書き出しのみ) | 切替比較に外部プレイヤーが必要 | Partitioned Convolution + 音声出力を将来課題として記録 (ADR-0005) |
| 12 | リサンプリング未実装 (ドライと RIR の fs 不一致は明示エラー) | fs の異なる素材は外部ツールで変換が必要 | 高品質リサンプラーの追加は需要が出た時点で検討 (仮定 §21) |
| 13 | 実音響ソルバーが存在しない (CI はモックのみ) | ExternalFDTD / ExternalGeometric は契約準拠ソルバーを別途用意して初めて機能する | ソルバー本体は別リポジトリで開発 (ADR-0004 / ADR-0007) |
| 14 | 声区 (レジスター) 分析・フォルマント周波数推定 (F1/F2) が無い (歌手フォルマントは帯域エネルギー比のみ) | 声楽的フィードバックの分解能が限定的 | LPC 等による高度化は将来課題 (診断的結論の禁止 — ADR-0006 — は維持) |

## 4. 品質基準の現在値

- 既存 baseline: `ofdx_selftest` = 24 files loaded, **1870 checks,
  0 failures** (減らないこと。実行種別ゲート / TPA 入力検証 /
  解析解の検証 / 校正オフセットの往復・ゲート検証を追加済み)。
- 音響コア: `ctest` の `acoustics.*` **14 テスト / 合計 1220 checks,
  0 failures** (`docs/opera-acoustics-validation.md` §9。負債 #7 / #8 の
  `test_bandfilter` 563 checks・`test_clipping` 60 checks を含む)。
- CI: Linux job に `ctest --test-dir build --output-on-failure`、
  Windows job に `-C Release` + `TMPDIR` 設定を追加済み (作業ツリー)。

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
   (負債 #12)、実音響ソルバー (負債 #13)、声区分析の高度化 (負債 #14)、
   旧フェーズ4 計画分 (負債 #10)。
