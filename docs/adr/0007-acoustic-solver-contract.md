# ADR-0007: 外部音響ソルバーの出力契約・AcousticBackend・モック戦略 (ADR-0004 の具体化)

## Status

Accepted (2026-07-16) — 実装済み (フェーズ5:
`src/acoustics/qt/AcousticRunner`、AcousticSolverTab、CI モック
ソルバー)。ADR-0004 (方針決定) を実装レベルで具体化する。
ADR-0004 の `AcousticBackend` 案 ({ None, ExternalSolver } の 2 値) は
本 ADR の 5 値 enum で置き換える。

## Context

- ADR-0004 で「音響シミュレーションは外部ソルバーを QProcess 疎結合で
  連携し、出力は作業ディレクトリへのファイル契約とする」ことを決めた。
  フェーズ5 の実装にあたり、(a) 契約の確定、(b) GUI 側クラス設計、
  (c) 実ソルバー不在下でのテスト戦略、を決める必要がある。
- 既存 `kernel/Runner` は電磁系カーネル (ofd / orcwa / obpm) 用で、
  `.ofd` 書き出し・カーネル固有の進捗行解析・Kernel enum に密結合して
  いる。baseline 運用ルール上、既存 Runner と selftest は不変で
  なければならない。
- RIR の取得元は外部 FDTD だけではない: 実測 WAV (フェーズ2)、統計
  モデルからの合成、幾何音響 (レイトレース系) ソルバーも同じ
  「RIR → RirAnalyzer」経路に載る。UI (AcousticSolverTab) は取得元を
  明示的に区別して表示する必要がある (要求 §2 の 3 区分)。
- 実音響ソルバーは本リポジトリに存在しない (別リポジトリで開発)。
  しかし GUI 側の起動・契約検証・エラー処理・RIR 受け渡しは CI で
  回帰テストしたい。

## Decision

1. **既存 Runner と分離した `AcousticRunner` を新設する**。理由:
   - 既存 Runner は `.ofd` 入力 + カーネル固有進捗解析に密結合で、
     ファイル契約ベースの音響ソルバーとはライフサイクルが異なる。
   - baseline (既存 Runner・selftest 不変) を守るには、拡張より分離が
     安全 (ADR-0004 の「`.ofd` 不可侵」と同じ論理)。
   - 音響側は「作業ディレクトリの契約ファイル検証」という Runner に
     ない責務を持つ。共通化できるのはプロセス起動の骨格だけで、
     共有する価値が薄い。
2. **`AcousticBackend` は 5 値 enum とする** (RIR の取得元を表す):
   `enum class AcousticBackend { None, MeasuredRir, Statistical,
   ExternalFDTD, ExternalGeometric }`。`.ofdx` には
   `opera_analysis.solver.backend` (int, 同順) で永続化する
   (値の並び替え・挿入禁止、追加は末尾のみ — ADR-0002/0003 と同規則)。
   外部プロセスを起動するのは ExternalFDTD / ExternalGeometric のみ。
3. **バイナリ解決** (ADR-0004 Decision 3 を確定): ① 環境変数
   `OFDX_ACOUSTIC_SOLVER` (絶対パス直接指定、最優先 — CI/開発
   オーバーライド) → ② `OPENFDTD_ACOUSTICS_HOME` 配下 → ③ アプリ
   実行ディレクトリ `kernel/` → ④ PATH。`.ofdx` の
   `solver.executable` が非空ならそれを最優先で使う。
4. **出力契約を確定する** (ソルバーは `solver.working_dir` に出力):
   - `metadata.json` (必須): `contract_version`、ソルバー名/
     バージョン、格子 (Δx・セル数)、fs、音源/受音点座標、音速、
     実行条件。未知キー無視・追加キーのみの互換規則。
   - `rir.wav` (必須): 受音点 RIR。WavReader 対応形式 (float32 推奨)。
   - `metrics.json` (任意): ソルバー側算出指標。GUI は自前計算
     (RirAnalyzer) を正とし突合表示のみに使う。
   - `solver.log` (必須): 実行ログ。進捗は stdout にも出力し
     AcousticRunner が解析する。
   契約違反 (metadata.json 不在 / rir.wav 読込失敗 / 異常終了コード)
   は明確なエラーメッセージで表示し、部分出力は採用しない。
5. **モックソルバー戦略**: 出力契約に準拠した**最小の C 実行ファイル**
   (合成 RIR — 例: 指数減衰系列 — と契約ファイル一式を書き出すだけ)
   をテスト専用にビルドし、CI では `OFDX_ACOUSTIC_SOLVER` でこれを
   指定して AcousticRunner → 契約検証 → WavReader → RirAnalyzer →
   結果表示区分までの統合経路を検証する。異常系 (ファイル欠落・
   壊れた WAV・非零終了コード) もモックの引数で再現する。
   モックは契約の**実行可能な仕様**を兼ねる: 契約を変更するときは
   モックと本 ADR を同時に更新する (仮定 §22)。
6. **`rir.wav` は実測 RIR と同一パイプラインで分析し、結果は
   「シミュレーション RIR」区分で表示する** (ADR-0004 の主目的を維持)。
   AcousticSolverTab は取得した RIR を RirAnalysisTab へ受け渡す。

## Consequences

- (+) 既存 Runner・selftest・`.ofd` 互換に一切手を入れずにフェーズ5 が
  完結する (baseline 不変)。
- (+) 実ソルバーがなくても GUI 側の全経路が CI で回帰テストされる。
  ソルバー開発側はモックのソースを「契約の参照実装」として読める。
- (+) backend 5 値により、UI の取得元 3 区分表示 (統計/実測/
  シミュレーション) が enum から一意に導出でき、表示の食い違いが
  起きない。
- (−) Runner が 2 系統になり、プロセス起動・キャンセル・タイムアウト
  処理が重複する。共通化は両者が安定した後の整理課題とする。
- (−) モックは契約と GUI 側処理のみを保証し、実ソルバーの物理・性能・
  実時間の進捗挙動は検証しない (仮定 §22)。実ソルバー接続時の受け入れ
  試験は別途必要 (`docs/opera-acoustics-development-status.md`
  負債 #13)。
- (−) `contract_version` という互換性管理面が正式に増えた。`.ofdx` と
  同じ「追加キーのみ・未知キー無視」規則で運用する。
- 未決事項 (実ソルバー接続時に確定): 複数受音点の rir.wav 形式
  (多チャンネル 1 ファイル vs 連番)、`metrics.json` の指標命名、
  進捗行フォーマットの正式化。
