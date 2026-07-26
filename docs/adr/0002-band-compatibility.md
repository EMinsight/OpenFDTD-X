# ADR-0002: 既存 6 帯域 (alpha[6]) の不変維持と詳細帯域の別管理

## Status

Accepted (2026-07-16) — 実装済み (`BandFilter.h` の `BandSet` 列挙、
`.ofdx` の `opera_analysis.band_mode`)。

## Context

- 既存の統計推定 (`src/core/RoomAcoustics`) と吸音バジェット
  (`AbsorptionRow::alpha[6]`) は 125/250/500/1k/2k/4k Hz の
  **6 帯域固定**である。この 6 要素配列は `.ofdx` の `absorption[].alpha`
  として永続化され、`noise_levels` (7 要素) とともに selftest の
  ラウンドトリップで固定されている。要素数の変更は旧ファイルを壊す
  (フェーズ0 調査 §7.2)。
- 一方、オペラ歌手向け分析では歌手フォルマント帯域
  (2.0–2.5k / 2.5–3.15k / 3.15–4.0k) や 1/3 オクターブなど、6 帯域より
  細かい分析帯域が必要 (要求 §4.2)。
- 検討した案:
  - **案1**: 既存 `alpha[6]` は不変。分析帯域は C++14 コアの
    `BandSet` 列挙 (Compat6 / FullBandOnly / Octave63To8k /
    ThirdOctave100To5k / SingerFormant) として定義し、`.ofdx` には
    `opera_analysis.band_mode` (int) として**別管理**で保存する。
  - 案2: `alpha` を可変長配列に拡張し帯域数を統一する —
    旧ファイル・selftest・既存 UI (吸音バジェット表) をすべて壊す。
  - 案3: `alpha_extended` 等の並列配列を吸音行に追加する — 実測 RIR
    分析には吸音率入力は不要であり、使わないデータ構造を複雑化する。

## Decision

**案1 を採用する。**

1. `AbsorptionRow::alpha[6]` / `noise_levels[7]` / `kBandHz[6]` は
   一切変更しない (統計推定系の帯域は 6 帯域のまま)。
2. 実測/シミュレーション RIR の分析帯域は `BandSet` 列挙で表現し、
   帯域定義 (`Band {label, centerHz, lowHz, highHz, fullBand}`) は
   `makeBands(BandSet)` がコード内で生成する (永続化しない)。
3. `.ofdx` には選択値のみ `acoustic.opera_analysis.band_mode` (int:
   0=Compat6, 1=1oct, 2=1/3oct, 3=SingerFormant) として保存する。
   enum 値の並び替え・挿入は禁止 (追加は末尾のみ)。
4. `BandSet::Compat6` は既存 6 帯域と同一中心周波数 (オクターブ幅) と
   し、統計推定との帯域対応表示を可能にする。

## Consequences

- (+) 旧 `.ofdx` / selftest / 既存タブに影響ゼロ (追加キーのみ)。
- (+) 帯域セットの追加はコード (`makeBands`) と enum 値 1 つの追加で
  済み、ファイル形式改訂が不要。
- (+) 統計推定 (6 帯域) と実測分析 (任意帯域) の帯域数不一致が
  データモデル上で衝突しない (別オブジェクト)。
- (−) 統計推定と実測分析を同一帯域軸で直接比較できるのは
  Compat6 選択時のみ。他の帯域セットでは比較表示に帯域変換
  (近傍帯域へのマッピング) が必要になる。
- (−) `band_mode` が int のため、値の意味はドキュメント
  (`docs/opera-acoustics-file-format.md` §2) と enum 定義の同期に依存
  する。将来キーを増やす場合は文字列 enum も検討する。
