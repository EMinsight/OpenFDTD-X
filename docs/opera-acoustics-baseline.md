# オペラ歌手向け室内音響分析 — フェーズ0 基準記録 (baseline)

記録日: 2026-07-16
対象コミット: `19f751c` (branch `claude/opera-acoustics` = origin/main)

本書は機能追加前の「壊してはならない」動作基準の実測記録である。
**以後の全フェーズにおいて、この基準テストが通らなくなる変更を行わない。**

## 1. 実測環境

| 項目 | 値 |
|---|---|
| OS | Ubuntu 24.04.4 LTS (Linux) |
| コンパイラ | GCC 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) |
| CMake | 3.28.3 |
| Qt | 6.4.2 (qt6-base-dev 6.4.2+dfsg-21.1build5) |
| ビルド設定 | 既定 (USE_HDF5=OFF, USE_LIBIGL=OFF, BUILD_TESTS=ON), C++17 |

## 2. ビルド — 成功

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

- 実測結果: 成功 (`openfdtd_x` と `ofdx_selftest` の2ターゲット、警告設定は
  `-Wall -Wextra`)。

## 3. 自己テスト — 全件成功

```bash
./build/ofdx_selftest
```

実測出力:

```
24 files loaded, 1560 checks, 0 failures
```

exit code = 0。

- **24 files loaded**: `tests/data/*.ofd` の OpenFDTD サンプル24ファイル全件を
  ロード → シリアライズ → 再パースし構造完全一致を確認 (.ofd ラウンドトリップ)。
- **1560 checks / 0 failures**: 上記ラウンドトリップ比較に加え、
  ボクセライザ・ガラスカタログ (Sellmeier/AGF/CSV)・室内音響
  (Sabine 既知値 / Eyring / Barron 単調性 / NC / エコーグラム /
  AcousticOpts の .ofdx ラウンドトリップ) を含む。

### .ofd/.ofdx ラウンドトリップの位置づけ

- `.ofd` ラウンドトリップは selftest が `tests/data/` のサンプル
  **24ファイルで検証済み** (本家 OpenFDTD カーネル互換形式の保証)。
- `.ofdx` (JSON サイドカー) は `testRoomAcoustics` 内で AcousticOpts の
  save→load ラウンドトリップ (room寸法/占有率/式/騒音/吸音バジェット) を検証済み。

## 4. GUI スモーク — 成功

```bash
QT_QPA_PLATFORM=offscreen ./build/openfdtd_x --help
```

- 実測結果: **exit 0**。Usage / オプション一覧
  (`[file]`, `--lang`, `--domain`, `--screenshot`, `--left-tab`) を出力して終了。
- 参考: CI (.github/workflows/ci.yml) はさらにオフスクリーンで
  `--domain optical/acoustic --screenshot` によるスクリーンショット生成を
  スモークテストしている (Linux job)。Windows job は Qt 6.8 LTS / MSVC で
  ビルド + selftest。

## 5. 基準の運用ルール

1. `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j` が
   常に成功すること (Qt 6.4.2 / GCC 13, Ubuntu 24.04 で確認済み)。
2. `./build/ofdx_selftest` が常に **0 failures / exit 0** であること。
   既存 1560 チェックは1件も失敗させない (テスト追加によりチェック総数が
   増えるのは可。その際は本書の数値を更新する)。
3. `QT_QPA_PLATFORM=offscreen ./build/openfdtd_x --help` が exit 0 であること。
4. `.ofd` 形式 (本家互換) の読み書きを変更しない。音響拡張は `.ofdx` の
   追加キーのみで行い、既存キーの改名・削除・型変更をしない。
5. **この基準テストが通らなくなる変更を行わない。** 通らなくなった場合は
   その変更をリバートし、原因を解消してから再適用する。
