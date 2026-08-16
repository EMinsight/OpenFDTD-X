# ADR-0010: 複数音源 (multi_source) の契約

## Status

Accepted (2026-08-15、2026-08-16 改訂: Decision 7 で音源ごとの
ゲイン・遅延を追加) — ソルバー側 (OpenAcoustics の `ofdx_acoustic_fdtd` /
`ofdx_acoustic_ga` 両バイナリ) は Decision 7 まで実装・検証済み。GUI 側の
設定 UI (AcousticSolverTab のトグル → `AcousticOpts::multiSource` →
`.ofdx` `acoustic.multi_source`) も実装済み (既定 false ならキー自体を
書かない — 絶対規則 2。selftest がバイト一致往復を検証する)。
Decision 7 の GUI 編集 UI のみ未実装 (Decision 7 の末尾参照)。

## Context

- `.ofd` は `feed` を複数持て、音響ウィザードも「波源 (feed) %1 個」と
  複数波源を数えて表示する。しかし ADR-0007 の契約は複数 feed の意味を
  定めておらず、両ソルバーは feed #1 のみを使い warning を出していた。
- ハイブリッド合成 (ADR-0008 Decision 4) はクロスオーバー帯の
  **バンドエネルギー**で両ソルバーの RIR を整合させる。したがって
  **両ソルバーが同じ音源集合を使うことが整合の前提**で、片方だけ複数音源に
  すると帯域整合が壊れる。契約は両ソルバーで対称に決める必要がある。
- 用途: 複数スピーカー (PA、ステレオ対、合唱の粗いモデル) が**同一信号**を
  再生する場合の可聴化。畳み込みは 1 本の RIR に対して行うので、
  「全音源が同じ dry 信号を同時に再生する」ことに相当する。

## Decision

1. **`.ofdx` に `acoustic.multi_source` (bool、既定 false) を新設する。**
   false = 従来どおり feed #1 のみ + warning (完全後方互換 — 既存入力の
   結果は 1 bit も変わらない)。true = **全 feed を強度 1 で t = 0 に同時
   発火し、`rir.wav` は重ね合わせ**。キーは `acoustic.ga` ではなく
   `acoustic` 直下に置く (両ソルバーが読むため。FDTD 側は `acoustic.ga` を
   丸ごと読み飛ばす規約なので、そこに置くと FDTD が読めない)。
2. **ファイル契約 (ADR-0007) は不変。** 1 受音点 = 1 RIR ファイルのまま。
   `rir.wav` の意味が「全音源の同時発火に対する応答」に広がるだけで、
   ファイル名・WAV 形式・progress 書式は変わらない。したがってモック
   (契約の実行可能な仕様) の変更は不要。
3. **metadata.json は追加キーのみ**: `multi_source` (bool) と `sources`
   (使用した全音源座標の配列 `[{ "pos_m": [x,y,z] }, ...]`)。既存の
   `source` オブジェクトは **feed #1 のまま**残す — GUI が読む
   `sigma_s` / `t0_s` / `fmax_hz` は全音源で共通のパルス属性なので、
   複数音源でもそのまま有効。
4. **振幅規約: 1/N 正規化はしない** (強度 1 の音源の物理的な重ね合わせ)。
   幾何音響は各音源の直接音が 1/(4πr_i)。FDTD は各 feed に**同一の**
   ガウシアン微分パルスを注入する (共通 σ・t0) — このため ADR-0008
   Decision 2 の音源逆フィルタは重ね合わせ RIR に対してそのまま有効。
5. **後期残響の合算則**: FDTD は振幅加算 (線形系の 1 実現)、幾何音響は
   エコーグラムの**エネルギー加算** (異なる音源間はインコヒーレント扱い)。
   帯域内クロス項の期待値は 0 なので、両者は期待値で一致する。
6. **両ソルバーで対称に実装する** (片側のみの実装は禁止)。ソルバー側の
   番人は OpenAcoustics の `acoustic_check.sh`:
   - FDTD (h): 2 feed の同時発火 RIR が各 feed 単独の RIR の和に一致
     (離散更新の線形性。L2 相対誤差 ≤ 1e-5、float32 量子化が支配項)
   - 幾何音響 (K): 2 音源の直接音がそれぞれ 1/(4πr_i)・t_i = r_i/c (±1%)、
     既定 (キー省略) は feed #1 のみ + warning、室外の音源は非零終了
7. **音源ごとのゲイン・遅延は `.ofdx` の `acoustic.sources[]` で与える**
   (2026-08-16 改訂で追加。指向性は引き続き範囲外):
   ```json
   "acoustic": {
     "multi_source": true,
     "sources": [ { "gain": 0.5 }, { "gain": 2.0, "delay_s": 0.005 } ]
   }
   ```
   - 配列の並びは `.ofd` の feed 行の順 (entry #1 = feed #1)。行・キーの
     省略は既定値 `gain = 1` / `delay_s = 0` — キー自体を省略すれば
     Decision 1〜5 の従来動作と完全一致 (後方互換)。feed 数を超える行は
     warning を出して無視する。
   - 意味: 音源 i は **t = delay_i [s] に強度 gain_i で発火**する。時間
     原点 t = 0 は共通のまま (幾何音響の直接音は t = delay_i + r_i/c、
     FDTD はパルス中心が t0 + delay_i)。負の gain は極性反転。
     範囲は |gain| ≤ 1000、0 ≤ delay_s ≤ 1 — 範囲外は既定値に落とさず
     **非零終了** (数値を捏造しない)。
   - キーは `acoustic.ga` ではなく `acoustic` 直下 (Decision 1 と同じ理由 —
     両ソルバーが読む)。`multi_source = false` でも entry #1 は feed #1 に
     効く (「発火する音源集合に一様に適用」で場合分けを作らない)。
   - 計算時間は両ソルバーとも max(delay_i) だけ自動延長する (遅延で直接音・
     残響が窓の外にこぼれない)。後期残響のエネルギー加算 (Decision 5) は
     gain_i² 重み。
   - metadata.json は `sources[]` の各要素に `gain` / `delay_s` を追記する
     (キー追加のみ — Decision 3 と同じ規則)。GUI が読む `source` の
     `sigma_s` / `t0_s` / `fmax_hz` は不変 (遅延は t0_s に**足し込まない** —
     ADR-0008 Decision 2 の音源逆フィルタは遅延・定数倍と可換なので、
     重ね合わせ RIR に対してそのまま有効)。
   - ソルバー側の番人 (OpenAcoustics `acoustic_check.sh`):
     FDTD (h2) — gain = 0.5 の RIR が gain = 1 の RIR のちょうど 0.5 倍
     (2 の冪のスケールは IEEE 丸めと可換)、delay = 5 ms で直接音ピークが
     5 ms 移動、ゲイン・遅延つき 2 feed の重ね合わせが単独実行の和に一致。
     幾何音響 (K2) — 直接音がそれぞれ gain_i/(4πr_i)・t = delay_i + r_i/c
     (±1%)。両者とも範囲外の値の非零終了を含む。
   - GUI 側の編集 UI (feed ごとのゲイン・遅延の表) は未実装 — 手書きした
     `acoustic.sources` は `.ofdx` の未知キー往復保全で保存時も保持される。
     UI を付けるときは `AcousticOpts` に載せて絶対規則 2 (無効時バイト
     不変) を守ること。

## Consequences

- (+) 既存フローは無変更 (opt-in、キー省略時は従来動作と完全一致)。
  ADR-0003 の未知キー無視により、旧版 GUI / 旧版ソルバーとも共存できる。
- (+) モック・AcousticRunner・HybridRir に変更不要 (ファイル契約が不変、
  GUI が読む metadata キーの意味も不変)。
- (−) 音源の**指向性**は本 ADR の範囲外 (全て無指向)。ゲイン・遅延は
  Decision 7 (2026-08-16 改訂) で `acoustic.sources[]` として対応した。
  指向性が必要になったら同じ `sources` 側に追加キーで拡張する
  (バンド別の指向性パターンの定義を伴うため、別途判断)。
- (−) 「音源ごとの個別 RIR」(rir_s2.wav 等) も範囲外 — ファイル契約の
  変更 (ADR-0007 の改訂) を伴うため、必要になったら別 ADR にする。
- GUI 側のトグル UI (AcousticSolverTab「複数音源」) と `.ofdx` 書き出しは
  実装済み。単発実行・ハイブリッド実行のどちらも同じ `prepareRunInput`
  経路でサイドカーを書くので、トグル 1 つで両ソルバーに同時に効く
  (対称性が自動的に保たれる)。
