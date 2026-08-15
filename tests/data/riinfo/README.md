# refractiveindex.info の検証用サンプル

`io/RefractiveIndexDb` (取り込み機能の読み手) を**ネットワーク無しで**検証する
ための実データです。CI はここだけを読み、外部へ通信しません。

## 出所とライセンス

[refractiveindex.info database](https://github.com/polyanskiy/refractiveindex.info-database)
より取得 (ブランチ `main` — 上流の README によると 2026-01-07 に `master` から
改名。取得時点では両者とも同じ内容を返すことを確認済み)。
**CC0 1.0 (パブリックドメイン)** — 各ファイル冒頭にその旨が書かれており、
原文のまま残してあります。

| ファイル | 元 | 内容 | 削ったもの |
|---|---|---|---|
| `catalog-excerpt.yml` | `database/catalog-nk.yml` | shelf / book / page の木 | 大半の項目 (2 つの shelf にまたがる形だけ残した) |
| `Ag-Johnson.yml` | `data/main/Ag/nk/Johnson.yml` | 表形式 (tabulated nk) | データ行を先頭 8 行に切詰 |
| `SiO2-Malitson.yml` | `data/main/SiO2/nk/Malitson.yml` | 式 (formula 1, Sellmeier) | なし (元から小さい) |
| `N-BK7.yml` | `data/specs/schott/optical/N-BK7.yml` | 式 (formula 2) + 表形式の k | データ行を先頭 8 行に切詰 |
| `Air-Ciddor.yml` | `data/other/mixed gases/air/nk/Ciddor.yml` | 式 (formula 6, Gases) | なし |
| `Si-ChandlerHorowitz.yml` | `data/main/Si/nk/Chandler-Horowitz.yml` | 式 (formula 4) + 表形式の k | データ行を先頭 8 行に切詰 |
| `CCl4-Moutzouris.yml` | `data/organic/CCl4 - carbon tetrachloride/nk/Moutzouris.yml` | 式 (formula 3, Polynomial) | なし |

## 何を判定しているか

- カタログの木 (shelf / book / page)、`data:` の相対パス、`data:` の無い
  page を候補にしないこと
- 表形式の読み取り、式の係数と有効範囲
- **式の値が公表値と合うこと** — 溶融石英 n(0.5876/1.0/0.3 µm) と
  N-BK7 n(0.5876/0.6563/0.4861 µm)。式を当てずっぽうで実装していない担保
- `N-BK7.yml` の `PROPERTIES.thermal_dispersion` にある `formula A` を
  DATA の式と取り違えないこと (`DATA:` 節は列 0 の次のキーで終わる)
- 式 3〜9 — 定義は上流同梱の公式仕様書
  `database/doc/Dispersion formulas.pdf` (RefractiveIndex.INFO, 2014-06-29)。
  実データのアンカー: **空気 (formula 6)** は公表の空気屈折率
  n−1 = 2.765e-4 @633 nm / 2.771e-4 @589 nm、**Si (formula 4)** は
  CO2 レーザー波長の定番値 n(10.6 µm) = 3.4179、**CCl4 (formula 3)** は
  CRC の n_D = 1.4601 (測定温度差 dn/dT ≈ −5.8e-4/K 込みで ±0.005)。
  5/7/8/9 は仕様書どおりの厳密な恒等式 (係数の極限・特別な λ) で判定
- `REFERENCES` の平文化 — このフィールドには HTML (`<a href>` / `<i>` / `<b>`) が
  入っており、上流通知 (2026-06) で **Markdown も入る**ことになった。
  タグとリンク先を落として引用そのものだけを残せるかを実データで判定する
