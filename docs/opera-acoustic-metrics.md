# 音響指標の計算定義 (実装準拠)

本書は `src/acoustics/core/` の**実装そのもの**を仕様として記述する
(AcousticMetrics.{h,cpp} / SchroederDecay.{h,cpp} / NoiseFloorEstimator.cpp、
歌声分析は VocalAnalyzer.{h,cpp} — §10)。
実装と本書が食い違う場合は実装を読み、本書を直すこと。
検証結果は `docs/opera-acoustics-validation.md`。

§1–§9 は RIR 分析 (系統 A/B)、§10 は歌声信号分析 (系統 C、フェーズ3)。

## 0. 共通の前提

- 入力: 単一チャンネル RIR `h[i]` (i = 0..N−1)、サンプリング周波数 fs。
- **時間原点**: `computeAcousticMetrics(rir, fs, directIndex)` は
  `rir[directIndex]` を t = 0 として直接音以降を切り出す
  (ISO 3382-1 は直接音到来を原点とする)。直接音位置は
  `DirectSoundDetector` が決める。
- エネルギーの数値下限: `kEnergyFloor = 1e-30` (10·log10 → −300 dB)。
  振幅の下限は 1e-15 (20·log10 → −300 dB)。
- 各指標は `MetricValue { value, valid, quality, warning }` で返る。
  quality は Valid / Warning (参考値) / Invalid (算出不可) の 3 値。

## 1. Schroeder 減衰カーブ (二乗後方積分 + Chu ノイズ補正)

出典: Schroeder (1965)。ノイズ補正は Chu (1978) の末尾ノイズエネルギー
減算に相当。

1. ノイズパワー推定: 末尾区間 (既定: 全長の 10%、最小 256 サンプル。
   入力が短い場合は全体を使い warning) の RMS から
   `p̂ = noiseRms²` (`estimateNoiseFloor`)。
2. 後方積分: `E(i) = Σ_{k=i}^{N-1} h[k]²`。
3. ノイズ補正 (`noiseCompensation = true` のとき。指標計算の既定は **on**):
   `E(i) ← max(0, E(i) − p̂·(N−i))`。
4. 正規化 dB 化: `L(i) = 10·log10(E(i)/E(0))`、比の下限 1e-30 で
   クランプ (−300 dB)。`L(0) = 0`。
5. **分析終了点** (`analysisEndIndex`): 窓幅 5 ms (既定) の移動平均
   包絡線 (x² の平均) が、包絡線最大位置より後で初めて
   `p̂ × 10^(margin/10)` (margin 既定 10 dB) を下回る点。見つからなければ
   末尾。回帰はこの点より手前のみで行う (Lundeby 法の簡略版)。
6. 減衰カーブ基準ノイズフロア:
   `noiseFloorDb = 10·log10((p̂ + 1e-30)/maxSmooth)` (maxSmooth は
   平滑化包絡線の最大値)。指数減衰では包絡線と Schroeder カーブが同じ
   傾きで減衰するため、この値を各残響指標の評価下限判定に使う。

無効条件 (`SchroederResult.valid = false`): 空入力 / fs ≤ 0 /
補正後の全エネルギー E(0) ≤ 0。

## 2. 減衰カーブの線形回帰 (`regressDecaySegment`)

区間 [startDb, endDb] (下記各指標の値) について:

- 開始点 i0: `L(i) ≤ startDb` となる最初の i。
  終了点 i1: `L(i) ≤ endDb` となる最初の i。いずれも
  `analysisEndIndex` より手前に存在しなければ **invalid**
  (「ノイズフロア到達前に評価下限に達しない」)。
- 最小二乗直線 `L ≈ slope·t + intercept` (t = i/fs [s])。点数 < 4 は
  invalid。
- 決定係数: `R² = 1 − SSE/Syy` (Syy = 0 なら 1)。
- 傾きの標準誤差: `SE(slope) = sqrt(SSE/(n−2)/Sxx)` [dB/s]。
- 結果には実際に使った区間の開始/終了レベル (startDb/endDb 実測値) が
  入る。

## 3. 残響指標 EDT / T20 / T30

共通式: `RT = −60 / slope` [s] (slope [dB/s] は §2 の回帰の傾き)。

| 指標 | 回帰区間 | 外挿倍率 | 出典 |
|---|---|---|---|
| EDT | 0 〜 −10 dB | ×6 (−60/−10) | ISO 3382-1 A.2.2 |
| T20 | −5 〜 −25 dB | ×3 (−60/−20) | ISO 3382-1 6.3 |
| T30 | −5 〜 −35 dB | ×2 (−60/−30) | ISO 3382-1 6.3 |

無効条件 (`valid = false`、順に判定):

1. **動的範囲不足**: `noiseFloorDb > endDb − validityMarginDb`
   (マージン既定 10 dB)。つまりノイズフロアが EDT: −20 dB /
   T20: −35 dB / T30: −45 dB より高いと算出不可。warning に
   「insufficient dynamic range (noise floor … > required …)」。
2. 回帰失敗 (§2 の各理由)。
3. `slope ≥ −1e-9` (減衰していない)。

品質低下 (`quality = Warning`、値は返す = 参考値):
`R² < rSquaredWarningThreshold` (既定 0.95) のとき
「low regression quality (R² = …)」。

パイプライン全体の動的範囲要件 (`RirAnalyzer`): 入力全体の
`peakDb − noiseFloorDb` が `minDynamicRangeDb` (コア既定 10 dB、
GUI 設定既定 35 dB) 未満なら分析自体をエラー
(`InsufficientDynamicRange`)、`warnDynamicRangeDb` (既定 30 dB) 未満は
警告付きで続行。

## 4. 明瞭度 C50 / C80 (ISO 3382-1 A.2.4)

境界サンプル: `b_te = floor(te·fs + 0.5)`、早期 = [0, b)、後期 = [b, N)
(直接音を 0 とした切り出し後の添字)。

```
C_te = 10·log10( Σ_{i=0}^{b-1} h[i]² / Σ_{i=b}^{N-1} h[i]² )   [dB]
te = 50 ms (C50) / 80 ms (C80)
```

無効条件: 全エネルギー ≤ 1e-30 (「no energy after direct sound」) /
信号長 N ≤ b_te (「signal shorter than 50/80 ms after direct」) /
後期エネルギー ≤ 1e-30 (「no late energy (ratio undefined)」)。

## 5. Definition D50 (ISO 3382-1 A.2.3)

```
D50 = Σ_{i=0}^{b50-1} h[i]² / Σ_{i=0}^{N-1} h[i]²    [0..1]
```

無効条件: N ≤ b50 / 全エネルギー ≤ 1e-30。
(後期エネルギーが 0 でも D50 は定義でき、値 1.0 になる — テスト済み。)

## 6. 重心時間 Ts (ISO 3382-1 A.2.5)

```
Ts = Σ_i (i/fs)·h[i]² / Σ_i h[i]²    [s]   (i = 0..N−1, 直接音 = t 0)
```

無効条件: 全エネルギー ≤ 1e-30。
注意: 積分上限は理論の ∞ ではなく**測定信号の末尾**である
(仮定 `docs/opera-acoustics-assumptions.md` §13)。

## 7. Early/Late エネルギー比 (50 ms / 80 ms)

```
EL_te = Σ_{i<b_te} h[i]² / Σ_{i≥b_te} h[i]²    (線形比, 無次元)
```

C50/C80 の線形版 (`C_te = 10·log10(EL_te)`)。無効条件は C50/C80 と同じ。

## 8. 帯域別指標 (BandFilter)

- 4 次バターワース帯域通過 IIR (双一次変換 + プリワーピング、零点
  z = +1 ×2 / z = −1 ×2、中心周波数で振幅 1 正規化)。
- `zeroPhaseFiltering = true` (既定) で前後方向フィルタ (filtfilt 相当、
  ゼロ位相)。このとき振幅特性は 2 乗され**実効 8 次**。
- 帯域上端がナイキスト (fs/2) 以上のときは `FilterDesignFailed` →
  その帯域の全指標が算出不可 (帯域単位の warning)。
- 帯域セット: Compat6 (125…4k オクターブ) / FullBandOnly /
  Octave63To8k / ThirdOctave100To5k (IEC 61260 公称中心) /
  SingerFormant (2.0-2.5k / 2.5-3.15k / 3.15-4.0k / 2.0-4.0k)。

## 9. 単位・表示レンジまとめ

| 指標 | 単位 | 典型レンジ (表示目安) |
|---|---|---|
| EDT / T20 / T30 | s | 0.3 – 4.0 |
| C50 | dB | −10 – +10 |
| C80 | dB | −5 – +15 |
| D50 | 無次元 (0..1) | 0 – 1 |
| Ts | s (UI は ms 表示可) | 0 – 0.4 |
| Early/Late | 線形比 | 0 – ∞ (log 表示推奨) |
| ノイズフロア / ピーク | dBFS | −120 – 0 |
| 反射相対レベル | dB (直接音比) | −60 – 0 |

## 10. 歌声信号分析指標 (VocalAnalyzer — フェーズ3)

対象: 歌唱 WAV (`opera_analysis.voice_file`)。**単一声・無伴奏**を前提と
する (仮定 `docs/opera-acoustics-assumptions.md` §18)。RIR 分析と同じく
C++14 コア (`ofdx_acoustic_core`) 内の `VocalAnalyzer` が計算し、結果は
`MetricValue` (§0) で返る。多チャンネル入力は RIR と同じ
`channel_mode` 規則で 1 チャンネル化する。

### 10.1 フレーム分割 (F0 系の共通前提)

- フレーム長 40 ms、ホップ 10 ms (fs からサンプル数に換算、切り捨て)。
- フレーム RMS が RMS ゲート未満のフレームは**無音扱い** (F0 計算対象外)。
- F0 系指標 (F0 統計・ピッチ安定性・ビブラート・HNR・倍音) は
  **有声フレームのみ**から計算する。

### 10.2 F0 (基本周波数) — YIN 法

出典: de Cheveigné & Kawahara (2002)。採用理由と代替比較は
`docs/adr/0006-vocal-f0-yin.md`。

1. フレームごとに差分関数
   `d(τ) = Σ_j (x[j] − x[j+τ])²` を計算する。
2. 累積平均正規化差分関数 (CMNDF):
   `d'(0) = 1`、`d'(τ) = d(τ) / [ (1/τ)·Σ_{k=1}^{τ} d(k) ]`。
3. 探索範囲 [τ_min, τ_max] (下記の声種別 F0 範囲を周期に換算) 内で、
   `d'(τ) < 0.15` (CMNDF 閾値) を満たす最初の極小を採用する。
   閾値を満たす τ が無ければそのフレームは**無声**。
4. **無声判定**: `min d'(τ) > 0.15`、またはフレーム RMS が
   RMS ゲート未満 (§10.1)。無声フレームの F0 は出力しない
   (0 Hz 等のダミー値で埋めない)。

声種別探索範囲 (`voice_type` は**探索範囲の限定のみ**に使う —
ADR-0006。診断的結論には使わない):

| voice_type | 声種 | F0 探索範囲 [Hz] |
|---|---|---|
| 0 | Soprano | 220 – 1400 |
| 1 | Mezzo-soprano | 180 – 1100 |
| 2 | Alto | 160 – 900 |
| 3 | Tenor | 110 – 700 |
| 4 | Baritone | 90 – 500 |
| 5 | Bass | 70 – 400 |
| 6 | Unknown (既定) | 60 – 1500 |

出力: F0 時系列 [Hz] (フレーム時刻付き)、有声率 [%]、F0 中央値 [Hz]。
無効条件: 有声フレーム数 0 (「no voiced frames detected」)。

### 10.3 ピッチ安定性

有声フレームの F0 を cent に変換し
(`cent_i = 1200·log2(F0_i / median(F0))`)、**中央値からの偏差の RMS**
[cent] を返す。ビブラートによる周期変動も含む値である (デトレンドは
しない — ビブラートと分離した安定性はスコープ外)。
無効条件: 有声フレーム数 < 2。

### 10.4 ビブラート (rate / depth)

1. 有声区間の F0 軌跡 (cent 系列、§10.3) から **250 ms 移動平均**を
   減算してデトレンドする (音程の緩やかな変化・ポルタメントを除去)。
2. デトレンド後系列のスペクトルから **3 – 9 Hz** の範囲で最大ピークを
   探索する。
3. rate = ピーク周波数 [Hz]、depth = その変調成分の振幅 [cent]
   (片振幅。±depth cent の揺れに相当)。

無効条件: 有声の連続区間が短くレート分解能が得られない場合 /
3–9 Hz にピークが検出できない場合 (「no vibrato detected」— これは
異常ではなく事実の報告である)。3–9 Hz を仮定とする根拠は
`docs/opera-acoustics-assumptions.md` §19。

### 10.5 LTAS (長時間平均スペクトル)

Welch 法: FFT 長 4096、Hann 窓、50% オーバーラップ、セグメントごとの
パワースペクトルを平均。出力は dB (未校正時は相対 dB、フルスケール
基準)。周波数分解能は fs/4096 (48 kHz で ≈ 11.7 Hz)。
無効条件: 入力長 < 4096 サンプル (「signal shorter than one FFT
frame」)。

### 10.6 スペクトル重心

```
SC = Σ_k f_k·P(f_k) / Σ_k P(f_k)    [Hz]
```

P(f) は LTAS (§10.5) のパワースペクトル。「音色の明るさ」の
一次指標として表示する。無効条件: LTAS が無効 / 全パワー ≤ 1e-30。

### 10.7 HNR (調波対雑音比)

出典: Boersma (1993) の自己相関法。フレームごと (§10.1 の有声
フレーム) に:

1. 窓補正済み正規化自己相関 `r'(τ)` を計算 (信号の自己相関を窓関数の
   自己相関で除算)。
2. F0 に対応する遅れ τ₀ 近傍の最大値 `r'_max` から
   `HNR = 10·log10( r'_max / (1 − r'_max) )` [dB]。

出力はフレーム別 HNR の**中央値** [dB]。無効条件: 有声フレーム数 0 /
`r'_max ≥ 1` となる数値異常フレームは除外 (全フレーム除外なら invalid)。

### 10.8 倍音レベル H1–H8

LTAS (§10.5) 上で、F0 中央値 (§10.2) の整数倍 k·F0 (k = 1..8) の
近傍 (±半音相当) のピークレベル [dB] を抽出する。倍音がナイキストを
超える次数、および F0 が無効な場合は当該次数を invalid にする。
表示は H1 基準の相対レベル (H1 = 0 dB) を推奨。

### 10.9 帯域エネルギーと歌手フォルマント比

LTAS の帯域積分エネルギー [dB]:

| 帯域 | 範囲 [kHz] |
|---|---|
| SingerFormant 下 | 2.0 – 2.5 |
| SingerFormant 中 | 2.5 – 3.15 |
| SingerFormant 上 | 3.15 – 4.0 |
| SingerFormant 全体 | 2.0 – 4.0 |

帯域境界は RIR 分析の `BandSet::SingerFormant` (§8) と同一。

**歌手フォルマント比** (singer's formant ratio, Sundberg 1974):

```
SFR = 10·log10( E(2–4 kHz) / E(0–2 kHz) )    [dB]
```

E は LTAS のパワー帯域積分。無効条件: いずれかの帯域が fs/2 を超える
(fs < 8 kHz の入力) / E(0–2k) ≤ 1e-30。

### 10.10 SPL (絶対音圧レベル)

`CalibrationState = Absolute` (校正オフセット既知) の場合のみ、
RMS レベル + `calibrationOffsetDb` を dB SPL として返す。それ以外は
`valid = false` (RIR 分析の絶対 SPL 系と同じ規則 — 仮定 §2)。

### 10.11 単位・表示レンジまとめ (歌声)

| 指標 | 単位 | 典型レンジ (表示目安) |
|---|---|---|
| F0 中央値 | Hz | 60 – 1500 (声種依存) |
| 有声率 | % | 0 – 100 |
| ピッチ安定性 | cent (RMS) | 0 – 200 |
| ビブラート rate | Hz | 3 – 9 |
| ビブラート depth | cent (片振幅) | 0 – 300 |
| LTAS | dB (相対) | −120 – 0 |
| スペクトル重心 | Hz | 500 – 5000 |
| HNR | dB | 0 – 40 |
| H1–H8 | dB (H1 基準) | −60 – 0 |
| 歌手フォルマント比 | dB | −30 – +5 |
| SPL | dB SPL (Absolute 校正時のみ) | 40 – 120 |

## 11. 出典

- ISO 3382-1:2009 — Acoustics — Measurement of room acoustic
  parameters — Part 1: Performance spaces (6.3 残響時間、Annex A.2:
  EDT/D50/C50/C80/Ts の定義)。
- M. R. Schroeder, "New Method of Measuring Reverberation Time",
  J. Acoust. Soc. Am. 37, 409–412 (1965) — 二乗後方積分。
- W. T. Chu, "Comparison of reverberation measurements using
  Schroeder's impulse method and decay-curve averaging method",
  J. Acoust. Soc. Am. 63(5), 1444–1450 (1978) — 定常ノイズエネルギー
  減算による補正。
- (参考) A. Lundeby et al., "Uncertainties of Measurements in Room
  Acoustics", Acustica 81 (1995) — 分析終了点判定の考え方 (本実装は
  簡略版であり完全な反復法ではない)。
- A. de Cheveigné, H. Kawahara, "YIN, a fundamental frequency
  estimator for speech and music", J. Acoust. Soc. Am. 111(4),
  1917–1930 (2002) — F0 推定 (CMNDF・絶対閾値法)。
- P. Boersma, "Accurate short-term analysis of the fundamental
  frequency and the harmonics-to-noise ratio of a sampled sound",
  IFA Proceedings 17, 97–110 (1993) — 自己相関法 HNR。
- J. Sundberg, "Articulatory interpretation of the 'singing
  formant'", J. Acoust. Soc. Am. 55(4), 838–844 (1974) — 歌手
  フォルマント (2–4 kHz) と帯域エネルギー比。
- P. D. Welch, "The use of fast Fourier transform for the estimation
  of power spectra", IEEE Trans. Audio Electroacoust. 15(2), 70–73
  (1967) — LTAS の平均化法。

規格文書・論文本文は転載せず、定義から自前実装している
(`docs/licensing-review.md` §3)。
