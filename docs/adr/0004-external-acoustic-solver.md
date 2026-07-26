# ADR-0004: 音響 FDTD は既存 ofd (電磁) を流用せず、外部ソルバーを疎結合で連携する

## Status

Accepted (2026-07-16)。フェーズ5 で実装済み — 実装レベルの具体化
(出力契約の確定・AcousticBackend 5 値・モック戦略) は
`docs/adr/0007-acoustic-solver-contract.md` を参照。本 ADR の
Decision 2 の enum 案 ({ None, ExternalSolver }) は ADR-0007 の
5 値 enum で置き換えられた。

## Context

- 既存 Runner は電磁 FDTD カーネル `ofd` (+ RCWA/BPM) を QProcess で
  起動する。**音響専用カーネルは存在せず**、音響ドメインでも
  Kernel::FDTD (= `ofd`) のままで、`AcousticOpts` は `.ofdx` に
  書かれるだけ (本家 `ofd` は `.ofdx` を無視する — フェーズ0 調査 §4)。
- 電磁 FDTD と音響 FDTD は方程式系が異なる (Maxwell のベクトル場 vs
  線形音響のスカラー圧力 + 粒子速度)。境界条件 (PML の定式化、
  インピーダンス境界/吸音率)、材料モデル、音源 (点音源・指向性)、
  安定条件の物理定数もすべて異なる。`.ofd` 入力形式は本家互換のため
  不可侵であり、音響用に意味を上書きすることはできない。
- 実測 RIR 分析パイプライン (`RirAnalyzer`) は既に WAV 入力で完結して
  おり、「ソルバーが RIR を WAV で出せば、実測と同一コードで指標を
  計算できる」構造がある。

## Decision

1. **既存 `ofd` を音響に流用しない。** 音響シミュレーションは
   別バイナリの外部音響ソルバーとし、GUI とは QProcess による
   **疎結合** (ファイル入出力 + 終了コード + stdout 進捗) で連携する。
   ソルバーは同梱でも第三者製でもよい (契約を満たせば差し替え可能)。
2. **AcousticBackend enum (案)**: プロジェクト設定に
   `enum class AcousticBackend { None, ExternalSolver }` を追加する
   (None = 統計推定と実測分析のみ)。Runner 拡張は既存の 3 点セット
   (Kernel enum・binary prefix・HOME 環境変数) の流儀に従う。
3. **バイナリ解決**: ① 環境変数 `OFDX_ACOUSTIC_SOLVER` (絶対パス
   直接指定、最優先 — CI/開発用オーバーライド) → ② 環境変数
   `OPENFDTD_ACOUSTICS_HOME` 配下 → ③ アプリ実行ディレクトリの
   `kernel/` → ④ PATH 委任。Windows では `.exe` 付加
   (既存 `resolveBinary` と同型)。
4. **出力契約** (ソルバーは作業ディレクトリに以下を出力する):
   - `metadata.json` (必須): スキーマ版、ソルバー名/バージョン、格子
     (Δx・セル数)、fs、音源/受音点座標、音速、実行条件。
   - `rir.wav` (必須): 受音点 RIR。`WavReader` 対応形式 (float32 推奨)。
   - `metrics.json` (任意): ソルバー側算出指標。GUI は自前計算
     (RirAnalyzer) を正とし突合表示のみに使う。
   - `solver.log` (必須): 実行ログ。進捗は stdout にも出力し Runner が
     解析する。
   GUI 側は `rir.wav` を実測 RIR と**同一パイプライン**で分析し、結果は
   「シミュレーション RIR」区分で表示する (要求 §2 の 3 区分)。
5. **音響 FDTD の規模見積り式は電磁用と独立に実装する**:
   - 音速 c = 343 m/s (電磁の光速 c₀ とは別定数)。
   - セル寸法 Δx = λ_min/10 = c/(10·f_max)。
   - セル数 = V/Δx³ (V は室容積)。
   - 時間刻み (3D CFL): Δt ≤ Δx/(c·√3)。ステップ数 = 模擬時間/Δt
     (模擬時間は想定 RT 以上、目安 1.5·RT + マージン)。
   既存ステータスバーの電磁用 cells/mem/Δt(CFL) 表示ロジックを
   共用・改変せず、音響用見積りを別関数として持つ。

## Consequences

- (+) `.ofd` 互換・既存 Runner・selftest に一切手を入れずに音響
  シミュレーションを導入できる (baseline 不変)。
- (+) ソルバーの実装言語・ライセンス・開発サイクルが GUI から独立。
  C API + C++14 コア (ADR-0001) をソルバー側が再利用して指標を
  自己検証することもできる。
- (+) シミュレーション結果と実測の指標計算が同一コード経路になり、
  「定義差による食い違い」が構造的に発生しない。
- (−) プロセス境界のオーバーヘッドとエラー処理 (異常終了・契約違反
  ファイル・部分出力) の実装が必要。契約違反は「metadata.json 不在 /
  rir.wav 読込失敗」を明確なエラーメッセージで表示する方針。
- (−) 出力契約のスキーマ管理という新しい互換性面が増える。
  `metadata.json` に `contract_version` を持たせ、`.ofdx` と同じ
  「追加キーのみ・未知キー無視」規則を適用する。
- (−) f_max を上げると計算量が f_max⁴ で増える (Δx³ × ステップ数)。
  見積り式を実行前に UI 表示し、非現実的な設定を事前警告する。
- 未決事項 (フェーズ5 で確定): 複数受音点の rir.wav 形式 (多チャンネル
  1 ファイル vs 連番)、`metrics.json` の指標命名、進捗行フォーマット。
