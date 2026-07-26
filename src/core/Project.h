// Project.h — top-level data model.
//
// One Project holds everything the GUI edits. Tabs are *views* of this model;
// when the user clicks Compute we serialize Project to a .ofd file (compatible
// with the original OpenFDTD kernel) plus a .ofdx JSON sidecar carrying the
// extension-domain settings, then hand both to Runner.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include "Domain.h"
#include "Material.h"
#include "Geometry.h"
#include "Source.h"
#include "MeshAxis.h"
#include "PostOpts.h"

namespace ofd {

// ── 全般タブ (solver / abc / pbc / frequency) ───────────────────────────────
struct GeneralOpts {
    QString title;
    int     maxiter = 3000;
    int     nout    = 50;
    double  converg = 1e-3;

    int     abc = 0;          // 0 = Mur-1st, 1 = PML
    int     pmlL = 5;         // PML layers
    double  pmlM = 2.0;       // PML order m
    double  pmlR0 = 1e-5;     // PML reflection coefficient

    bool    pbcX = false, pbcY = false, pbcZ = false;

    double  f1min = 2e9, f1max = 3e9;  int f1div = 10;  // frequency1 [Hz]
    double  f2min = 3e9, f2max = 3e9;  int f2div = 0;   // frequency2 [Hz]
    bool    hasF1 = true, hasF2 = true;  // false = key absent in loaded file

    double  dt = 0;           // timestep   (0 = auto)
    double  tw = 0;           // pulsewidth (0 = auto)
    double  rfeed = 0;        // feed resistance correction
    int     plot3dgeom = 0;
};

// ── 光ドメイン拡張 (.ofdx) ──────────────────────────────────────────────────
enum class OpticalSolver { FDTD, RCWA, BPM, FMM };
enum class OpticalMode { BPF, Waveguide, Ring, MZI, Metasurface, PhC, NF2FF, SParam };

struct OpticalOpts {
    OpticalSolver solver = OpticalSolver::FDTD;
    OpticalMode   mode   = OpticalMode::BPF;
    double  lambdaMin = 1500.0;   // nm
    double  lambdaMax = 1600.0;
    int     lambdaDiv = 201;

    // RCWA
    int     rcwaNx = 11, rcwaNy = 11;   // Fourier orders
    double  rcwaPeriodX = 600.0, rcwaPeriodY = 600.0;  // nm
    int     rcwaLayers = 8;

    // BPM
    int     bpmAlgorithm = 0;     // 0=FFT, 1=FDM, 2=Wide-Angle Padé
    double  bpmDz = 50.0;         // nm
    double  bpmRefIndex = 1.45;
    int     bpmInputMode = 0;     // 0=TE0, 1=TE1, 2=TM0, 3=Gaussian

    // FMM
    int     fmmHarmonics = 15;
    bool    fmmLiRules = true;

    // BPF
    double  bpfBandMin = 1540.0, bpfBandMax = 1560.0;  // nm
    double  bpfQ = 10000.0;

    // Ring
    double  ringRadius_um = 5.0;
    double  ringGap_nm = 200.0;

    // ── 非線形 (TPA) / ONN 光活性化関数 ──
    // Honda, Shoji, Amemiya, "Optical activation function using a metamaterial
    // waveguide for an all-optical neural network," Opt. Lett. 49, 5811 (2024).
    // メタマテリアル装荷 Si 導波路の二光子吸収 (β=424 cm/GW) による飽和型
    // (ReLU 相当) 活性化。カーネル入力キー: tpa / powersweep (OpenBPM)。
    bool    tpaEnabled = false;
    int     tpaMaterialId = 2;       // TPA を適用する材料 ID
    double  tpaBeta_cmGW = 424.0;    // TPA 係数 β [cm/GW] (論文値)
    bool    powerSweepEnabled = false;
    double  psPmin_W = 0.001;        // 掃引下限 P_in [W]
    double  psPmax_W = 10.0;         // 掃引上限 P_in [W]
    int     psPoints = 41;           // 掃引点数 (≥1)
    bool    psLog = true;            // true=log 間隔, false=lin 間隔
};

// ── 室内音響ドメイン拡張 (.ofdx) ────────────────────────────────────────────

// 吸音バジェットの1行 (面・要素)。α は 125/250/500/1k/2k/4k Hz の6帯域。
struct AbsorptionRow {
    enum Role { Audience, Ceiling, SideWall, RearWall, Floor, Air, Other };
    bool    enabled = true;
    int     role = Other;
    QString name;
    double  area = 0;                            // m² (Air 行は未使用)
    double  alpha[6] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 };
    double  airA = 0;                            // Air 行: 吸音力 A [Sabin] 直接指定
};

struct AcousticOpts {
    bool    rt60 = true, c80 = true, d50 = false, sti = false, edt = false;
    bool    impulseResponse = true;
    bool    auralization = false;
    int     sampleRate = 48000;     // WAV出力サンプリング周波数
    QString srcDirectivity = "omni"; // omni / cardioid / speaker
    double  srcSPL_dB = 94.0;
    int     micCount = 1;

    // ── ホール解析 (RoomAcousticsTab) ──
    double  roomL = 30.0, roomW = 20.0, roomH = 12.0;  // シューボックス [m]
    double  volume = 12000.0;       // 室容積 V [m³] (寸法と独立に編集可)
    double  surface = 3800.0;       // 総表面積 S [m²]
    int     occupancy = 2;          // 0=空席, 1=半分, 2=満席 (客席行のα係数)
    int     rtFormula = 1;          // 0=Sabine, 1=Eyring
    QVector<AbsorptionRow> absorption;   // 吸音バジェット
    double  noiseLevels[7] = { 42, 38, 33, 28, 24, 21, 18 };  // 63..4kHz [dB]
};

// ── 実測 RIR 分析 (RirAnalysisTab, .ofdx "acoustic/opera_analysis") ─────────
// オペラ歌手向けの実測インパルス応答分析の「設定のみ」を保持する。
// 分析結果 (RirAnalysisResult) はモデルに持たない (毎回再計算する)。
// 既存 AcousticOpts (統計推定) とは独立。
struct OperaAcousticSettings {
    bool    enabled = false;
    QString rirPath;              // 実測 RIR の WAV ファイル
    QString voicePath;            // 歌唱音源 WAV (可聴化・将来拡張用)
    int     voiceType = 6;        // 0..5=Sop..Bass, 6=Unknown
    int     calibrationState = 2; // 0=Absolute 1=Relative 2=Uncalibrated
    int     directSoundMethod = 1;// 0=Peak 1=Envelope 2=MovingRms
    int     bandMode = 0;         // 0=既存互換6帯域 1=1oct 2=1/3oct 3=フォルマント帯域
    bool    noiseCorrection = true;
    double  minimumDynamicRangeDb = 35.0;
    int     channelMode = 2;      // 0=L 1=R 2=平均モノ

    // 可聴化 (AuralizationTab, .ofdx "opera_analysis/auralization")
    QString auralizationDryFile;      // ドライ (無響/近接) 歌唱 WAV
    QString auralizationOutputFile;   // ウェット出力 WAV
    int     auralizationGainMode = 0; // 0=そのまま 1=推奨ゲイン適用 (自動正規化なし)

    // 歌声分析 (VocalAnalysisTab, .ofdx "opera_analysis/vocal")
    // 0 = 声種 (voiceType) プリセットの探索範囲を使う。> 0 で上書き。
    // 校正状態は既存 calibrationState を再利用する (Absolute 時のみ SPL 有効)。
    double  vocalF0MinHz = 0.0;
    double  vocalF0MaxHz = 0.0;
};

// ── 水中音響ドメイン拡張 (.ofdx) ────────────────────────────────────────────
struct SSPPoint { double depth_m; double c_mps; };

struct UnderwaterOpts {
    double  waterTemp_C = 15.0;
    double  salinity_psu = 34.5;
    QVector<SSPPoint> ssp;
    bool    sofar = false;
    QString bottomType = "sand";
    double  bottomC_mps = 1650.0;
    double  bottomRho_kgm3 = 1900.0;
    double  sonarFreq_kHz = 3.5;
    double  sonarSL_dB = 220.0;
    double  rangeMax_km = 50.0;
};

// ── tidy3d クラウドバックエンド (光ドメイン専用, .ofdx) ─────────────────────
struct Tidy3dOpts {
    QString projectName = "openfdtd-x";
    QString resolution  = "medium";   // coarse / medium / fine
    bool    autoPml     = true;
};

// ────────────────────────────────────────────────────────────────────────────
class Project : public QObject {
    Q_OBJECT
public:
    explicit Project(QObject *parent = nullptr);

    Domain activeDomain() const { return m_domain; }

    GeneralOpts        &general()     { return m_general; }
    MeshAxis           &mesh(int axis) { return m_mesh[axis]; }   // 0=x 1=y 2=z
    QVector<Material>  &materials()   { return m_materials; }
    QVector<Load>      &loads()       { return m_loads; }
    QVector<Geometry>  &geometries()  { return m_geometries; }
    QVector<Feed>      &feeds()       { return m_feeds; }
    PlaneWave          &planewave()   { return m_planewave; }
    QVector<Probe>     &probes()      { return m_probes; }
    PostOpts           &post()        { return m_post; }
    OpticalOpts        &optical()     { return m_optical; }
    AcousticOpts       &acoustic()    { return m_acoustic; }
    OperaAcousticSettings &operaAcoustic() { return m_operaAcoustic; }
    UnderwaterOpts     &underwater()  { return m_underwater; }
    Tidy3dOpts         &tidy3d()      { return m_tidy3d; }

    const GeneralOpts       &general()    const { return m_general; }
    const MeshAxis          &mesh(int axis) const { return m_mesh[axis]; }
    const QVector<Material> &materials()  const { return m_materials; }
    const QVector<Load>     &loads()      const { return m_loads; }
    const QVector<Geometry> &geometries() const { return m_geometries; }
    const QVector<Feed>     &feeds()      const { return m_feeds; }
    const PlaneWave         &planewave()  const { return m_planewave; }
    const QVector<Probe>    &probes()     const { return m_probes; }
    const PostOpts          &post()       const { return m_post; }
    const OpticalOpts       &optical()    const { return m_optical; }
    const AcousticOpts      &acoustic()   const { return m_acoustic; }
    const OperaAcousticSettings &operaAcoustic() const { return m_operaAcoustic; }
    const UnderwaterOpts    &underwater() const { return m_underwater; }
    const Tidy3dOpts        &tidy3d()     const { return m_tidy3d; }

    // Lines from a loaded .ofd that the GUI doesn't model — preserved verbatim
    // on save so a hand-edited file survives a GUI round trip.
    QStringList &extraLines() { return m_extraLines; }
    const QStringList &extraLines() const { return m_extraLines; }

    QString filePath() const { return m_filePath; }
    void    setFilePath(const QString &p) { m_filePath = p; }

    qint64 totalCells() const;
    double estimatedMemoryMB() const;
    double courantDt() const;    // CFL timestep estimate from min spacing

    void clear();                // reset to the default new-project state

    // Persistence (implemented in io/OfdIO.cpp; these are thin wrappers)
    bool load(const QString &path, QString *err = nullptr);  // .ofd + .ofdx
    bool save(const QString &path, QString *err = nullptr);

public slots:
    void setActiveDomain(ofd::Domain d);
    void touch() { emit changed(); }   // call after editing through references

signals:
    void domainChanged(ofd::Domain);
    void changed();    // structural change → viewport / tree / statusbar refresh
    void loaded();     // file (re)loaded → all tabs re-read their widgets
    void materialsEdited();  // 別タブが materials() を書き換えた → MaterialTab 再表示

private:
    Domain  m_domain = Domain::EM;
    QString m_filePath;

    GeneralOpts        m_general;
    MeshAxis           m_mesh[3];
    QVector<Material>  m_materials;
    QVector<Load>      m_loads;
    QVector<Geometry>  m_geometries;
    QVector<Feed>      m_feeds;
    PlaneWave          m_planewave;
    QVector<Probe>     m_probes;
    PostOpts           m_post;
    OpticalOpts        m_optical;
    AcousticOpts       m_acoustic;
    OperaAcousticSettings m_operaAcoustic;
    UnderwaterOpts     m_underwater;
    Tidy3dOpts         m_tidy3d;
    QStringList        m_extraLines;
};

} // namespace ofd
