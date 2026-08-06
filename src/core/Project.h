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

// RCWA 層スタックの 1 層。OpenRCWA の `rcwalayer = <eps1> <eps2> <fill>
// <thickness[m]>` に 1:1 対応する (sol/input_data.c)。GUI は nm で保持し、
// .ofd 書き出し時に ×1e-9 で m へ換算する。
// 先頭と末尾の層は半無限層として扱われ、厚みはカーネルで無視される。
struct RcwaLayer {
    double eps1 = 1.0;         // 格子の材質1の比誘電率 (> 0)
    double eps2 = 1.0;         // 格子の材質2の比誘電率 (> 0)
    double fill = 0.5;         // フィルファクタ [0,1]
    double thickness_nm = 0;   // 厚み [nm] (≥ 0、半無限層は 0)
};

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
    // orcwa へ渡す実際の層スタック。既定は空 = 従来どおり rcwalayer 行を
    // 出力しない (rcwaLayers は GUI 上の「層分割数」で意味が異なるため、
    // 後方互換のため残す — 改名も削除もしない)。
    QVector<RcwaLayer> rcwaLayerList;

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
    // BPF 設計目標 (挿入損失 / 阻止域減衰) — .ofdx "bpf" への追加キー
    double  bpfIL_dB = 0.5;          // 挿入損失 IL [dB]
    double  bpfStop_dB = 40.0;       // 阻止域減衰 [dB]

    // Ring
    double  ringRadius_um = 5.0;
    double  ringGap_nm = 200.0;
    bool    ringThruPort = true;     // スルーポート出力
    bool    ringDropPort = true;     // ドロップポート出力

    // ── 光解析モード別設定 (.ofdx "optical" への追加キー。カーネル入力
    //    (.ofd) には出力しない — 設計目標 / 解析対象の記録として保存する) ──
    // 導波路モード解析 (解くモードの選択と目標損失)
    bool    wgTE0 = true, wgTE1 = false;   // TE モード
    bool    wgTM0 = false, wgTM1 = false;  // TM モード
    double  wgLoss_dBcm = 0.3;             // 損失 [dB/cm]
    // MZI 干渉計
    double  mziDeltaL_um = 50.0;     // アーム長差 ΔL [μm]
    bool    mziThermo = true;        // 位相シフタ: 熱光学
    bool    mziElectro = false;      // 位相シフタ: 電気光学
    // メタサーフェス
    double  metaPeriod_nm = 400.0;   // 格子周期 [nm]
    int     metaShape = 0;           // 0=Pillar 1=Hole 2=Cross
    int     metaPhase = 0;           // 0=等位相 (flat lens) 1=偏向 2=渦光 (OAM)
    // フォトニック結晶
    int     phcLattice = 0;          // 0=三角格子 1=正方格子 2=ハニカム
    double  phcA_nm = 430.0;         // 格子定数 a [nm]
    double  phcRoverA = 0.30;        // 穴半径 r/a (無次元)
    bool    phcBand = true;          // バンド構造解析
    bool    phcDefect = false;       // 欠陥モード
    // 近接場/遠方場変換 (ofd_post 連携は未実装 — 設定の保存のみ)
    int     nfffSurface = 0;               // 変換面 0=直方体 1=球面
    double  nfffDistance_lambda = 1000.0;  // 観測距離 [λ]
    // S パラメータ抽出 (Touchstone 出力は未実装 — 設定の保存のみ)
    int     spPorts = 2;             // ポート数
    int     spPortIn = 1;            // S21 の入力ポート番号 (1 始まり)
    int     spPortOut = 2;           // S21 の出力ポート番号 (1 始まり)
    bool    spS11 = true, spS21 = true;    // 抽出する成分
    bool    spPhase = true;          // 位相情報を含む
    bool    spGroupDelay = false;    // 群遅延

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

// TPA / パワースイープ入力の妥当性判定 (GUI の入力検証と selftest で共用)。
// 不正な設定をカーネルへ渡さないため、GUI は false のとき対応する有効フラグ
// (tpaEnabled / powerSweepEnabled) を落とす — 「警告が出ているのに既定値の
// β で走る」状態を作らない。
bool isValidTpaBeta(double beta_cmGW);                       // β > 0
bool isValidPowerSweepRange(double pmin_W, double pmax_W);   // 0 < Pmin ≤ Pmax

// RCWA 層の妥当性 (GUI の入力検証と .ofd 書き出しゲートで共用)。
// eps1 > 0, eps2 > 0, 0 ≤ fill ≤ 1, thickness_nm ≥ 0。
bool isValidRcwaLayer(const RcwaLayer &layer);
// 層スタック全体が orcwa へ渡せる状態か (空でなく、全層が有効)。
// false のとき .ofd へ RCWA 設定を一切書き出さない — 不正な設定のまま
// カーネルを走らせない (.claude/rules/gui.md)。
bool isValidRcwaStack(const QVector<RcwaLayer> &layers);

// ── ディスプレイ / AR-VR 光学 (.ofdx "display_optics") ──────────────────────
// DisplayOpticsTab のフォーム値。`optics/DisplayMetrics` の解析式に渡して
// 評価量 (視野角・アイボックス・シースルー透過率・取り出し効率・環境光
// コントラスト) を計算する。カーネル入力 (.ofd) には出力しない。
// 既定値のままなら .ofdx にキー自体を書かない (旧ファイルとバイト一致)。
struct DisplayOpticsOpts {
    int    device = 0;              // 0=AR導波路 1=OLED 2=microLED 3=LCD/偏光

    // AR 導波路コンバイナ
    int    wgType = 0;              // 0=SRG 1=VHG 2=PVG 3=幾何
    double subThick_mm = 0.7;       // 基板厚
    double subIndex = 1.80;         // 基板屈折率 n
    double gratPeriod_nm = 385.0;   // 格子周期 Λ
    double gratDepth_nm = 220.0;    // 格子深さ
    double gratSlant_deg = 30.0;    // 斜め角
    bool   threeGratings = true;
    bool   rcwaOptimize = false;    // RCWA 最適化連携 (未実装 — 記録のみ)
    double designLambda_nm = 550.0; // 設計波長
    double guideMaxAngle_deg = 80.0;// 導波角の実装上限
    double outcouplerLen_mm = 30.0; // 出射格子の長さ (瞳拡大方向)
    double eyeRelief_mm = 18.0;     // アイレリーフ
    // 設計目標 (判定バッジのしきい値)
    double fovTarget_deg = 40.0;
    double eyeboxTarget_mm = 10.0;
    double seeThroughTarget_pct = 80.0;

    // OLED 光取り出し
    bool   bottomEmission = false, topEmission = true, microcavity = true;
    bool   sepIqe = true, sepSpp = true, sepWaveguide = true;
    int    outcouplingStruct = 0;   // 0=なし 1=マイクロレンズ 2=散乱層 3=PhC
    double oledIndex = 1.75;        // 有機層/基板の屈折率
    double oledIqe = 0.90;          // 内部量子効率 (電荷バランス込み)

    // microLED
    double chipSize_um = 5.0;
    bool   sidewallRecomb = true, sidewallDbr = true, directional = false;
    double mlIndex = 2.45;          // GaN の屈折率
    double mlIqe = 0.80;            // 側壁再結合を除いた内部量子効率
    double mlSurfVel_cm_s = 1.0e4;  // 表面再結合速度 S
    double mlLifetime_ns = 10.0;    // バルク実効寿命 τ

    // LCD / 偏光系
    int    lcdMode = 2;             // 0=TN 1=IPS 2=VA
    bool   lcAnisotropy = true, compFilm = true;
    double lcdPeakLum_cdm2 = 500.0; // 白輝度
    double lcdDarkroomCr = 5000.0;  // 暗室コントラスト比
    double lcdAmbient_lx = 200.0;   // 環境照度
    double lcdReflectance = 0.045;  // 画面の拡散反射率
};

// ── 照明光学・測色 (.ofdx "illumination") ───────────────────────────────────
// IlluminationTab のフォーム値。スペクトルモデルは解析的に定義され、
// `optics/Colorimetry` で XYZ → 色度 (x,y)/(u',v')・CCT・Duv・発光効率を
// 計算する。カーネル入力 (.ofd) には出力しない。
// 既定値のままなら .ofdx にキー自体を書かない (旧ファイルとバイト一致)。
struct IlluminationOpts {
    int     app = 0;            // 0=LED照明 1=車載 2=バックライト 3=太陽光集光
    int     srcModel = 1;       // 0=ランバート 1=レイデータ 2=LEDチップ
    QString rayFile = QStringLiteral("CREE_XPG3_5000K.ray");
    // スペクトルモデル: 0=白色LED(青+蛍光体) 1=RGB3チップ
    //                   2=フルスペクトル(黒体) 3=単色
    int     spectrum = 0;
    double  flux_lm = 1200.0;
    double  rays = 5.0e6;

    // 光学系 (レイトレース未実装 — 記録のみ)
    bool    reflector = true, tirLens = false, diffuser = true;
    bool    lightGuide = false, phosphor = true;
    int     surface = 2;        // 0=鏡面 1=拡散 2=BSDF実測 3=ABGモデル

    // 白色 LED = 青 LED ピーク + 蛍光体の broad band (ガウシアン 2 ローブ)。
    // ratio はピーク強度比 (放射束比は ratio × FWHM に比例)。
    // 既定値は CCT ≈ 5050 K・Duv ≈ +0.0007 (黒体軌跡上) になる組み合わせ。
    double  bluePeak_nm = 450.0, blueFwhm_nm = 20.0;
    double  phosPeak_nm = 570.0, phosFwhm_nm = 100.0;
    double  phosRatio = 0.610;  // 蛍光体/青 のピーク強度比
    // RGB 3 チップ (ガウシアン 3 ローブ)。既定値は 3 原色の合成が
    // 5000 K 黒体の色度に一致する強度比。
    double  rPeak_nm = 630.0, rFwhm_nm = 20.0, rRatio = 2.094;
    double  gPeak_nm = 525.0, gFwhm_nm = 35.0, gRatio = 1.000;
    double  bPeak_nm = 460.0, bFwhm_nm = 22.0, bRatio = 0.872;
    // フルスペクトル = 黒体放射
    double  blackbody_K = 5000.0;
    // 単色
    double  monoPeak_nm = 550.0, monoFwhm_nm = 2.0;

    // 設計目標 (判定バッジのしきい値)
    double  cctTarget_K = 5000.0, cctTol_K = 300.0;
    double  duvTol = 0.006;
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

// 騒音源内訳の1行 (room-acoustics.jsx の騒音源テーブル)。
// 寄与レベルは A 特性 [dB(A)]、対策は自由記述 ("—" = 対策なし)。
struct NoiseSourceRow {
    bool    enabled = true;
    QString name;
    double  level_dBA = 0;
    QString measure;
};

// 新規プロジェクト / .ofdx 欠落時の既定 4 行 (mock room-acoustics.jsx:697-709)。
QVector<NoiseSourceRow> defaultNoiseSources();

// 音源リストの1行 (AcousticSourceTab「音源一覧」)。室内音響ではスピーカー、
// 水中音響ではソナー送信源を表す (ドメインごとに別リストを持つ)。
// level_dB は室内 = 基準 SPL @1m [dB]、水中 = 音源レベル SL [dB re μPa·m]。
// aim / signal は自由記述 ("-Z 30°" / "chirp 3-5kHz" / WAV ファイル名)。
struct AcousticSourceRow {
    enum Kind { Omni = 0, Cardioid = 1, Bipolar = 2, Directional = 3 };
    bool    enabled = true;
    QString name;
    int     kind = Omni;
    double  x_m = 0.0, y_m = 0.0, z_m = 0.0;
    QString aim;
    QString signal;
    double  level_dB = 94.0;
};

// 新規プロジェクト / .ofdx 欠落時の既定音源。値は「初期値」であり測定結果では
// ない (利用者が表で編集し .ofdx に保存される)。
inline QVector<AcousticSourceRow> defaultAcousticSources()
{
    auto row = [](bool on, const char *name, int kind,
                  double x, double y, double z, const char *aim, double lv) {
        AcousticSourceRow r;
        r.enabled = on;
        r.name = QString::fromUtf8(name);
        r.kind = kind;
        r.x_m = x; r.y_m = y; r.z_m = z;
        r.aim = QString::fromUtf8(aim);
        r.level_dB = lv;
        return r;
    };
    // ステレオ + センターの基本構成 (信号は未選択 = 空)
    return {
        row(true, "L_main",  AcousticSourceRow::Cardioid, -3.0, 4.5, 5.0,
            "-Z 30°", 94.0),
        row(true, "R_main",  AcousticSourceRow::Cardioid,  3.0, 4.5, 5.0,
            "-Z 30°", 94.0),
        row(true, "C_voice", AcousticSourceRow::Cardioid,  0.0, 4.0, 5.5,
            "-Z", 88.0),
    };
}

inline QVector<AcousticSourceRow> defaultSonarSources()
{
    auto row = [](bool on, const char *name, int kind,
                  double x, double y, double z, const char *aim,
                  const char *sig, double lv) {
        AcousticSourceRow r;
        r.enabled = on;
        r.name = QString::fromUtf8(name);
        r.kind = kind;
        r.x_m = x; r.y_m = y; r.z_m = z;
        r.aim = QString::fromUtf8(aim);
        r.signal = QString::fromUtf8(sig);
        r.level_dB = lv;
        return r;
    };
    return {
        row(true,  "TX_sonar",  AcousticSourceRow::Directional,
            -1200.0, 50.0, 0.0, "+X", "chirp 3-5kHz", 220.0),
        row(false, "TX_pinger", AcousticSourceRow::Omni,
            0.0, 100.0, 0.0, "—", "tone 12kHz", 195.0),
    };
}

// 受音点 (マイクアレイ) の1行 (AcousticTab の受音点表)。
// 位置は室内座標 [m]、type は 0=Omni 1=Stereo 2=Binaural。
struct ReceiverRow {
    bool    enabled = true;
    double  x = 0.0, y = 1.2, z = 0.0;   // 位置 [m]
    int     type = 0;                    // 0=Omni 1=Stereo 2=Binaural
    QString name;
    // この受音点の RIR WAV パス (可聴化タブの一括レンダリング入力)。
    // .ofdx "acoustic.receivers[].rir_file" — 追加キーのみ。欠落時は空
    // (未指定 = 一括レンダリングの対象外。ソルバ実行か実測 WAV を指定する)
    QString rirFile;
};

// 受音点リストの既定値 (count 点)。先頭 4 点は mock (tabs.jsx AcousticTab)
// の受音点表そのもの、5 点目以降は 2 m 間隔で後方へ並べる。
// あくまで「初期値」であり、GUI の表で編集・追加・削除できる。
// 行の追加 (末尾 1 点だけ欲しい場合) は defaultReceivers(n+1).last() で得る。
inline QVector<ReceiverRow> defaultReceivers(int count)
{
    struct Def { double x, y, z; int type; const char *name; };
    static const Def kDef[4] = {
        {  0.0, 1.2,  8.0, 0, "P1 中央" },
        { -2.0, 1.2,  8.0, 0, "P2 左"   },
        {  2.0, 1.2,  8.0, 0, "P3 右"   },
        {  0.0, 1.2, 14.0, 1, "P4 後方" },
    };
    QVector<ReceiverRow> v;
    for (int i = 0; i < count; ++i) {
        ReceiverRow r;
        if (i < 4) {
            r.x = kDef[i].x; r.y = kDef[i].y; r.z = kDef[i].z;
            r.type = kDef[i].type;
            r.name = QString::fromUtf8(kDef[i].name);
        } else {
            r.x = 0.0; r.y = 1.2; r.z = 14.0 + 2.0 * (i - 3);
            r.type = 0;
            r.name = QStringLiteral("P%1").arg(i + 1);
        }
        v.push_back(r);
    }
    return v;
}

struct AcousticOpts {
    bool    rt60 = true, c80 = true, d50 = false, sti = false, edt = false;
    bool    impulseResponse = true;
    bool    auralization = false;
    int     sampleRate = 48000;     // WAV出力サンプリング周波数
    QString srcDirectivity = "omni"; // omni / cardioid / speaker
    double  srcSPL_dB = 94.0;
    int     micCount = 1;

    // ── AcousticTab 追加設定 (.ofdx "acoustic" への追加キー。既定値は
    //    mock tabs.jsx AcousticTab のまま — 旧ファイル互換) ──
    bool    lf = false;              // LF (側方音エネルギー) 指標
    double  srcX_m = -3.0;           // 音源位置 x [m]
    double  srcY_m = 1.6;            // 音源位置 y [m]
    double  srcZ_m = 5.0;            // 音源位置 z [m]
    double  srcAimTheta_deg = 90.0;  // 音源の向き θ [deg]
    double  srcAimPhi_deg   = 0.0;   // 音源の向き φ [deg]
    int     analysisType = 0;        // 解析タイプ 0=IRF 1=RT60 2=STI
    bool    thirdOctave = true;      // 1/3 オクターブ帯域
    int     bandRange = 2;           // 対象帯域 0=125Hz~ 1=500Hz~2k 2=125Hz~16k

    // ── ホール解析 (RoomAcousticsTab) ──
    double  roomL = 30.0, roomW = 20.0, roomH = 12.0;  // シューボックス [m]
    double  volume = 12000.0;       // 室容積 V [m³] (寸法と独立に編集可)
    double  surface = 3800.0;       // 総表面積 S [m²]
    int     occupancy = 2;          // 0=空席, 1=半分, 2=満席 (客席行のα係数)
    int     rtFormula = 1;          // 0=Sabine, 1=Eyring, 2=Fitzroy (非均一)
    QVector<AbsorptionRow> absorption;   // 吸音バジェット
    double  noiseLevels[7] = { 42, 38, 33, 28, 24, 21, 18 };  // 63..4kHz [dB]
    // 騒音源内訳 (.ofdx "acoustic.noise_sources" — 追加キー。欠落時は既定4行)
    QVector<NoiseSourceRow> noiseSources = defaultNoiseSources();
    // 受音点リスト (.ofdx "acoustic.receivers" — 追加キー)。AcousticTab の
    // 受音点表そのもので、受音点数スピン (micCount) と同一データ:
    // 常に receivers.size() == micCount を保つ (OfdIO の読み込みと
    // AcousticTab の編集経路の両方で維持する)。キーが無い旧ファイルは
    // mic_count 個の既定点で埋める (OfdIO)。
    QVector<ReceiverRow> receivers = defaultReceivers(1);
    // 音源リスト (.ofdx "acoustic.sources" — 追加キー。欠落時は既定 3 行)。
    // AcousticSourceTab の「音源一覧」表そのもの。統計推定 (RoomAcousticsTab)
    // は単一音源 (srcX_m/srcY_m/srcZ_m/srcSPL_dB) を使うため、この一覧は
    // 現状「配置の記録」であり計算には渡されない (タブに注記あり)。
    QVector<AcousticSourceRow> sources = defaultAcousticSources();
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
    // dBFS → dB SPL の換算オフセット。calibrationState==Absolute のときのみ
    // 分析に渡される (それ以外では 0 が渡る — QtAcousticAdapter)。
    double  calibrationOffsetDb = 0.0;
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

    // 音響ソルバー連携 (AcousticSolverTab, .ofdx "opera_analysis/solver")。
    // backend は AcousticBackend (kernel/AcousticRunner.h) と同順の int
    // (0=None 1=MeasuredRir 2=Statistical 3=ExternalFDTD 4=ExternalGeometric)
    int     solverBackend = 3;
    QString solverExecutable;     // 空 = 探索順による自動解決
    int     solverThreads = 4;    // OMP_NUM_THREADS
    int     solverProcesses = 1;  // >1 で mpiexec -n
};

// ── 水中音響ドメイン拡張 (.ofdx) ────────────────────────────────────────────
struct SSPPoint { double depth_m; double c_mps; };

// 伝搬経路に沿った海底地形の 1 点 (BELLHOP の .bty 1 行に対応)。
// range_km は音源からの水平距離 [km]、depth_m は海面からの水深 [m]。
struct BathyPoint { double range_km; double depth_m; };

struct UnderwaterOpts {
    double  waterTemp_C = 15.0;
    double  salinity_psu = 34.5;
    QVector<SSPPoint> ssp;
    bool    sofar = false;
    QString bottomType = "sand";
    double  bottomC_mps = 1650.0;
    double  bottomRho_kgm3 = 1900.0;
    // 底質の吸収係数 α [dB/λ] — BELLHOP ハーフスペース行の減衰
    // (SSPOPT 'W' = dB/wavelength と整合)。既定 0.5 は従来 BellhopIO に
    // ハードコードされていた砂〜シルト底の代表値 (既定のままなら .env は
    // 従来とバイト一致)。
    double  bottomAlpha_dBlambda = 0.5;
    double  sonarFreq_kHz = 3.5;
    double  sonarSL_dB = 220.0;
    double  rangeMax_km = 50.0;
    // ソナー送信源リスト (.ofdx "underwater.sources" — 追加キー。欠落時は
    // 既定 2 行)。AcousticSourceTab の音源一覧 (水中ドメイン表示) の実体。
    QVector<AcousticSourceRow> sources = defaultSonarSources();

    // ── 測点と伝搬経路 (.ofdx "underwater.site" — 追加キー) ─────────────────
    // OceanEnvironmentTab の緯度・経度・伝搬方位。地形断面のサンプリング経路
    // (測点から方位へ rangeMax_km だけ伸ばした大圏) を決める。
    // 既定値のままなら .ofdx へ書かない (旧ファイルとバイト一致)。
    double  siteLat_deg = 35.0;
    double  siteLon_deg = 140.0;
    double  trackBearing_deg = 90.0;   // 真北 0°・時計回り (90° = 東)

    // ── 海底地形断面 (.ofdx "underwater.bathymetry" — 追加キー) ─────────────
    // 空なら平坦海底 (従来動作) で、.bty も書き出さない。
    QVector<BathyPoint> bathymetry;
    // 断面の出所 (表示と再現性のための記録)。"" = なし、"synthetic" = 合成、
    // それ以外はデータセットのファイル名。
    QString bathySource;

    // ── Bellhop 実行設定 (.ofdx "underwater.bellhop" — 追加キー) ────────────
    // 既定値は従来 BellhopIO がハードコードしていた値なので、触らなければ
    // .env は従来とバイト一致になる。
    QString runMode  = "coherent";    // coherent/incoherent/semicoherent/
                                      // eigenray/ray/arrivals
    QString beamType = "geometric";   // geometric/gaussian/hat/
                                      // cartesian/raycentered
    int     numRays = 0;              // NBEAMS (0 = カーネル自動)
    double  angleMin_deg = -45.0;
    double  angleMax_deg =  45.0;
    double  srcDepth_m = 0.0;         // 0 = 自動 (水深の 10%、従来動作)
    int     numRcvDepth = 201;        // NRD
    int     numRcvRange = 501;        // NR
};

// ── メッシュ細分化領域 (.ofdx "geometry.refine_regions") ────────────────────
// GeometryTab「細分化領域」表の 1 行。局所的に格子を細かく (あるいは粗く)
// したい直方体領域の *定義* で、利用者が入力するデータそのもの。
// 細分化エンジン (サブグリッド / AMR) は未実装なので、現状この定義は
//   (a) .ofdx への保存、(b) 現在の基本格子から数えたセル増加量の見積り
// にのみ使われ、実際の格子・.ofd の出力は一切変えない (タブに注記あり)。
// 既定は空リスト = キーを書き出さない (旧 .ofdx とバイト一致)。
struct RefineRegion {
    bool    enabled = true;
    QString name;
    double  min_m[3] = { 0, 0, 0 };   // 領域下限 (x,y,z) [m]
    double  max_m[3] = { 0, 0, 0 };   // 領域上限 (x,y,z) [m]
    double  ratio = 3.0;              // 分割比 r (1 セル → r³ セル。<1 で粗く)
};

// ── 回路系電磁解析のポート定義 (.ofdx "circuit.ports") ──────────────────────
// CircuitSolversTab「ポート定義」表の 1 行。PEEC / FEM 抽出のポート (励振点と
// その基準導体) の *定義* で、利用者が入力するデータそのもの。
// 抽出エンジンは別リポジトリのカーネル側にあり GUI からの起動は未実装なので、
// 現状この定義は .ofdx への保存だけに使われる (タブに注記あり)。
struct CircuitPortRow {
    enum Kind { Lumped = 0, Probe = 1 };   // 集中ポート / 内部観測
    bool    enabled = true;
    QString name;
    int     kind = Lumped;
    QString net;      // 接続ネット名 (自由記述)
    QString ref;      // 基準導体名 (自由記述)
};

// 新規プロジェクト / .ofdx 欠落時の既定 3 行。値は「初期値」であって
// 抽出結果ではない (利用者が表で編集し .ofdx に保存される)。
QVector<CircuitPortRow> defaultCircuitPorts();

// ── フォトニック回路のネットリスト (.ofdx "schematic.netlist") ──────────────
// SchematicTab「ネットリスト」表の 1 行。要素間の接続 (from/to) と、その
// 接続で想定する波長範囲の自由記述。回路シミュレータは未実装なので、
// この表は接続の *記録* であり計算には渡されない (タブに注記あり)。
struct PhotonicNetRow {
    bool    enabled = true;
    QString from;         // 例 "LASER1.out"
    QString to;           // 例 "MZI1.in1"
    QString wavelength;   // 例 "1530~1570nm" / "—"
};

// 新規プロジェクト / .ofdx 欠落時の既定 5 行 (初期値)。
QVector<PhotonicNetRow> defaultPhotonicNetlist();

// ── モニター定義 (.ofdx "monitors") ─────────────────────────────────────────
// MonitorsTab「モニター一覧」表の 1 行。利用者が入力するデータそのもので、
// 追加・削除・編集が .ofdx へ保存される。type は MonitorsTab のモニタータイプ
// ID (ASCII の安定語 — 表示名は言語で変わるので保存しない)。
// モニター駆動の後処理は未実装なので、この表は観測点の *定義* であり
// カーネル入力 (.ofd) には出力されない (タブに注記あり)。
struct MonitorRow {
    bool    enabled = true;
    QString type;     // "point" / "plane" / "ntff" … (MonitorsTab kAddTypes[].id)
    QString name;     // 例 "E_probe"
    QString region;   // 位置・範囲 (自由記述, 例 "(0.02, 0, 0.001)")
    QString band;     // 周波数/波長・周波数帯 (自由記述, 例 "2.5 GHz")
};

inline bool operator==(const MonitorRow &a, const MonitorRow &b)
{
    return a.enabled == b.enabled && a.type == b.type && a.name == b.name
        && a.region == b.region && a.band == b.band;
}

// 新規プロジェクト / .ofdx 欠落時の既定行 (ドメイン別の「初期値」であって
// 実行結果ではない)。値は mock (ansys-tabs.jsx MonitorsTab) の初期表示。
// 水中音響は室内音響と同じ初期構成を使う。
inline QVector<MonitorRow> defaultMonitors(Domain d)
{
    auto row = [](bool on, const char *type, const char *name,
                  const char *region, const char *band) {
        MonitorRow r;
        r.enabled = on;
        r.type = QString::fromUtf8(type);
        r.name = QString::fromUtf8(name);
        r.region = QString::fromUtf8(region);
        r.band = QString::fromUtf8(band);
        return r;
    };
    if (d == Domain::Optical)
        return {
            row(true,  "plane",  "T_drop",      "Z=2.5μm 面",
                "1500~1600 nm (201pt)"),
            row(true,  "mode",   "thru_mode",   "X=10μm, TE₀",  "1550 nm"),
            row(true,  "volume", "E_field_3D",  "[-2,2]³ μm",   "1550 nm"),
            row(false, "movie",  "propagation", "Y=0 面",       "時間 0~50fs"),
            row(false, "ntff",   "far_field",   "θ:-90~90° φ:0~360°", "1550 nm"),
        };
    if (d == Domain::Acoustic || d == Domain::Underwater)
        return {
            row(true,  "point", "P1_center",    "(0, 1.2, 8)", "63Hz~16kHz"),
            row(true,  "line",  "mic_array",    "Y=1.2m 線",   "全帯域"),
            row(true,  "irf",   "IRF_response", "P1, P2, P3",  "時間 0~3s"),
            row(false, "plane", "SPL_floor",    "Y=1.2m 面",   "1kHz"),
        };
    return {
        row(true,  "point",  "E_probe",   "(0.02, 0, 0.001)", "2.5 GHz"),
        row(true,  "global", "impedance", "feed #1",          "2~3 GHz"),
        row(true,  "plane",  "E_surface", "Z=1mm 面",         "2.5 GHz"),
        row(false, "ntff",   "far_field", "全方向",           "2.5 GHz"),
    };
}

// まだ利用者が編集していない (どれかのドメインの既定そのままの) 一覧か。
// ドメイン切替時に「既定のまま」のときだけ新ドメインの既定へ差し替えるために使う
// (利用者が編集した一覧は勝手に捨てない)。
inline bool isDefaultMonitorSet(const QVector<MonitorRow> &rows)
{
    for (Domain d : { Domain::EM, Domain::Optical, Domain::Acoustic,
                      Domain::Underwater })
        if (rows == defaultMonitors(d)) return true;
    return false;
}

// ── 解析グループ (.ofdx "analysis_groups") ──────────────────────────────────
// AnalysisGroupsTab「登録済みグループ」表の 1 行。モニターの組と、その組から
// 得たい出力の自由記述。ポスト処理スクリプトの実行は未実装なので、この表は
// 解析単位の *定義* であり計算には渡されない (タブに注記あり)。
struct AnalysisGroupRow {
    bool    enabled = false;
    QString name;      // 例 "Antenna patterns"
    QString monitors;  // 含まれるモニター (自由記述, 例 "6 box monitors")
    QString output;    // 出力 (自由記述, 例 "遠方界・ゲイン・効率")
};

inline bool operator==(const AnalysisGroupRow &a, const AnalysisGroupRow &b)
{
    return a.enabled == b.enabled && a.name == b.name
        && a.monitors == b.monitors && a.output == b.output;
}

// 新規プロジェクト / .ofdx 欠落時の既定行 (ドメイン別の「初期値」)。
// 値は mock (ansys-workflow.jsx AnalysisGroupsTab) の初期表示で、
// 上位 2 行だけ有効というのも mock のまま。
inline QVector<AnalysisGroupRow> defaultAnalysisGroups(Domain d)
{
    auto row = [](bool on, const char *name, const char *mons,
                  const char *out) {
        AnalysisGroupRow r;
        r.enabled = on;
        r.name = QString::fromUtf8(name);
        r.monitors = QString::fromUtf8(mons);
        r.output = QString::fromUtf8(out);
        return r;
    };
    if (d == Domain::Optical)
        return {
            row(true,  "Q-factor analyzer",     "3 time monitors",
                "Q値, 共振λ, FWHM"),
            row(true,  "Transmission spectrum", "2 frequency monitors",
                "T(λ), R(λ)"),
            row(false, "Mode source coupler",   "1 mode monitor", "結合効率, η"),
            row(false, "NTFF analyzer",         "6 box monitors", "遠方界 E(θ,φ)"),
            row(false, "S-matrix extractor",    "N port monitors",
                ".s2p Touchstone"),
            row(false, "Polarization analyzer", "1 plane monitor",
                "Stokes parameters"),
        };
    if (d == Domain::Acoustic)
        return {
            row(true,  "RT60 calculator",   "3 point monitors",
                "RT60(オクターブ)"),
            row(true,  "Clarity (C80/D50)", "3 point monitors", "C80, D50, STI"),
            row(false, "Auralization",      "binaural monitor",
                ".wav (HRTF畳み込み)"),
            row(false, "Spatial impulse",   "line monitors", "反射面別エネルギー"),
        };
    if (d == Domain::Underwater)
        return {
            row(true,  "TL analyzer",     "range monitors", "TL(range, depth)"),
            row(true,  "Eigenray finder", "point monitors", "τ, θ, E"),
            row(false, "Beam pattern",    "sphere monitor", "ソナー指向性 B(θ,φ)"),
        };
    return {
        row(true,  "Antenna patterns", "6 box monitors",  "遠方界・ゲイン・効率"),
        row(true,  "S-parameter",      "N port monitors", "S11/S21 .sNp"),
        row(false, "VSWR",             "feed monitor",    "VSWR(f)"),
        row(false, "SAR analyzer",     "volume monitor",  "局所/全身SAR"),
    };
}

inline bool isDefaultAnalysisGroupSet(const QVector<AnalysisGroupRow> &rows)
{
    for (Domain d : { Domain::EM, Domain::Optical, Domain::Acoustic,
                      Domain::Underwater })
        if (rows == defaultAnalysisGroups(d)) return true;
    return false;
}

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
    DisplayOpticsOpts  &displayOptics() { return m_displayOptics; }
    IlluminationOpts   &illumination()  { return m_illumination; }
    AcousticOpts       &acoustic()    { return m_acoustic; }
    OperaAcousticSettings &operaAcoustic() { return m_operaAcoustic; }
    UnderwaterOpts     &underwater()  { return m_underwater; }
    Tidy3dOpts         &tidy3d()      { return m_tidy3d; }
    QVector<RefineRegion> &refineRegions() { return m_refineRegions; }
    QVector<CircuitPortRow> &circuitPorts() { return m_circuitPorts; }
    QVector<PhotonicNetRow> &photonicNetlist() { return m_photonicNet; }
    QVector<MonitorRow>     &monitors()       { return m_monitors; }
    QVector<AnalysisGroupRow> &analysisGroups() { return m_analysisGroups; }

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
    const DisplayOpticsOpts &displayOptics() const { return m_displayOptics; }
    const IlluminationOpts  &illumination()  const { return m_illumination; }
    const AcousticOpts      &acoustic()   const { return m_acoustic; }
    const OperaAcousticSettings &operaAcoustic() const { return m_operaAcoustic; }
    const UnderwaterOpts    &underwater() const { return m_underwater; }
    const Tidy3dOpts        &tidy3d()     const { return m_tidy3d; }
    const QVector<RefineRegion> &refineRegions() const { return m_refineRegions; }
    const QVector<CircuitPortRow> &circuitPorts() const { return m_circuitPorts; }
    const QVector<PhotonicNetRow> &photonicNetlist() const { return m_photonicNet; }
    const QVector<MonitorRow>     &monitors()       const { return m_monitors; }
    const QVector<AnalysisGroupRow> &analysisGroups() const { return m_analysisGroups; }

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
    DisplayOpticsOpts  m_displayOptics;
    IlluminationOpts   m_illumination;
    AcousticOpts       m_acoustic;
    OperaAcousticSettings m_operaAcoustic;
    UnderwaterOpts     m_underwater;
    Tidy3dOpts         m_tidy3d;
    QVector<RefineRegion> m_refineRegions;   // 既定は空 (.ofdx へ書かない)
    // 既定行のままなら .ofdx へキーを書かない (旧ファイルとバイト一致)
    QVector<CircuitPortRow> m_circuitPorts = defaultCircuitPorts();
    QVector<PhotonicNetRow> m_photonicNet = defaultPhotonicNetlist();
    // モニター定義 / 解析グループも既定のままなら .ofdx へキーを書かない。
    // 既定は「新規プロジェクトのドメイン (EM)」のもので、ドメイン切替時に
    // 既定のままなら各タブが新ドメインの既定へ差し替える。
    QVector<MonitorRow>       m_monitors = defaultMonitors(Domain::EM);
    QVector<AnalysisGroupRow> m_analysisGroups = defaultAnalysisGroups(Domain::EM);
    QStringList        m_extraLines;
};

} // namespace ofd
