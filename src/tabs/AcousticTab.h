// AcousticTab.h — 室内音響タブ (RT60/C80/D50/STI, マイクアレイ, 可聴化).
// Settings persist in the .ofdx sidecar.
//
// 先頭の「音響解析の進め方」パネルは、室内音響がタブ 10 個以上に分かれていて
// どれをどの順に触るのか分からない、という報告への対応。7 ステップを
// 「番号 / やること / 対応タブ (左ナビ表記そのまま) / 現在の状態」で並べ、
// 状態はプロジェクトの実データから判定する (未設定を設定済みと書かない)。
#pragma once
#include <QScrollArea>
#include "../core/Project.h"   // workflowStatus() が Project を直接読む

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QDoubleSpinBox;

namespace ofd {

class Project;

class AcousticTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AcousticTab(Project *project, QWidget *parent = nullptr);

    // ── 「音響解析の進め方」パネルの状態判定 ────────────────────────────────
    // ofdx_selftest は GUI_SOURCES をリンクしないため、判定ロジックは
    // ヘッダ内 inline の static メンバにしてある
    // (AuralizationTab::drySourceCandidates と同じ流儀 — クラスは
    // instantiate せずに関数だけを検証する)。

    static const int kWorkflowSteps = 7;

    // ステップ 5 (計算の実行) の前提条件。外部ソルバーを起動しない
    // バックエンド (None / 実測RIR / 統計) では「バイナリが見つからない」は
    // 問題ではないので、3 値で区別する。
    enum SolverReadiness {
        SolverUnresolved = 0,  // 外部ソルバー使用だが実行ファイルが見つからない
        SolverResolved   = 1,  // 外部ソルバーの実行ファイルを解決できた
        SolverNotUsed    = 2,  // 外部ソルバーを使わない設定 (実測/統計/なし)
    };

    // 1 ステップ分の状態。state 以外の意味はステップごとに異なる:
    //   1: n1=メッシュ総セル数 (0=メッシュ未定義)  n2=形状の数
    //   2: n1=波源 (feed) の数
    //   3: n1=観測点 (point) の数
    //   4: n1=有効な吸音行の数                    n2=吸音表の全行数
    //   5: n1=SolverReadiness                    n2=RIR があれば 1
    //   6: n1=実測RIR (rirPath) があれば 1
    //   7: n1=可聴化のドライ音源があれば 1
    struct StepStatus {
        int    state = 0;   // 0=未設定 1=一部のみ 2=設定済み
        qint64 n1 = 0;
        qint64 n2 = 0;
    };

    // ステップ番号 (1..kWorkflowSteps) の状態をプロジェクトから判定する純関数。
    // 範囲外のステップは既定値 (state=0) を返す。
    static StepStatus workflowStatus(const Project &p, int step,
                                     int solverReadiness)
    {
        StepStatus s;
        switch (step) {
        case 1: {   // 部屋の形状とメッシュ
            const bool meshOk = p.mesh(0).isValid() && p.mesh(1).isValid()
                                && p.mesh(2).isValid();
            s.n1 = meshOk ? p.totalCells() : 0;
            s.n2 = p.geometries().size();
            // メッシュだけで形状が無い状態は「一部のみ」(直方体の室でも
            // 面の材質を持たせるには形状が要る)
            s.state = !meshOk ? 0 : (s.n2 > 0 ? 2 : 1);
            break;
        }
        case 2:     // 音源の位置 = .ofd の feed (音源リストから同期される)
            s.n1 = p.feeds().size();
            s.state = s.n1 > 0 ? 2 : 0;
            break;
        case 3:     // 受音点の位置 = .ofd の point (観測点)
            s.n1 = p.probes().size();
            s.state = s.n1 > 0 ? 2 : 0;
            break;
        case 4: {   // 壁の吸音率 = 吸音バジェット (AcousticOpts::absorption)
            const QVector<AbsorptionRow> &rows = p.acoustic().absorption;
            for (const AbsorptionRow &r : rows)
                if (r.enabled) ++s.n1;
            s.n2 = rows.size();
            s.state = s.n1 > 0 ? 2 : (s.n2 > 0 ? 1 : 0);
            break;
        }
        case 5: {   // 計算の実行 (RIR 生成)
            const bool rir = !p.operaAcoustic().rirPath.trimmed().isEmpty();
            s.n1 = (solverReadiness < 0 || solverReadiness > SolverNotUsed)
                       ? int(SolverUnresolved) : solverReadiness;
            s.n2 = rir ? 1 : 0;
            s.state = rir ? 2 : (s.n1 == SolverUnresolved ? 0 : 1);
            break;
        }
        case 6:     // 指標の分析 (実測RIR分析タブが読む WAV)
            s.n1 = p.operaAcoustic().rirPath.trimmed().isEmpty() ? 0 : 1;
            s.state = s.n1 ? 2 : 0;
            break;
        case 7:     // 聞こえ方の生成 (可聴化のドライ音源)
            s.n1 = p.operaAcoustic()
                       .auralizationDryFile.trimmed().isEmpty() ? 0 : 1;
            s.state = s.n1 ? 2 : 0;
            break;
        default:
            break;
        }
        return s;
    }

    // ステップ番号 → 左ナビのキー (MainWindow::buildLeftNav の Def::key)。
    // 範囲外は nullptr。タブ同士を直接依存させないため、AcousticTab は
    // このキーを navigateRequested() で投げるだけで、実際の切替は
    // MainWindow (TabNavigator::selectKey) が行う。
    static const char *workflowNavKey(int step)
    {
        switch (step) {
        case 1: return "geometry";      // ① 形状 (メッシュ詳細は同じ入口)
        case 2: return "source";        // ④ 音源 (feed)
        case 3: return "source";        // ④ 音源の観測点 (point)
        case 4: return "roomac";        // 🏛 ホール解析 (吸音バジェット)
        case 5: return "acsolver";      // 🔌 音響ソルバ連携
        case 6: return "riranalysis";   // 🎤 実測RIR分析
        case 7: return "auralization";  // 🔊 可聴化
        default: return nullptr;
        }
    }

signals:
    // 手順パネルの行がクリックされた → 左ナビをこのキーへ切り替えてほしい。
    // (タブ間の直接依存を作らないよう MainWindow が中継する)
    void navigateRequested(const QString &navKey);

private slots:
    void refresh();
    void runConvolve();        // 可聴化経路 (ConvolutionEngine) へ委譲
    void chooseAuralSource(int index);   // ソース音源の選択 (生成 / 取込)
    void applyReceiverCount(); // 受音点数スピン → 受音点リストの伸縮

protected:
    // 折り返し後の行高で手順パネルを実寸に合わせ直す (列幅は表示後に決まる)
    void resizeEvent(QResizeEvent *e) override;

private:
    void apply();
    void updateSolverView();   // ソルバー切替 → 説明文と条件付きパネルの表示
    void refreshWorkflow();    // 手順パネルの「現在の状態」列を更新
    void fitStepTable();       // 手順パネルの行高/全体高を内容に合わせる
    int  solverReadiness() const;   // ステップ 5 の前提条件を実環境から判定
    // 受音点リスト (AcousticOpts::receivers) の表 ↔ モデル
    void refreshReceivers();
    void applyReceivers();
    // 材質設定 = 吸音バジェット (AcousticOpts::absorption) の表 ↔ モデル
    void refreshSurfaces();
    void applySurfaces();
    // 可聴化 (ドライ音源の表示と、出力形式が何になるかの表示)
    void refreshAuralization();
    int  rirChannelCount();    // RIR の実チャネル数 (0 = 未設定/読めない)

    Project   *m_p;
    bool       m_updating = false;

    // 「音響解析の進め方」パネル (番号 / やること / 対応タブ / 現在の状態)
    QTableWidget *m_stepTable = nullptr;
    QLabel       *m_stepHint = nullptr;   // 表と同じ幅に収める説明文

    QCheckBox *m_rt60, *m_c80, *m_d50, *m_sti, *m_edt, *m_irf, *m_aural;
    QComboBox *m_sampleRate;
    QComboBox *m_directivity;
    QDoubleSpinBox *m_spl;
    QSpinBox  *m_micCount;

    // ── モック (tabs.jsx AcousticTab) 追加分 ──────────────────────────────
    // LF / 位置・向き / 解析タイプ / 帯域 / 受音点リストは AcousticOpts
    // (.ofdx) に永続化。ソルバー・可聴化ソース/出力形式はローカル状態。
    QCheckBox *m_lf;                    // LF (側方音エネルギー)
    QLineEdit *m_srcPos, *m_srcAim;     // 位置(x,y,z) [m] / 向き(θ,φ) [deg]
    QTableWidget *m_micTable;           // 受音点表 (AcousticOpts::receivers)

    QComboBox *m_solver;                // FDTD / Ray / Image-Source / Hybrid
    QLabel    *m_solverDesc;
    QWidget   *m_rayPanel, *m_ismPanel, *m_hybridPanel;
    QSpinBox  *m_numRays, *m_maxBounces, *m_rayCrossover;
    QCheckBox *m_specular, *m_diffuse;
    QComboBox *m_rayBandRes;
    QSpinBox  *m_ismOrder;
    QCheckBox *m_ismVisibility;
    QSpinBox  *m_hybridSplit;

    QComboBox *m_analysisType;          // IRF / RT60 / STI
    QCheckBox *m_thirdOctave;
    QComboBox *m_bandRange;

    QComboBox *m_auralSource;
    QCheckBox *m_outMono, *m_outStereo, *m_outBinaural, *m_outAmbi;
    QLabel    *m_auralDry = nullptr;      // 現在のドライ音源 (実データの表示)
    QLabel    *m_outNote = nullptr;       // 出力形式が何で決まるかの説明
    // このセッションで生成したクリックのパス (ソース音源の選択表示に使う。
    // .ofdx へは書かない — 既存キーを増やさないため)
    QString    m_clickPath;
    // RIR のチャネル数のキャッシュ (パス + 更新時刻が変わったら読み直す)
    QString    m_rirProbeKey;
    int        m_rirProbeCh = 0;
    // 材質設定 = 吸音バジェット (AcousticOpts::absorption) の View。
    // 同じデータを RoomAcousticsTab の吸音バジェット表とも共有する。
    QTableWidget *m_surfTable;
};

} // namespace ofd
