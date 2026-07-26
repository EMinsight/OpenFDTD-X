# 人工 RIR による検証記録 (実測結果)

記録日: 2026-07-16。対象: `src/acoustics/` 音響コア +
`tests/acoustics/` 全テスト。
環境: Linux (Ubuntu 24.04), GCC 13.3.0, `g++ -std=c++14 -O2`
(コアは C++14 のままコンパイル可能であることも同時に確認)。
CMake 統合後は `ctest` の `acoustics.*` として同一テストが実行される。

以下の数値はすべて**テスト実行の実出力からの転記**である
(テストは決定的: シード固定 LCG の人工 RIR、外部データ不要。
生成条件は `tests/acoustic_data/metadata.json` と
`tests/acoustics/test_common.h` に記録)。

## 1. Schroeder 減衰カーブ + 回帰 (test_schroeder — 125 checks PASS)

理想指数減衰 `x(t) = e^(−6.91 t/RT)` の減衰カーブ傾きは厳密に
−60/RT [dB/s]。許容: 傾き ±1%、R² > 0.999。

| ケース | 実測傾き [dB/s] | 期待値 | 誤差 | R² |
|---|---|---|---|---|
| RT=0.5 s (補正なし) | −120.0390 | −120.0 | **0.032%** | **1.000000** |
| RT=1.0 s (補正なし) | −60.0195 | −60.0 | 0.032% | 1.000000 |
| RT=2.0 s (補正なし) | −30.0097 | −30.0 | 0.032% | 1.000000 |
| RT=1.0 s (Chu 補正あり) | −60.0195 | −60.0 | 0.033% | 1.000000 |

その他: 回帰区間が確保できない場合 (−200 dB まで届かない短い信号) の
invalid 化、空入力・start<end 不正引数の invalid 化、−50 dB ノイズ付加時に
分析終了点が末尾より手前に置かれノイズフロア推定が −60〜−30 dB に
収まることを確認。

## 2. EDT / T20 / T30 精度 (test_reverberation — 88 checks PASS)

人工 RIR (直接音デルタ + 指数減衰白色雑音)、許容 **±5%**。

ノイズなし:

| RT [s] | EDT | T20 | T30 |
|---|---|---|---|
| 0.5 | 0.5022 (+0.43%) | 0.4996 (−0.08%) | 0.4987 (−0.26%) |
| 1.0 | 0.9934 (−0.66%) | 0.9910 (−0.90%) | 0.9971 (−0.29%) |
| 2.0 | 1.9952 (−0.24%) | 1.9937 (−0.32%) | 2.0029 (+0.15%) |
| 3.0 | 2.9908 (−0.31%) | 2.9937 (−0.21%) | 2.9962 (−0.13%) |

ノイズフロア −60 dBFS 付加:

| RT [s] | EDT | T20 | T30 |
|---|---|---|---|
| 0.5 | 0.4938 (−1.24%) | 0.5006 (+0.12%) | 0.5002 (+0.05%) |
| 1.0 | 0.9931 (−0.69%) | 0.9998 (−0.02%) | 1.0013 (+0.13%) |
| 2.0 | 2.0104 (+0.52%) | 1.9906 (−0.47%) | 1.9950 (−0.25%) |
| 3.0 | 2.9909 (−0.30%) | 3.0088 (+0.29%) | 3.0040 (+0.13%) |

最大誤差 (全 16 ケース): **EDT ≤ 1.24% / T20 ≤ 0.90% / T30 ≤ 0.29%**
— いずれも許容 ±5% を大きく下回る。

**動的範囲不足の invalid 動作** (RT=2.0 s、ノイズフロア −35 dBFS):
T20 / T30 とも `valid = false`、quality = Invalid、理由文字列は実出力で

```
T30: insufficient dynamic range (noise floor -24.9 dB > required -45.0 dB = -35 dB - 10 dB margin)
```

**帯域分割後** (Compat6・ゼロ位相、RT=1.0 s、1k/2k 帯で確認):

| 帯域 | T30 | 誤差 |
|---|---|---|
| 1 kHz | 1.0202 | **+2.02%** |
| 2 kHz | 0.9902 | −0.98% |

帯域分割後は統計ゆらぎが増えるが許容 ±5% 内。

統合パイプライン (`RirAnalyzer`): 全帯域 (FullBandOnly) で T20/T30 が
±5% 内、非有限値 0、クリッピング非検出、動的範囲 > 40 dB、総合品質が
Invalid でないこと、未校正時に絶対 SPL が invalid になること、
Absolute 校正 (offset 94 dB) 時のみ valid になることを確認。

## 3. C50 / C80 / D50 / Ts の理論値一致 (test_clarity — 19 checks PASS)

解析的に計算できる 2 反射 RIR (直接音 1.0、60 ms に 0.5、100 ms に 0.4)。

| 指標 | 実測 | 理論値 | 許容 |
|---|---|---|---|
| C50 | 3.8722 dB | 3.8722 dB | ±0.2 dB |
| C80 | 8.9279 dB | 8.9279 dB | ±0.2 dB |
| D50 | 0.70922 | 0.70922 | ±0.01 |
| Ts | 0.02199 s | 0.02199 s | ±1 ms |

表示桁 (4〜5 桁) で理論値に**完全一致**。Early/Late 比も相対 ±1% で一致。
境界系: 早期のみの信号で C50/C80 が「後期エネルギーなし」で invalid、
D50 = 1.0、Ts = 6 ms (理論値)。80 ms 未満の信号で C80 invalid。

## 4. 直接音検出 (test_direct_sound — 25 checks PASS)

- 単一デルタの位置・時刻・振幅・Valid 品質。
- 最大ピーク (1.0 @120 ms) に先行する強いピーク (0.4 @50 ms、−7.96 dB・
  70 ms 先行) → 先行側を直接音に採用し Warning。
- 弱い先行成分 (−26 dB) は不採用、1 ms 未満の先行も不採用 (警告なし)。
- EnvelopeThreshold 法の立ち上がり点検出、MovingRmsThreshold 法の
  デルタ位置検出。
- エラー系: 空入力 / 全無音 → found = false。
- 人工 RIR (デルタ + 減衰雑音 RMS 0.3) でデルタ位置を正しく検出。

## 5. 反射検出・時間区分集計 (test_reflections — 44 checks PASS)

既知の 4 反射 (遅延 10/50/120/250 ms、レベル −6/−12/−20/−30 dB):

- 実出力: `4 reflections detected, delays: 10.00 50.00 120.00 250.00 ms`
  — 偽検出ゼロ、遅延 ±1 ms、レベル ±1 dB、信頼度 > 0.5。
- 時間区分 [0,20)/[20,80)/[80,200)/[200,∞) ms に 1 件ずつ集計され、
  各区分のエネルギー比が 10^(level/10) と相対 ±20% で一致。
- −40 dB より弱い反射の非検出、`maxReflections` による強い順の制限、
  ノイズフロア (+8 dB マージン) に埋もれた反射 (−55 dB) の非検出と
  明確な反射 (−12 dB) の検出。

## 6. WAV 入出力 (test_wav — 52 checks PASS)

書き込み→読み込みラウンドトリップの最大誤差:

| 形式 | ch / fs | 実測許容 (すべて PASS) |
|---|---|---|
| PCM16 (Writer) | mono 48k / stereo 44.1k | ≤ 1e-4 |
| float32 (Writer) | mono 96k / stereo 12345 Hz | ≤ 1e-6 |
| PCM24 (生バイト組立) | mono 48k / stereo 44.1k | ≤ 3e-7 |
| PCM32 (生バイト組立) | mono 48k / stereo 32k | ≤ 2e-9 |

odd サイズ LIST チャンクのパディング、奇数サンプル数 (24bit mono で
data チャンクが奇数長)、非 WAV データ → `UnsupportedFormat`、
存在しないファイル → `FileNotFound` を確認。

## 7. C API (test_c_api — 39 checks PASS、純 C99 でコンパイル)

実出力: `EDT=0.9928 T20=0.9989 T30=0.9991 C50=-0.05 C80=2.99 D50=0.497 Ts=0.0729`
(RT=1.0 s の人工 RIR)。理論値 D50 = 1−10^(−0.3) = 0.4988 (±0.05)、
Ts = RT/13.82 = 0.0724 s (±0.02) と一致。NULL 引数 7 系統・短入力の
INVALID_AUDIO・struct_size/api_version 不一致の拒否・
`ofdx_ac_last_error` の非 NULL 保証・destroy(NULL) 安全性を確認。

## 8. テストケース一覧 (何を検証したか)

| 観点 | ケース | テスト |
|---|---|---|
| ノイズなし | RT 0.5/1/2/3 s | test_reverberation |
| 低ノイズ (−60 dB) | RT 0.5/1/2/3 s | test_reverberation |
| 高ノイズ (−35 dB) | 動的範囲不足 → invalid + 理由 | test_reverberation |
| 反射 | 既知 4 反射 / 弱反射除外 / 上限数 / ノイズ埋没 | test_reflections, test_clarity |
| クリッピング | クリーン信号で非検出 (偽陽性なし) を統合テストで確認 | test_reverberation |
| 無音・空入力 | 直接音 found=false / analyze が EmptyInput 系エラー | test_direct_sound, test_schroeder |
| 短尺入力 | 0.05 s 未満 → InputTooShort / 80 ms 未満 → C80 invalid / 回帰区間不足 → invalid | test_reverberation, test_clarity, test_schroeder |
| 非有限値 | NaN 混入 → NonFiniteSample エラー | test_reverberation |
| 校正 | Uncalibrated で絶対 SPL invalid / Absolute で valid | test_reverberation, (C API は広帯域指標のみ) |
| 帯域分割 | Compat6 ゼロ位相で T30 ±5% 内 | test_reverberation |
| ファイル I/O | PCM16/24/32/float32、odd チャンク、異常系 | test_wav |
| C ABI | 純 C コンパイル、NULL、struct_size/api_version | test_c_api |

## 9. 集計と結論

| テスト | checks | failures |
|---|---|---|
| test_schroeder | 125 | 0 |
| test_reverberation | 88 | 0 |
| test_clarity | 19 | 0 |
| test_direct_sound | 25 | 0 |
| test_reflections | 44 | 0 |
| test_wav | 52 | 0 |
| test_c_api | 39 | 0 |
| **合計** | **392** | **0** |

既存 baseline (`ofdx_selftest`: 24 files loaded, 1560 checks,
0 failures) は本コア追加の影響を受けない (コアは既存コードに
リンクされるのみで既存経路を変更しない)。

**未達成項目なし** — 定義した許容基準 (残響指標 ±5%、Schroeder 傾き
±1%、明瞭度系の理論値一致、無効条件の invalid 動作) をすべて満たした。
残る検証上の弱点 (クリッピング陽性系の単体テスト、48 kHz 以外での帯域
フィルタ精度) は「未達成」ではなく追加テスト候補として
`docs/opera-acoustics-development-status.md` §3 に記録する。
