# オペラ歌手向け室内音響分析 — アーキテクチャ設計

対象: OpenFDTD-X (branch `claude/opera-acoustics`)。
本書は実装済みコード (`src/acoustics/**`, `CMakeLists.txt`) の構成を
示す。フェーズ3 (歌声分析)・フェーズ4 (可聴化)・フェーズ5 (外部音響
ソルバー連携) を含む。

## 1. 全体構成 (現在の OpenFDTD-X + 音響分析層)

```mermaid
flowchart TB
    subgraph GUI["GUI 層 — C++17 / Qt6 Widgets"]
        MW["MainWindow / DomainBar<br/>(ドメイン切替: EM / 光 / 室内音響 / 水中)"]
        AT["AcousticTab<br/>(FDTD 音響設定)"]
        RAT["RoomAcousticsTab<br/>(統計推定: Sabine / Eyring / Barron / NC)"]
        RIRT["RirAnalysisTab<br/>実測/シミュレーション RIR 分析 UI"]
        VAT["VocalAnalysisTab (フェーズ3)<br/>歌声分析 UI"]
        AUT["AuralizationTab (フェーズ4)<br/>可聴化 (ドライ×RIR 畳み込み)"]
        AST["AcousticSolverTab (フェーズ5)<br/>シミュレーション RIR 取得"]
        MP["MiniPlot / PlotPanel<br/>(波形・減衰カーブ・F0/LTAS 表示)"]
    end

    subgraph CORE17["既存モデル層 — C++17 / Qt 依存"]
        PRJ["Project<br/>AcousticOpts / OperaAcousticSettings"]
        ROOMAC["core/RoomAcoustics<br/>(統計推定・純関数)"]
        OFDIO["io/OfdIO<br/>.ofd + .ofdx (JSON) の読み書き"]
        RUN["kernel/Runner<br/>(QProcess: ofd / orcwa / obpm — 不変)"]
    end

    subgraph AC["音響分析層 — 新規"]
        QTA["acoustics/qt/QtAcousticAdapter<br/>(C++17 / Qt: 型変換・ch選択)"]
        ARUN["acoustics/qt/AcousticRunner (フェーズ5)<br/>(QProcess 疎結合・出力契約検証)"]
        subgraph CORE14["ofdx_acoustic_core — C++14 / Qt 非依存 (STATIC lib)"]
            RA["RirAnalyzer"]
            VA["VocalAnalyzer (フェーズ3)<br/>YIN F0 / ビブラート / LTAS / HNR"]
            CE["ConvolutionEngine (フェーズ4)<br/>Overlap-Add 畳み込み"]
            FFT["Fft (フェーズ4)<br/>自前 radix-2 FFT"]
            EST["AcousticFdtdEstimator (フェーズ5)<br/>音響 FDTD 規模見積り (電磁と独立)"]
            WAV["io: WavReader / WavWriter"]
        end
        CAPI["ofdx_acoustic_c_api — 安定 C ABI<br/>openfdtd_x_acoustics.h (C99 から利用可)"]
    end

    EXT["外部音響ソルバー (フェーズ5)<br/>実ソルバー or モックソルバー (CI 用 小型 C 実行ファイル)"]
    OFDX[(".ofdx JSON<br/>acoustic.opera_analysis")]
    WAVF[("実測/シミュレーション RIR WAV<br/>歌唱 WAV (ドライ/ウェット)")]

    MW --> AT & RAT & RIRT & VAT & AUT & AST
    RAT --> ROOMAC
    RIRT --> QTA --> RA
    VAT --> QTA --> VA
    AUT --> QTA --> CE
    CE --> FFT
    VA --> FFT
    RA --> WAV
    CE --> WAV
    CAPI --> RA
    RIRT --> MP
    VAT --> MP
    AT & RAT & RIRT & VAT & AUT & AST --> PRJ
    PRJ <--> OFDIO <--> OFDX
    QTA --> WAVF
    MW --> RUN
    AST --> ARUN --> EXT
    AST --> EST
    EXT -- "rir.wav (出力契約)" --> WAVF
    WAVF -- "RIR 分析へ受け渡し" --> RIRT
```

依存規則 (CMake で強制):

- `ofdx_acoustic_core` は **Qt にリンクしない**。`CXX_STANDARD 14` /
  `CXX_STANDARD_REQUIRED ON` / `CXX_EXTENSIONS OFF` (逸脱はビルドエラー)。
- `ofdx_acoustic_c_api` はコアのみに依存 (同じく C++14 固定)。ヘッダは
  純 C (C99) から include 可能で、`tests/acoustics/test_c_api.c` が
  C コンパイラでのビルド可否そのものを検証する。
- Qt 型 (`QString` 等) がコアに入る唯一の入口は `QtAcousticAdapter`。

## 2. C++14 コアのクラス構成 (実装済み)

```mermaid
classDiagram
    class RirAnalyzer {
        +RirAnalyzer(RirAnalyzerConfig)
        +analyze(ArrayView~const double~, double fs) AcousticResult~RirAnalysisResult~
        +analyze(AudioBuffer, channel) AcousticResult~RirAnalysisResult~
    }
    class RirAnalyzerConfig {
        +CalibrationState calibration
        +double calibrationOffsetDb
        +BandSet bandSet
        +bool zeroPhaseFiltering
        +bool removeDc
        +double minDynamicRangeDb
        +double clipThreshold
        +int clipRunLength
    }
    class DirectSoundDetector {
        +detectDirectSound(x, fs, cfg) DirectSoundResult
        Peak / EnvelopeThreshold / MovingRmsThreshold
    }
    class NoiseFloorEstimator {
        +estimateNoiseFloor(x, tailFraction, minTail) NoiseFloorEstimate
        末尾10% RMS
    }
    class SchroederDecay {
        +computeSchroederDecay(rir, fs, opt) SchroederResult
        +regressDecaySegment(curve, fs, startDb, endDb, endIdx) RegressionResult
        二乗後方積分 + Chu 1978 ノイズ補正
    }
    class AcousticMetrics {
        +computeAcousticMetrics(rir, fs, directIndex, opt) AcousticMetricsSet
        EDT T20 T30 C50 C80 D50 Ts Early/Late
    }
    class BandFilter {
        +makeBands(BandSet) vector~Band~
        +design(lowHz, highHz, fs)$ AcousticResult~BandFilter~
        +apply(x, zeroPhase) vector~double~
        4次バターワース BPF (filtfilt 対応)
    }
    class ReflectionDetector {
        +detectReflections(rir, fs, direct, cfg) vector~ReflectionEvent~
        +summarizeReflections(events, directEnergy) ReflectionTimeSummary
        時間区分 0-20 / 20-80 / 80-200 / 200+ ms
    }
    class WavReader {
        +readWavFile(path) AcousticResult~AudioBuffer~
        +readWavFromMemory(data, size) AcousticResult~AudioBuffer~
    }
    class WavWriter {
        +writeWavFile(path, buffer, format) AcousticResult~bool~
        Pcm16 / Float32
    }
    class VocalAnalyzer {
        +analyze(x, fs, VocalAnalyzerConfig) AcousticResult~VocalAnalysisResult~
        YIN F0 / 安定性 / ビブラート / LTAS / 重心 / HNR / H1-H8 / SFR
    }
    class ConvolutionEngine {
        +convolve(dry, rir, fs一致必須) AcousticResult~ConvolutionResult~
        Overlap-Add。自動正規化なし
        outputPeak / suggestedGainDb / クリップ数を報告
    }
    class Fft {
        +forward(x) / +inverse(X)
        自前 radix-2 (2^n 長)
    }
    class AcousticFdtdEstimator {
        +estimate(V, fmax, simTime) FdtdEstimate
        c=343, dx=c/(10 fmax), CFL dt<=dx/(c*sqrt(3))
        電磁用見積りと独立実装
    }

    RirAnalyzer --> RirAnalyzerConfig
    RirAnalyzer --> DirectSoundDetector : ② 直接音検出
    RirAnalyzer --> NoiseFloorEstimator : ① 前処理・動的範囲
    RirAnalyzer --> BandFilter : ③ 帯域分割
    RirAnalyzer --> AcousticMetrics : ④ 帯域別指標
    AcousticMetrics --> SchroederDecay : 減衰カーブ + 回帰
    SchroederDecay --> NoiseFloorEstimator : ノイズパワー推定
    RirAnalyzer --> ReflectionDetector : ⑤ 反射検出 (広帯域)
    RirAnalyzer ..> WavReader : AudioBuffer 経由
    WavWriter ..> WavReader : ラウンドトリップ検証
    VocalAnalyzer --> Fft : LTAS (Welch)
    ConvolutionEngine --> Fft : Overlap-Add ブロック
    ConvolutionEngine ..> WavWriter : ドライ/ウェット書き出し
```

処理順 (`RirAnalyzer::analyze`):
前処理 (入力長 → 非有限値 → クリッピング → DC 除去 → 動的範囲) →
直接音検出 → 絶対 SPL (校正時のみ) → 帯域分割 → 帯域別
Schroeder/ISO 3382-1 指標 → 反射検出・時間区分集計 → 総合品質判定。
共通の結果表現は `MetricValue` (値 + valid + AnalysisQuality + warning) と
`AcousticResult<T>` (エラーコード 16 値、例外なし)。

## 3. 分析結果フロー (フェーズ2: GUI 統合)

```mermaid
sequenceDiagram
    participant U as ユーザー
    participant T as RirAnalysisTab
    participant A as QtAcousticAdapter
    participant C as ofdx_acoustic_core
    participant P as Project / OfdIO

    U->>T: RIR WAV を選択・設定変更
    T->>P: OperaAcousticSettings 更新 (touch)
    P-->>P: 保存時 .ofdx acoustic.opera_analysis へ (設定のみ)
    U->>T: 「分析」実行
    T->>A: analyzeFile(settings)
    A->>A: readWav (QFile→メモリ→readWavFromMemory)
    A->>A: selectChannel (L / R / 平均モノ)
    A->>A: toAnalyzerConfig (校正/帯域/直接音法/動的範囲)
    A->>C: RirAnalyzer::analyze(samples, fs)
    C-->>A: AcousticResult<RirAnalysisResult>
    A-->>T: 結果 (コア型のまま返す)
    T->>T: MetricValue.quality で 有効/参考値/算出不可 を表示
    T->>T: MiniPlot に波形・Schroeder 減衰カーブ
    Note over T,P: 分析結果は永続化しない (毎回再計算)。<br/>.ofdx には設定のみ保存 (ADR-0003)
```

- 結果のライフサイクル: `RirAnalysisResult` はメモリ上のみ。`.ofdx` には
  設定 (`opera_analysis` キー群) だけを保存する。大容量データ (WAV) は
  パス参照 (`docs/opera-acoustics-file-format.md`)。
- 統計推定 (RoomAcousticsTab) / 実測 RIR / シミュレーション RIR の 3 区分
  を UI で明示する (要求 §2)。

## 3b. 歌声分析フロー (フェーズ3: VocalAnalysisTab)

RIR 分析 (§3) と同型の経路を通る: `VocalAnalysisTab` →
`QtAcousticAdapter` (WAV 読み込み・チャンネル選択・設定変換) →
`VocalAnalyzer::analyze` → 結果 (`VocalAnalysisResult`) を品質 3 値で
表示、F0 軌跡 / LTAS を MiniPlot に描画。指標定義は
`docs/opera-acoustic-metrics.md` §10、F0 アルゴリズム選定は
`docs/adr/0006-vocal-f0-yin.md`。RIR 分析 (系統 A/B) と歌声分析
(系統 C) は独立に実行できる (要求 §5)。分析結果は RIR と同じく
永続化せず、`.ofdx` には設定 (`voice_file` / `voice_type`) のみ保存。

## 3c. 可聴化フロー (フェーズ4: AuralizationTab)

```mermaid
flowchart LR
    DRY[("ドライ歌唱 WAV<br/>(auralization.dry_file)")]
    RIRW[("RIR WAV<br/>(実測 rir_file または<br/>シミュレーション rir.wav)")]
    AUT2["AuralizationTab"]
    CE2["ConvolutionEngine<br/>(Overlap-Add, 自前 radix-2 FFT)"]
    REP["レポート: outputPeak /<br/>suggestedGainDb / クリップ数"]
    WET[("ウェット WAV<br/>(auralization.output_file)")]

    DRY --> AUT2
    RIRW --> AUT2
    AUT2 --> CE2
    CE2 --> REP --> AUT2
    CE2 --> WET
    AUT2 -- "A/B 比較 = ドライ/ウェットの<br/>WAV 書き出し (再生は外部プレイヤー)" --> WET
```

- ドライと RIR の **fs 不一致は明示エラー** (リサンプリングしない —
  仮定 §21)。
- **自動正規化しない** (仮定 §12 と同方針)。出力ピーク・推奨ゲイン
  [dB]・クリップサンプル数を報告し、ゲイン適用はユーザー判断
  (`auralization.gain_mode`)。
- **リアルタイム再生は未対応**。A/B はドライ/ウェット WAV の書き出し
  で行う (ADR-0005 Consequences)。

## 4. 外部音響ソルバー連携 (フェーズ5 — AcousticRunner)

既存 `ofd` (電磁 FDTD) は音響に流用しない (ADR-0004)。音響ソルバーは
別バイナリとして、既存 `kernel/Runner` とは別クラス
`AcousticRunner` が QProcess で疎結合に起動する (分離理由は
`docs/adr/0007-acoustic-solver-contract.md`)。

```mermaid
flowchart LR
    subgraph APP["OpenFDTD-X (GUI)"]
        CFG["AcousticSolverTab<br/>AcousticBackend 設定<br/>(None / MeasuredRir / Statistical /<br/>ExternalFDTD / ExternalGeometric)"]
        EST2["AcousticFdtdEstimator<br/>(実行前の規模見積り表示)"]
        RES["AcousticRunner::resolveSolver()<br/>探索順: ① OFDX_ACOUSTIC_SOLVER (直接指定)<br/>② OPENFDTD_ACOUSTICS_HOME 配下<br/>③ アプリ実行ディレクトリ kernel/<br/>④ PATH"]
        QP["AcousticRunner (QProcess 起動)<br/>(作業ディレクトリに入力一式を書き出し)"]
        AN["RirAnalyzer<br/>(rir.wav を実測と同一パイプラインで分析)"]
    end
    subgraph WD["作業ディレクトリ (出力契約)"]
        MJ["metadata.json<br/>(ソルバー名/版, 格子, fs, 音源・受音点, 実行条件)"]
        RW["rir.wav<br/>(受音点 RIR, float32 推奨)"]
        MX["metrics.json<br/>(ソルバー側算出指標 — 任意)"]
        LG["solver.log<br/>(進捗・診断。stdout 進捗行の写し)"]
    end
    SOLVER["外部音響ソルバー<br/>(別バイナリ・別リポジトリ可)<br/>CI ではモックソルバー<br/>(契約準拠の小型 C 実行ファイル)"]

    CFG --> EST2
    CFG --> RES --> QP --> SOLVER
    SOLVER --> MJ & RW & MX & LG
    RW --> AN
    MJ --> AN
    AN --> GUI2["結果表示<br/>(区分: シミュレーション RIR)"]
```

出力契約 (ソルバー側の義務):

| ファイル | 必須 | 内容 |
|---|---|---|
| `metadata.json` | 必須 | スキーマ版、ソルバー名/バージョン、格子 (Δx, セル数)、fs、音源/受音点座標、音速、実行時間 |
| `rir.wav` | 必須 | 受音点ごとの RIR (複数受音点は複数チャンネルまたは連番ファイル)。float32 推奨 (WavReader 対応形式であること) |
| `metrics.json` | 任意 | ソルバー側で算出した指標。GUI 側は自前計算 (RirAnalyzer) を正とし、突合表示のみに使う |
| `solver.log` | 必須 | 実行ログ。進捗は stdout にも出力 (Runner が進捗解析) |

環境変数:

- `OPENFDTD_ACOUSTICS_HOME` — 音響ソルバー群のインストール先 (既存
  `OPENFDTD_HOME` / `OPENRCWA_HOME` / `OPENBPM_HOME` と同じ流儀)。
- `OFDX_ACOUSTIC_SOLVER` — ソルバーバイナリの絶対パス直接指定
  (探索順のどれよりも優先。CI・開発時のオーバーライド用)。

`AcousticBackend` (RIR の取得元 5 値):

| 値 | 意味 |
|---|---|
| `None` | RIR 取得なし (統計推定のみ) |
| `MeasuredRir` | 実測 RIR (RirAnalysisTab の従来経路) |
| `Statistical` | 統計モデルからの合成 RIR |
| `ExternalFDTD` | 外部音響 FDTD ソルバー |
| `ExternalGeometric` | 外部幾何音響 (レイトレース系) ソルバー |

`rir.wav` の分析は実測 RIR と完全に同一のコード経路
(`WavReader` → `RirAnalyzer`) を通すため、指標の定義差・実装差が
発生しない — これがフェーズ5 を「ソルバー = RIR 生成器」に限定する
主目的である。

CI 統合テスト: 実ソルバーは本リポジトリに含まれないため、**出力契約に
準拠した最小のモックソルバー (小型 C 実行ファイル)** をビルドし、
`OFDX_ACOUSTIC_SOLVER` で直接指定して AcousticRunner →
契約検証 → RirAnalyzer の経路を通しで検証する
(`docs/adr/0007-acoustic-solver-contract.md`)。

音響 FDTD の規模見積り (`AcousticFdtdEstimator`) は実行前に UI 表示
する。**電磁用のステータスバー見積りロジックの流用ではなく独立実装**
である (ADR-0004 Decision 5): c = 343 m/s、Δx = c/(10·f_max) (λ/10)、
CFL Δt ≤ Δx/(c·√3)、総セル数 = V/Δx³、メモリ = 6 field × double
(8 byte) 想定、概算実行時間 = セル数 × ステップ数 / スループット。

## 5. 二層 C++ 規格 (GUI C++17 / コア C++14) の理由

詳細は `docs/adr/0001-two-tier-cxx-standard.md`。要点:

1. **全体を C++14 にはできない**: Qt6 が C++17 を強制する
   (実測: Qt 6.4.2 `qglobal.h` が `-std=c++14` に対し
   `#error "Qt requires a C++17 compiler"`)。GUI 層は C++17 必須。
2. **コアを C++14 に固定する価値**: 音響コアは外部カーネル
   (フェーズ5 のソルバー側での再利用)・他プロジェクト・古い
   ツールチェーン (CUDA / 組込み / 長期サポートコンパイラ) から
   リンクされ得る。要求規格を下げるほど再利用先が広がる。
3. **機械的強制**: 「C++14 の範囲で書く」だけでは逸脱を検知できない
   (フェーズ0 調査 §7.1)。別 STATIC ライブラリターゲットに分離し
   `CXX_STANDARD 14; CXX_STANDARD_REQUIRED ON; CXX_EXTENSIONS OFF` を
   設定することで、C++17 構文の混入がその場でビルドエラーになる。
4. **接続コスト**: Qt 型との変換は `QtAcousticAdapter` の 1 箇所に集約
   し、コアは `std::vector<double>` / `std::string` / POD のみを使う。
   さらに C ABI (`ofdx_acoustic_c_api`) を被せ、C99 からの利用と
   ABI 前方互換 (`struct_size` / `api_version` 検査) を保証する。
