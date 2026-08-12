// OpticalTab.h — 光解析タブ: FDTD/RCWA/BPM/FMM ソルバー切替 + 光モード設定.
// Settings persist in the .ofdx sidecar; RCWA/BPM run the OpenRCWA (orcwa) /
// OpenBPM (obpm) sister kernels through Runner.
//
// 光解析モード (BPF / 導波路 / リング / MZI / メタサーフェス / フォトニック結晶
// / NF→FF / S パラメータ) はモード別セクションを持ち、選択中のモードのものだけを
// 表示する (updateModeSections)。末尾に分散モデル (Drude/Lorentz/Sellmeier)。
//
// 非線形 (TPA) / ONN 活性化 (Honda, Shoji, Amemiya, Opt. Lett. 49, 5811
// (2024)): BPM セクションで tpa / powersweep キーを設定し、obpm 実行後の
// activation_curve.csv を活性化カーブ P_out(P_in)・透過率 T(P_in) として表示。
#pragma once
#include <QScrollArea>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QStackedWidget;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;
class SectionBox;

class OpticalTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OpticalTab(Project *project, QWidget *parent = nullptr);

    // obpm + powersweep の実行完了後に MainWindow から呼ばれる。workdir の
    // activation_curve.csv があれば読み込んで ONN 活性化カーブを表示する。
    // aeff_m2 はカーネルログ "ONN: A_eff = ... [m^2]" から抽出した値 (0=不明)。
    // beta_cmGW / length_m は解析解の重ね描きに使う β [cm/GW] と伝搬長 L [m]。
    // いずれも「その実行の開始時にスナップショットした値」を渡すこと
    // (表示時点のライブ値を使うと、実行中の UI 編集で実測 CSV と対応しない
    //  解析解が重なる)。
    void showActivationResult(const QString &workdir, double aeff_m2,
                              double beta_cmGW, double length_m);

private slots:
    void refresh();

private:
    void apply();
    void updateTpaWidgetState();
    // mock の {mode === "…" && <Section…>} 相当: 選択モードのセクションだけ表示
    void updateModeSections();
    // S パラメータの書き出し (カーネルの test.snp → Touchstone / CSV)
    void exportSparam();
    // BPF 設計目標の透過スペクトル (目標帯域・Q・IL・阻止域から再計算)
    void updateBpfPlot();
    // RCWA 層テーブル ↔ モデル。applyRcwaTable() はテーブルの内容を
    // そのままモデルへ書き、不正な行を赤字にして警告文字列を返す
    // (UI とモデル/保存内容を乖離させない — 不正時は OfdIO 側が RCWA 行を
    //  丸ごと出力しないので、カーネルには不正な設定が渡らない)。
    QStringList applyRcwaTable();
    void        refreshRcwaTable();
    void        refreshOpticalSystem();  // 面データ表 (レンズエディタと共有)
    void        updateGeoMethodView();   // 解法 → 波動ソルバー設定の有効・無効
    // Raycast 節の設定で非順次レイトレースを実行し、結果を要約する
    void        runRaycast();

    Project   *m_p;
    bool       m_updating = false;

    QComboBox *m_solver;
    QStackedWidget *m_solverStack;
    // 解法 (波動FDTD / Raycast 幾何光学 / ハイブリッド FDTD+Ray)。
    // OpticalSolver enum は Runner のカーネル選択に直結するので拡張できない →
    // mock の Seg を UI 専用のローカル state として持つ (永続化しない)。
    QComboBox *m_geoMethod;
    QLabel    *m_geoHint;
    QComboBox *m_mode;
    QLineEdit *m_lambdaMin, *m_lambdaMax;
    QSpinBox  *m_lambdaDiv;

    // RCWA
    QSpinBox  *m_rcwaNx, *m_rcwaNy, *m_rcwaLayers;
    QLineEdit *m_rcwaPx, *m_rcwaPy;
    QTableWidget *m_rcwaStack;      // eps1 / eps2 / fill / 厚み[nm]
    QPushButton  *m_rcwaAdd, *m_rcwaDel;
    QLabel       *m_rcwaWarn;
    // BPM
    QComboBox *m_bpmAlgo, *m_bpmInput;
    QLineEdit *m_bpmDz, *m_bpmN0;
    // BPM: 非線形 (TPA) / ONN 活性化
    QCheckBox *m_tpaEnable, *m_psEnable;
    QSpinBox  *m_tpaMatId, *m_psPoints;
    QLineEdit *m_tpaBeta, *m_psPmin, *m_psPmax;
    QComboBox *m_psScale;
    QLabel    *m_tpaWarn;
    // FMM
    QSpinBox  *m_fmmHarmonics;
    QCheckBox *m_fmmLi;
    // BPF / Ring
    QLineEdit *m_bpfMin, *m_bpfMax, *m_bpfQ;
    QLineEdit *m_ringR, *m_ringGap;

    // ── 光解析モード別セクション (mock: mode ごとの条件付き <Section>) ────────
    // モードに対応するセクションのみ表示する (updateModeSections)。
    SectionBox *m_secBpf, *m_secWg, *m_secRing, *m_secMzi;
    SectionBox *m_secMeta, *m_secPhc, *m_secNfff, *m_secSparam;

    // 以下のモード別設定は OpticalOpts へ apply()/refresh() で配線され、
    // .ofdx サイドカーに保存される (カーネル入力 .ofd は不変)。
    // BPF: 挿入損失 / 阻止域 + 透過スペクトル (設計目標カーブを再計算)
    QLineEdit *m_bpfIL, *m_bpfStop;
    MiniPlot  *m_bpfPlot;
    // Ring: thru / drop ポート出力
    QCheckBox *m_ringThru, *m_ringDrop;
    // 導波路モード解析 (opt_wg)
    QCheckBox *m_wgTe0, *m_wgTe1, *m_wgTm0, *m_wgTm1;
    QLineEdit *m_wgLoss;
    // MZI
    QLineEdit *m_mziDeltaL;
    QCheckBox *m_mziThermo, *m_mziElectro;
    // メタサーフェス
    QLineEdit *m_metaPeriod;
    QComboBox *m_metaShape, *m_metaPhase;
    // フォトニック結晶
    QComboBox *m_phcLattice;
    QLineEdit *m_phcA, *m_phcRa;
    QCheckBox *m_phcBand, *m_phcDefect;
    // 近傍界→遠方界変換 (ofd_post 連携は未実装 — 設定の保存のみ)
    QComboBox *m_nfffSurface;
    QLineEdit *m_nfffDistance;
    // S パラメータ (入力/出力ポート番号は S21 抽出の対象ポート対)
    QSpinBox    *m_spPorts, *m_spPortIn, *m_spPortOut;
    QCheckBox   *m_spS11, *m_spS21, *m_spPhase, *m_spGroupDelay;
    QPushButton *m_spExport;

    // ── Raycast 設定 / Geometric Optics (mock: 幾何光学レイトレース) ──
    // 対応する Project フィールドが無いためローカル state (モック既定値) のみ。
    QSpinBox  *m_rayCount, *m_rayBounces, *m_rayVizCount;
    QSpinBox  *m_rayDiffOrder;            // 拡散次数 (拡散反射を追跡する段数)
    QLineEdit *m_rayMinEnergy;
    QComboBox *m_raySampling;
    QCheckBox *m_raySpecular, *m_rayDiffuse;
    QCheckBox *m_rayPolarized, *m_rayDispersion, *m_rayFresnel;
    QCheckBox *m_rayVizEnable;
    // 追跡の実行 (optics/IlluminationTrace)。系は照明タブと共有の
    // IlluminationOpts から組む (core/IlluminationScene)
    QPushButton *m_rayRunBtn = nullptr;
    QLabel      *m_rayResult = nullptr;

    // ── 光学系定義 / Optical system (面データ表 + 解析オプション) ──
    QTableWidget *m_optSysTable;
    QCheckBox *m_optSeidel, *m_optSpot, *m_optMtf, *m_optRayAberr;

    // ── ハイブリッド連携 / FDTD↔Ray bridge ──
    QCheckBox *m_hybModeDecomp, *m_hybGaussian;
    QComboBox *m_hybPropModel;

    // ── 分散モデル / Dispersion model (Drude / Lorentz / Sellmeier) ──
    QComboBox *m_dispModel;

    // ONN 活性化カーブ結果表示
    QLabel       *m_onnStatus;
    MiniPlot     *m_onnPlotP, *m_onnPlotT;
    QTableWidget *m_onnTable;
};

} // namespace ofd
