---
paths:
  - "src/io/**"
  - "src/core/**"
  - "tests/**"
---

# .ofd / .ofdx フォーマット ルール

- `.ofd` は本家 OpenFDTD (`sol/input_data.c` / `post/post_data.c`) 完全互換。
  キーの追加・変更・削除は禁止。GUI が知らないキーは `Project::extraLines()` に
  保持し、保存時にそのまま書き戻す (ラウンドトリップ保証)。
- 拡張ドメイン (光 / 音響 / 水中 / tidy3d) の設定は同 basename の `.ofdx`
  (JSON サイドカー) へ。 本家カーネルはこれを無視するので下位互換 100%。
- `tests/data/*.ofd` は本家サンプルそのもの — 絶対に書き換えない。
- I/O を触ったら必ず `./build/ofdx_selftest` (24 files, 0 failures) を実行。
