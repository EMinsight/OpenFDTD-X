---
description: フルビルド + 全テスト (selftest / ctest) を実行して回帰を確認する
---

OpenFDTD-X の変更を検証する。以下を順に実行し、結果を要約して報告すること。

1. クリーン構成 + ビルド (警告が増えていないか確認):
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j"$(nproc)" 2>&1 | grep -Ei "warning|error" || true
   ```
2. 自己テスト (チェック総数が前回から**減っていない**ことも確認):
   ```bash
   QT_QPA_PLATFORM=offscreen ./build/ofdx_selftest
   ```
3. 音響コアテスト:
   ```bash
   QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
   ```
4. シリアライズ互換 (機能を何も有効化していない Project の .ofd 出力が
   変更前とバイト一致か — selftest 内のバイト一致チェックが通ること)。
5. コミット対象に成果物 (`build/`, `*.gch`, `*.o`) が混ざっていないか
   `git status --short` で確認。

失敗があれば修正してから再実行。全部緑になるまで完了報告しない。
