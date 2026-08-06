# 水中音響 FDTD ソルバー (新規リポジトリ) の仕様

OpenFDTD-X の水中音響ドメインに **近距離・低周波用の波動ソルバー**を足すための
仕様書。新規リポジトリのチャットセッションへそのまま貼れる形で書いてある。

---

## 0. この文書の使い方

以下「1. 依頼文」を新しいリポジトリのセッションに貼る。「2. 補足資料」は
GUI 側 (OpenFDTD-X) の既存の実装状況で、必要になったら参照する。

---

## 1. 依頼文 (ここから貼る)

### 作るもの

**2 次元軸対称 (r-z) の水中音響 FDTD ソルバー**を新規に作ってください。
バイナリ名は `ofdx_uw_fdtd`、C または C++ で、外部ライブラリ依存なし
(CMake + OpenMP のみ任意)。GUI (OpenFDTD-X) から `QProcess` で起動される
処理カーネルです。

**既存の `ofdx_acoustic_fdtd` (OpenAcoustics) を拡張するのではなく、別物として
作ってください。** あちらは室内音響用で、座標系・境界・媒質・音源・出力・
検証のすべてが違います:

| | OpenAcoustics (室内) | 本ソルバー (水中) |
|---|---|---|
| 座標系 | 3D 直交 (直方体) | **2D 軸対称 (r-z)**、`1/r` 項あり |
| 媒質 | 等速 `c = 343` 一定 | **`c(z)` の音速プロファイル** |
| 境界 | 6 面の剛/インピーダンス壁 | 海面 `p=0` + 距離依存の海底 + **PML** |
| 音源 | 広帯域ガウシアン微分 (RIR 用) | ソナー周波数のトーンバースト |
| 出力 | `rir.wav` (受音点の時系列) | **TL 場** + 受波点時系列 |
| 検証 | Sabine / ISO 3382 | Pekeris ノーマルモード / Lloyd ミラー |

共通なのは leapfrog の p-v 交互配置更新だけです。

### 入出力 — **BELLHOP と同じ形式にすること**

これが最重要の設計指針です。GUI 側は既に BELLHOP 用の入出力を実装済みなので、
同じ形式を採れば **GUI の変更がほぼゼロ**になり、かつ **同じ入力ファイルで
bellhopcxx と結果を直接比較できる**ようになります。

```
ofdx_uw_fdtd <FILEROOT>
  読む: <FILEROOT>.env          BELLHOP の ENVFIL (必須)
        <FILEROOT>.bty          BELLHOP の BTYFIL (あれば海底地形)
        <FILEROOT>.uwfdtd.json  FDTD 固有の設定 (無ければ既定値)
  書く: <FILEROOT>.shd          BELLHOP の SHDFIL 互換 (TL 場)
        <FILEROOT>.prt          テキストログ
        <FILEROOT>_rx.wav       受波点の時系列 (任意、設定で ON)
  標準出力: "progress a/b" 形式の進捗行 (GUI が進捗バーに使う)
```

#### `.env` から読むもの

BELLHOP マニュアルの ENVFIL 仕様に従ってください。実例:

```
'OpenFDTD-X underwater (sand bottom)'	! TITLE
200			! FREQ (Hz)
1			! NMEDIA
'CVW'			! SSPOPT
0 0.0 2000		! NMESH, SIGMA, DEPTH of bottom (m)
0 1500 /
2000 1520 /
'A~' 0.0                ! 底面 — 2 文字目 '~' があれば .bty を読む
2000 1650 0.0 1.9 0.5 /	! 深度 c_p c_s ρ[g/cm3] α[dB/λ]
1			! NSD
100 /			! SD (m)
101			! NRD
0.0 2000 /		! RD (m)
201			! NR
0.0 20 /		! R (km)
'CG'			! RunType (1 文字目=モード, 2 文字目=ビーム種別)
0			! NBEAMS
-45 45 /		! ALPHA1,2 (deg)
0.0 2100 21		! STEP (m), ZBOX (m), RBOX (km)
```

- **FREQ** — 音源の中心周波数
- **SSP 点列** — `c(z)`。区分線形で格子へ展開する
- **底面行** — `c_p` / 密度 / 減衰 `α [dB/λ]`。2 文字目が `~` か `*` なら
  `<FILEROOT>.bty` を読む (無ければ SSP 最深点の平坦海底)
- **NSD / SD** — 音源深度 (1 点だけ対応で可)
- **NRD / RD, NR / R** — 出力する受波器格子。**この格子で `.shd` を書く**
- **RunType の 2 文字目・NBEAMS・ALPHA** — レイ法固有なので FDTD では使わない。
  **黙って無視せず、起動時に `.prt` と標準出力へ warning を出すこと**
- **ZBOX / RBOX** — 計算領域の上限。格子の外周に PML を置く

`.bty` は 1 行目が補間種別 (`'L'`)、2 行目が点数、以降が `距離[km] 水深[m]`。
距離は単調増加。全ての水深は `.env` の底深度以下 (BELLHOP と同じ制約)。

#### `.uwfdtd.json` (FDTD 固有、省略可)

```json
{
  "cells_per_wavelength": 10,
  "pml_cells": 20,
  "duration_s": 0,          // 0 = 自動 (最遠受波器の到達 + 余裕)
  "source": "tone_burst",   // tone_burst | ricker | gaussian_derivative
  "cycles": 5,              // トーンバーストの波数
  "write_rx_wav": false,    // 受波点時系列を WAV で書くか
  "rx_wav_fs": 48000,
  "snapshot_interval": 0    // >0 なら伝搬スナップショットも出す
}
```

**省略時は既定値で動くこと。** 不正値はエラーで止め、黙って既定値へ倒さない。

#### `.shd` (SHDFIL) の書き方

固定長レコードのバイナリ。レイアウトは bellhopcuda `src/mode/tl.cpp` の
`WriteHeader()` / `GetRecNum()` が正です:

```
レコード長 = 先頭 int32 LRecl [4 byte 語数] × 4 byte
rec 0 : int32 LRecl, char[80] Title
rec 1 : char[10] PlotType ("rectilin  ")
rec 2 : int32 Nfreq, Ntheta, NSx, NSy, NSz, NRz, NRr, 続けて freq0, atten
rec 3 : 周波数配列
rec 4 : 方位配列
rec 5,6 : Sx, Sy  (TL では両端 2 値)
rec 7 : Sz
rec 8 : Rz
rec 9 : Rr
rec 10 + (((isx*NSy + isy)*Ntheta + itheta)*NSz + isz)*NRz + irz
      : complex<float32> × NRr
```

2D では `Ntheta = NSx = NSy = 1` なので、実質 `rec 10 + irz` に受波器深度ごとの
複素音圧が NRr 個並びます。**GUI は複素音圧の絶対値から
`TL = -20 log10|p|` を計算する**ので、`p` は BELLHOP と同じ正規化
(自由音場 1 m で `|p| = 1`) にしてください。FDTD の生の場は音源注入の
仕方で決まる任意単位なので、**自由音場の解析解 `1/(4πr)` で校正して
から書き出すこと**。校正に使った係数は `.prt` に出してください。

### 物理仕様

#### 支配方程式 (2D 軸対称)

```
∂v_r/∂t = -(1/ρ) ∂p/∂r
∂v_z/∂t = -(1/ρ) ∂p/∂z
∂p/∂t   = -ρ c(z)² ( ∂v_r/∂r + v_r/r + ∂v_z/∂z )
```

`r = 0` の軸上は `v_r/r → ∂v_r/∂r` (ロピタル) として扱う。
標準的な交互配置格子 + leapfrog、CFL は `dt ≤ dx / (c_max √2)`。

#### 格子

`dx = c_min / (cells_per_wavelength × FREQ)`。既定 10 セル/波長。
`.prt` に **実際の dx・格子サイズ・想定メモリ・有効上限周波数**を必ず出す
(利用者が「この設定で何 Hz まで信用できるか」を判断できるように)。

#### 境界条件

| 境界 | 扱い |
|---|---|
| 海面 (z = 0) | `p = 0` (pressure release) |
| 海底 | `.bty` の断面に沿った流体-流体境界。密度・音速・減衰は `.env` の底面行。階段近似で可 |
| 最大距離側・下端 | **PML** (既定 20 セル)。反射率を `.prt` に出す |
| 軸 (r = 0) | 軸対称条件 |

#### 音源

`.env` の `SD` の深度、`r = 0` に点音源。既定は FREQ の 5 波トーンバースト
(ハン窓)。`.uwfdtd.json` で Ricker / ガウシアン微分も選べるようにする。

### 検証 (これが通って初めて完成)

「落ちない」「それらしい絵が出る」は合格条件ではありません。**解析解との
定量比較**を必ず付けてください。

| 検証 | 解析解 | 許容 |
|---|---|---|
| 自由音場の球面拡散 | `p = 1/(4πr)` | 1 波長以上離れた点で ±2% |
| Lloyd ミラー干渉 | 海面のみ反射、`p = 1/(4πr₁) − 1/(4πr₂)` (鏡像) | 干渉の節・腹の位置が ±λ/10 |
| 等速 Pekeris 導波路 | ノーマルモード和 (モード数は分散方程式から) | TL が ±1 dB |
| BELLHOP との相互検証 | 同じ `.env` を bellhopcxx で実行 | 距離非依存・高周波側で TL が ±3 dB |

最後の 1 つが **入出力を BELLHOP に合わせる最大の理由**です。同じ入力ファイルを
両方に食わせるだけで比較できます (FDTD は低周波側、Bellhop は高周波側が
得意なので、両方が成り立つ中間帯域で比べる)。

検証は `data/sample/` にケースを置き、CI (3 OS) で自動実行してください。

### 計算規模の目安 (仕様に書いておく)

`c = 1500 m/s`、`dx = λ/10`、2D:

| 周波数 | 領域 | セル数 | 実行性 |
|---|---|---|---|
| 200 Hz | 1 km × 500 m | 0.9 M | 数秒 |
| 200 Hz | 20 km × 2 km | 71 M | 時間単位 |
| 1 kHz | 1 km × 500 m | 22 M | 分〜時間 |
| 10 kHz | 1 km × 500 m | 2.2 G (27 GB) | **不可能** |

**このソルバーは近距離・低周波用です。** 長距離は Bellhop の担当で、
GUI 側でハイブリッド (近傍 FDTD + 遠方 Bellhop) に繋ぐ計画です。
`.prt` に「この設定はセル数が多すぎる」旨の警告を出す閾値を設けてください。

### 移植性 (OpenFDTD 系の共通規約)

- **C99 の VLA 禁止** (MSVC)。`malloc` + 明示インデックスのフラット配列
- **OpenMP のループ変数は `int`** (MSVC の OpenMP 2.0 は 64bit を受け付けない)。
  ループ内でのインデックス宣言も不可 (C3015) — 事前宣言する
- libm リンクは CMake の `MATH_LIB` 変数経由
- MSVC は `/utf-8`、`_USE_MATH_DEFINES`
- CI は Linux / macOS / Windows (MSVC + Ninja) の 3 OS

### 段階

1. **第 1 段** — 等速・平坦海底・PML なし (剛壁) で球面拡散の検証を通す
2. **第 2 段** — PML + 海面 `p=0` で Lloyd ミラーを通す
3. **第 3 段** — `c(z)` + 流体海底で Pekeris を通す
4. **第 4 段** — `.bty` の距離依存海底 + `.shd` 出力 + Bellhop 相互検証
5. **第 5 段** — 受波点時系列の WAV 出力、スナップショット

各段で検証ケースと CI を足してから次へ進んでください。

## 1 の依頼文はここまで

---

## 2. 補足資料 — GUI (OpenFDTD-X) 側の現状

### 既に実装済みで、そのまま使えるもの

| GUI 側 | 内容 |
|---|---|
| `src/io/BellhopIO` | `UnderwaterOpts` → `.env` / `.bty` の生成 |
| `src/io/BathymetryIO` | 緯度経度 + 方位 → 大圏サンプリング → 地形断面 |
| `src/io/ShdReader` | SHDFIL の読み取り (`TL = -20 log10\|p\|`) |
| `src/io/ArrReader` | BELLHOP の到達 (`.arr`) → 受信インパルス応答 → WAV |
| `src/tabs/UnderwaterTab` | TL 断面ヒートマップ、受信波形の書き出し |
| `src/CenterPane` | 結果ペインの 2D 断面へ TL を表示 |
| `src/kernel/Runner` | `bellhopcxx <FILEROOT>` 起動、`.env`/`.bty` の書き出し |

**`.shd` を BELLHOP 互換で書けば、この全部がそのまま動きます。**

### GUI 側で足す必要があるもの (このソルバーができてから)

1. `UnderwaterOpts` にソルバー選択を追加 (現状 `UnderwaterTab` の
   ソルバー選択は UI だけで、`Runner::kernelForProject()` は水中を
   常に `Kernel::Bellhop` へ倒している)
2. `.uwfdtd.json` サイドカーの生成
3. カーネルの探索 (環境変数 `OPENUWFDTD_HOME` 等) と warning の受け取り
4. ハイブリッド (近傍 FDTD + 遠方 Bellhop) の距離分割とレベル整合
   — 室内音響のハイブリッド RIR (`docs/adr/0008-hybrid-rir.md`) と同じ構造

### ライセンス上の注意

**bellhopcuda は GPL-3 です。** GUI は `QProcess` で起動するだけなので現状は
問題ありませんが、**新ソルバーのコードを bellhopcuda のリポジトリへ入れると
GPL が及びます**。新規リポジトリとして独立させてください。BELLHOP の
入出力**書式**に合わせること自体は、書式が公開仕様なので制約になりません。
