---
description: Configure + build + run ofdx_selftest, then report failures with fixes
---

リリースビルドと自己テストを実行して結果を報告してください:

1. `cmake -B build -DCMAKE_BUILD_TYPE=Release` (初回のみ必要)
2. `cmake --build build -j`
3. `./build/ofdx_selftest`

ビルドエラーが出た場合は、新規ソースの CMakeLists.txt 登録漏れ
(GUI_SOURCES / CORE_SOURCES) をまず疑うこと。selftest が fail した場合は
`.ofd` ラウンドトリップの差分を表示して原因キーを特定すること。
