---
description: Headless GUI smoke — screenshot every domain and show the images
---

ヘッドレスで GUI スモークテストを実行してください。$ARGUMENTS が空なら
4 ドメイン全部、指定があればそのドメインのみ:

```bash
export QT_QPA_PLATFORM=offscreen
for d in em optical acoustic underwater; do
  ./build/openfdtd_x tests/data/dipole.ofd --domain "$d" \
    --screenshot "/tmp/gui-$d.png"
done
```

生成された PNG を Read で開いて確認し、モック
(claude.ai/design「OpenFDTD対応」) からの乖離があれば列挙すること。
