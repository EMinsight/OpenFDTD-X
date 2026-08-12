// SchematicTab.cpp
#include "SchematicTab.h"
#include "../core/Project.h"
#include "../core/ReceiverNoise.h"
#include "../optics/PhotonicCircuit.h"
#include "../optics/CircuitImpulse.h"
#include "../widgets/MiniPlot.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QStandardItemModel>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用の翻訳キー (接頭辞 sch_) ────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("sch_sim_section", "フォトニック回路シミュレーション",
              "Photonic circuit simulation");
    I18n::reg("sch_sim_hint",
              "Ansys INTERCONNECT / Synopsys PhotonicCAD 風の回路エディタ。\n"
              "回路図キャンバスからの自動生成は未実装ですが、下の「素子応答」で"
              "リング共振器・MZI の波長応答を S 行列で計算できます。",
              "Circuit editor in the style of Ansys INTERCONNECT / Synopsys "
              "PhotonicCAD.\nBuilding the netlist from a canvas is not "
              "implemented, but the element-response panel below computes the "
              "wavelength response of a ring resonator / MZI from its S-matrix.");
    // ── 素子応答 (optics/PhotonicCircuit) ──
    I18n::reg("sch_pc_section", "素子応答 (S 行列)", "Element response (S-matrix)");
    I18n::reg("sch_pc_hint",
              "素子レベルの S パラメータ (FDTD / FDE / RCWA で求める) を"
              "接続して回路応答にする層です。ここでは代表的な 2 素子を"
              "解析形の S 行列で計算します。導波路パラメータ (neff / ng / 損失) は"
              "モードソルバの結果を入れてください。",
              "This is the circuit level: element S-parameters (from FDTD / FDE "
              "/ RCWA) combined into a circuit response. Two canonical elements "
              "are computed here from their closed-form S-matrices. Feed the "
              "waveguide parameters (neff / ng / loss) from the mode solver.");
    I18n::reg("sch_pc_device", "素子", "Element");
    I18n::reg("sch_pc_ring_ap", "リング共振器 (全域通過)",
              "Ring resonator (all-pass)");
    I18n::reg("sch_pc_ring_ad", "リング共振器 (アド・ドロップ)",
              "Ring resonator (add-drop)");
    I18n::reg("sch_pc_mzi", "マッハツェンダ干渉計", "Mach-Zehnder interferometer");
    I18n::reg("sch_pc_neff", "実効屈折率 neff", "Effective index neff");
    I18n::reg("sch_pc_ng", "群屈折率 ng", "Group index ng");
    I18n::reg("sch_pc_loss", "伝搬損失 [dB/cm]", "Propagation loss [dB/cm]");
    I18n::reg("sch_pc_radius", "リング半径 [μm]", "Ring radius [um]");
    I18n::reg("sch_pc_k1", "結合率 κ1", "Coupling kappa1");
    I18n::reg("sch_pc_k2", "結合率 κ2 (ドロップ)", "Coupling kappa2 (drop)");
    I18n::reg("sch_pc_dl", "アーム長差 ΔL [μm]", "Arm length difference dL [um]");
    I18n::reg("sch_pc_shift", "位相シフト [rad]", "Phase shift [rad]");
    I18n::reg("sch_pc_range", "波長範囲 [nm]", "Wavelength range [nm]");
    I18n::reg("sch_pc_points", "点数", "Points");
    I18n::reg("sch_pc_run", "▶ 波長応答を計算", "\u25b6 Compute the response");
    I18n::reg("sch_pc_x", "波長 [nm]", "Wavelength [nm]");
    I18n::reg("sch_pc_y", "透過率 [dB]", "Transmission [dB]");
    I18n::reg("sch_pc_res",
              "FSR %1 nm (解析値 λ²/(ng·L) = %2 nm)、FWHM %3 nm、Q = %4、"
              "フィネス %5、消光比 %6 dB、共振 %7 nm",
              "FSR %1 nm (analytic lambda^2/(ng L) = %2 nm), FWHM %3 nm, "
              "Q = %4, finesse %5, extinction %6 dB, resonance %7 nm");
    I18n::reg("sch_pc_partial",
              "消光比 %1 dB。指標を出せませんでした: %2",
              "Extinction %1 dB. Metrics unavailable: %2");
    I18n::reg("sch_pc_note",
              "▸ 素子は解析形の S 行列です。実素子の κ・neff は FDTD / FDE で"
              "求めた値を入れてください。熱光学シフトは「ノイズ・温度効果」の"
              "温度から自動で効きます。ネットリストは経路まで解きますが、"
              "経路上の素子を直列に掛け合わせる回路レベル解析は未対応で、"
              "上の応答は選択した 1 素子ぶんです。",
              "\u25b8 The elements use closed-form S-matrices. Take the real "
              "kappa / neff from an FDTD / FDE run. The thermo-optic shift is "
              "applied automatically from the temperature under \"Noise and "
              "temperature\". The netlist is resolved into a path, but "
              "cascading the elements along it is not supported yet — the "
              "response above is for the single selected element.");
    I18n::reg("sch_to_applied",
              "熱光学シフト適用: %1 ℃ → 共振 %2 nm ずれ (dn/dT = 1.86e-4 /K, Si)",
              "Thermo-optic shift applied: %1 degC -> resonance moved %2 nm "
              "(dn/dT = 1.86e-4 /K, Si)");
    I18n::reg("sch_net_empty",
              "有効な接続がありません — 表に行を足すと経路を解きます",
              "No enabled connections — add rows and the path is resolved");
    I18n::reg("sch_net_nosrc",
              "入力を持たない素子がありません (全体が閉路になっています) — "
              "始点が決まらないので経路を解けません",
              "No node without an input (the netlist is a closed loop), so "
              "there is no start point to trace from");
    I18n::reg("sch_net_path_ok", "経路: %1", "Path: %1");
    I18n::reg("sch_net_path_cut", "経路: %1 — ここで打ち切り (%2)",
              "Path: %1 — stopped here (%2)");
    I18n::reg("sch_mode",       "シミュレーションモード", "Simulation mode");
    I18n::reg("sch_mode_freq",  "周波数領域",             "Frequency domain");
    I18n::reg("sch_mode_time",  "時間領域",               "Time domain");
    I18n::reg("sch_td_x", "時間 [ps]", "Time [ps]");
    I18n::reg("sch_td_y", "|h(t)| (複素包絡線)", "|h(t)| (complex envelope)");
    I18n::reg("sch_td_res",
              "主到達 %1 ps / タップ間隔 %2 ps (解析値 ng·L/c = %3 ps) / "
              "1 周あたりの振幅比 %4。見えている時間長は %5 ps です。"
              "搬送波 (193 THz) は標本化できないので、λ0 まわりの帯域だけを"
              "見た複素包絡線を描いています (搬送波の位相は含みません)。",
              "Main arrival %1 ps / tap spacing %2 ps (analytic ng*L/c = %3 ps) "
              "/ amplitude ratio per round trip %4. The window spans %5 ps. "
              "The carrier (193 THz) cannot be sampled, so this is the complex "
              "envelope over a band around lambda0 (no carrier phase).");
    I18n::reg("sch_td_tail",
              " ⚠ 窓の後ろ 1/4 に電力の %1 % が残っています — 応答が窓より"
              "長いので、その分は先頭へ巡回して混ざります。",
              " [!] %1 % of the power sits in the last quarter of the window -- "
              "the response outlasts the window, so that part wraps around.");
    I18n::reg("sch_td_nolen",
              "時間領域はリングの周長 / MZI のアーム長差から時間軸を決めます。"
              "長さが 0 なので描けません。",
              "The time axis comes from the ring circumference or the MZI arm "
              "length difference. It is zero, so nothing can be drawn.");
    I18n::reg("sch_td_fail", "時間領域の計算に失敗しました",
              "The time-domain calculation failed");
    I18n::reg("sch_mode_mixed", "混合 (光電子共存)",
              "Mixed (co-simulated opto-electronic)");

    I18n::reg("sch_lib_section", "要素ライブラリ / Element library", "Element library");
    I18n::reg("sch_lib_hint",
              "各要素は S パラメータ / コンパクトモデル "
              "(回路図キャンバス・ドラッグ配置は未実装)。",
              "Each element is an S-parameter / compact model "
              "(schematic canvas and drag placement are not implemented).");
    I18n::reg("sch_el_wg_s",   "Si リブ 450×220nm", "Si rib 450×220nm");
    I18n::reg("sch_el_eom_s",  "GHz級",             "GHz class");

    I18n::reg("sch_net_section", "ネットリスト / Connections", "Connections (netlist)");
    I18n::reg("sch_col_from",    "From",       "From");
    I18n::reg("sch_col_to",      "To",         "To");
    I18n::reg("sch_col_wl",      "波長依存",   "Wavelength dep.");
    I18n::reg("sch_net_hint",
              "接続は編集でき、プロジェクト (.ofdx) に保存されます。"
              "チェックを外した行は無効な接続として保存されます。"
              "回路図キャンバスからの自動生成と回路シミュレーションは未実装のため、"
              "この表は接続の記録であり計算には渡されません。",
              "The connections are editable and saved with the project (.ofdx); "
              "unchecked rows are stored as disabled. Generating the list from a "
              "schematic canvas and the circuit simulation itself are not "
              "implemented, so this table records connectivity only and is not "
              "passed to any computation.");
    I18n::reg("sch_net_add",   "＋ 接続を追加", "+ Add connection");
    I18n::reg("sch_net_del",   "− 選択行を削除", "− Delete selected row");

    I18n::reg("sch_noise_section", "ノイズ・温度効果", "Noise & temperature effects");
    I18n::reg("sch_shot",    "ショットノイズ",       "Shot noise");
    I18n::reg("sch_thermal", "熱雑音",               "Thermal noise");
    I18n::reg("sch_rin",     "RIN (相対強度雑音)",   "RIN (relative intensity noise)");
    I18n::reg("sch_phase",   "位相雑音",             "Phase noise");
    I18n::reg("sch_temp",    "温度",                 "Temperature");
    // 熱光学シフトは以前から runCircuitSim() が実際に適用していた。
    // ラベルの「(未実装)」は逆向きの誤記だったので外す (棚卸しの
    // 「動くものを未実装と書けば利用者はその機能に到達できない」)。
    I18n::reg("sch_to_shift", "熱光学シフトを自動適用",
              "Apply the thermo-optic shift automatically");
    I18n::reg("sch_uw_sim",
              "シミュレーションモードのうち「混合 (光電子共存)」"
              "(電気側の回路モデルと光電変換の連成が要ります)",
              "the “mixed” simulation mode (it needs an electrical circuit "
              "model coupled through the opto-electronic conversion)");
    I18n::reg("sch_uw_sim_ok",
              "「周波数領域」— 下の「素子応答」がこのモードの計算です。"
              "「時間領域」は同じ素子の H(λ) から複素包絡線のインパルス応答"
              "を出します",
              "“frequency domain” (the element response below) and "
              "“time domain”, which turns the same element's H(lambda) into "
              "a complex-envelope impulse response");
    I18n::reg("sch_uw_thermo",
              "位相雑音 (干渉計の遅延とレーザ線幅から強度雑音へ換算する"
              "モデルが要ります)",
              "phase noise (converting it to intensity noise needs the "
              "interferometer delay and the laser linewidth)");
    I18n::reg("sch_uw_thermo_ok",
              "温度と熱光学シフト (素子応答の共振波長に効きます) と、"
              "ショット / 熱 / RIN の雑音項 (下の雑音収支になります)",
              "the temperature and thermo-optic shift (they move the resonance "
              "in the element response) and the shot / thermal / RIN terms "
              "(they make up the noise budget below)");

    // 受光器の雑音収支
    I18n::reg("sch_rx_sec", "受光器の雑音収支", "Receiver noise budget");
    I18n::reg("sch_rx_power", "受光パワー [mW]", "Received power [mW]");
    I18n::reg("sch_rx_resp", "受光感度 [A/W]", "Responsivity [A/W]");
    I18n::reg("sch_rx_load", "負荷抵抗 [Ω]", "Load resistance [ohm]");
    I18n::reg("sch_rx_bw", "帯域 [GHz]", "Bandwidth [GHz]");
    I18n::reg("sch_rx_rin", "RIN [dB/Hz]", "RIN [dB/Hz]");
    I18n::reg("sch_rx_fmt",
              "光電流 %1 mA / 雑音電流 %2 nA(rms) / SNR %3 dB / NEP %4 pW/√Hz\n"
              "内訳: ショット %5 / 熱 %6 / RIN %7 (電力比)",
              "Photocurrent %1 mA / noise current %2 nA(rms) / SNR %3 dB / "
              "NEP %4 pW/rtHz\nBreakdown: shot %5 / thermal %6 / RIN %7 "
              "(power ratio)");
    I18n::reg("sch_rx_nosnr",
              "雑音項がすべて外れているので SNR は出せません "
              "(ショット / 熱 / RIN のどれかを選んでください)",
              "Every noise term is switched off, so there is no SNR to report "
              "(select shot, thermal or RIN)");
    I18n::reg("sch_rx_bad",
              "受光器の設定が不正です (感度・負荷抵抗・帯域は正の値、"
              "温度は絶対零度より上)",
              "The receiver settings are not valid (responsivity, load and "
              "bandwidth must be positive and the temperature above absolute "
              "zero)");
    I18n::reg("sch_rx_note",
              "▸ ショット 2qIB・熱 4kTB/R_L・RIN rin(RP)²B の定義式そのもの。"
              "温度は上の欄 (熱雑音は絶対温度に比例します)。アバランシェ増倍と"
              "増幅器雑音は含みません。",
              "▸ Shot 2qIB, thermal 4kTB/R_L and RIN rin(RP)^2B, straight from "
              "the definitions. The temperature comes from the field above "
              "(thermal noise scales with the absolute temperature). Avalanche "
              "gain and amplifier noise are not included.");
    return true;
}();

// muted text-sm 相当
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("color:#7A7A7A; font-size:11px;");
    return l;
}

// mock の要素ライブラリ (12 種) をそのまま転記。sub が翻訳キーのものは keyed=true。
struct ElemDef {
    const char *ic;
    const char *name;
    const char *sub;      // keyed のときは I18n キー
    bool        keyed;
};
const ElemDef kElements[12] = {
    { "⫴",  "Waveguide",          "sch_el_wg_s",     true  },
    { "⌑",  "Ring resonator",     "R=5μm, Q=15k",    false },
    { "≡",  "DBR Mirror",         "25 periods",      false },
    { "▷◁", "MZI",                "ΔL=50μm",         false },
    { "⊞",  "MMI 1×2",            "50:50 splitter",  false },
    { "⊟",  "MMI 2×2",            "3dB coupler",     false },
    { "◈",  "Grating coupler",    "-2.8dB @1550",    false },
    { "⊰",  "Edge coupler",       "SSC, -1.5dB",     false },
    { "⌬",  "Phase shifter (TO)", "Vπ=3.5V",         false },
    { "⚡", "PD detector",        "R=0.9 A/W",       false },
    { "☼",  "Laser source",       "DFB 1550nm",      false },
    { "◐",  "Modulator (EOM)",    "sch_el_eom_s",    true  },
};

// 熱光学の定数 (Si @ 1550 nm、室温)。ModeSolverTab / MultiphysicsTab と同値。
const double kToDnDt_Si   = 1.86e-4;   // dn/dT [1/K]
const double kToRefTemp_C = 25.0;      // 基準温度

} // namespace

SchematicTab::SchematicTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // ── フォトニック回路シミュレーション ───────────────────────────────────
    auto *sSim = new SectionBox(I18n::tr("sch_sim_section"), body);
    sSim->vbox()->addWidget(mutedLabel(I18n::tr("sch_sim_hint"), sSim));
    m_mode = new QComboBox(sSim);
    m_mode->addItem(I18n::tr("sch_mode_freq"));
    m_mode->addItem(I18n::tr("sch_mode_time"));
    m_mode->addItem(I18n::tr("sch_mode_mixed"));
    m_mode->setCurrentIndex(0);              // mock: value="freq"
    // 時間領域は素子応答 H(λ) の複素包絡線インパルス応答として実装した
    // (optics/CircuitImpulse)。**混合 (光電子共存) だけは実体が無い**ので
    // 選べるように見せない (絶対規則 5)。
    if (auto *item = qobject_cast<QStandardItemModel *>(m_mode->model())
                         ->item(2))
        item->setEnabled(false);
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { runCircuitSim(); });
    sSim->form()->addRow(I18n::tr("sch_mode"), m_mode);
    sSim->form()->addRow(tabhelp::unwiredNote(sSim, I18n::tr("sch_uw_sim"),
                                              I18n::tr("sch_uw_sim_ok")));
    v->addWidget(sSim);

    // ── 要素ライブラリ / Element library (3列グリッドのカード) ─────────────
    auto *sLib = new SectionBox(I18n::tr("sch_lib_section"), body);
    sLib->vbox()->addWidget(mutedLabel(I18n::tr("sch_lib_hint"), sLib));
    auto *grid = new QGridLayout();
    grid->setSpacing(6);
    for (int i = 0; i < 12; ++i) {
        const ElemDef &e = kElements[i];
        auto *card = new QFrame(sLib);
        card->setObjectName("elemCard");
        // キャンバスが無くドラッグできないため、掴めるカーソルにしない
        // (mock は cursor:"grab" だが未実装機能を動作済みに見せない — 絶対規則 5)
        card->setCursor(Qt::ArrowCursor);
        card->setStyleSheet(
            "#elemCard { border:1px solid palette(mid); border-radius:3px;"
            " background:palette(alternate-base); }");
        auto *cv = new QVBoxLayout(card);
        cv->setContentsMargins(7, 5, 7, 5);
        cv->setSpacing(1);

        auto *head = new QHBoxLayout();
        head->setSpacing(4);
        auto *ic = new QLabel(QString::fromUtf8(e.ic), card);
        ic->setStyleSheet("font-size:11px; font-weight:600; color:#0078D4;");
        head->addWidget(ic);
        auto *nameL = new QLabel(QString::fromUtf8(e.name), card);
        nameL->setStyleSheet("font-size:11px; font-weight:600;");
        head->addWidget(nameL);
        head->addStretch(1);
        cv->addLayout(head);

        auto *subL = new QLabel(e.keyed ? I18n::tr(e.sub)
                                        : QString::fromUtf8(e.sub), card);
        subL->setStyleSheet("font-size:10px; color:#7A7A7A;");
        subL->setWordWrap(true);
        cv->addWidget(subL);

        grid->addWidget(card, i / 3, i % 3);
    }
    for (int c = 0; c < 3; ++c) grid->setColumnStretch(c, 1);
    sLib->vbox()->addLayout(grid);
    v->addWidget(sLib);

    // ── ネットリスト / Connections ─────────────────────────────────────────
    // 表は Project::photonicNetlist() のビュー。編集は即座にモデルへ書き戻し、
    // .ofdx ("schematic.netlist") へ保存される (固定サンプルではない)。
    auto *sNet = new SectionBox(I18n::tr("sch_net_section"), body);
    m_netFont = mono;
    m_net = new QTableWidget(0, 4, sNet);
    m_net->setHorizontalHeaderLabels({ "#", I18n::tr("sch_col_from"),
                                       I18n::tr("sch_col_to"),
                                       I18n::tr("sch_col_wl") });
    m_net->verticalHeader()->setVisible(false);
    m_net->setColumnWidth(0, 40);
    m_net->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_net->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_net->setMinimumHeight(170);
    sNet->vbox()->addWidget(m_net);
    sNet->vbox()->addWidget(mutedLabel(I18n::tr("sch_net_hint"), sNet));

    auto *netBtns = new QHBoxLayout();
    auto *addBtn = new QPushButton(I18n::tr("sch_net_add"), sNet);
    auto *delBtn = new QPushButton(I18n::tr("sch_net_del"), sNet);
    netBtns->addWidget(addBtn);
    netBtns->addWidget(delBtn);
    netBtns->addStretch(1);
    sNet->vbox()->addLayout(netBtns);
    // 表から素子のつながりを辿った結果 (optics/PhotonicCircuit::tracePath)。
    // 分岐や閉路は 1 本の経路にできないので、その理由をそのまま出す。
    m_netPath = new QLabel(sNet);
    m_netPath->setWordWrap(true);
    m_netPath->setStyleSheet("font-size:11px;");
    sNet->vbox()->addWidget(m_netPath);
    v->addWidget(sNet);

    connect(addBtn, &QPushButton::clicked, this, [this] {
        PhotonicNetRow r;
        r.wavelength = QString::fromUtf8("—");
        m_p->photonicNetlist().push_back(r);
        m_p->touch();
        refreshNetlist();
        m_net->setCurrentCell(m_net->rowCount() - 1, 1);
    });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        const int row = m_net->currentRow();
        QVector<PhotonicNetRow> &net = m_p->photonicNetlist();
        if (row < 0 || row >= net.size()) return;
        net.remove(row);
        m_p->touch();
        refreshNetlist();
    });
    connect(m_net, &QTableWidget::itemChanged, this, &SchematicTab::onNetItemChanged);

    // ── 素子応答 (S 行列) ──────────────────────────────────────────────────
    {
        auto *sec = new SectionBox(I18n::tr("sch_pc_section"), body);
        auto *hint = new QLabel(I18n::tr("sch_pc_hint"), sec);
        hint->setWordWrap(true);
        hint->setStyleSheet("color:palette(mid);");
        sec->vbox()->addWidget(hint);
        auto *form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(8);
        form->setVerticalSpacing(4);
        auto spin = [&](double lo, double hi, double v, int dec, double step) {
            auto *w = new QDoubleSpinBox(sec);
            w->setRange(lo, hi);
            w->setDecimals(dec);
            w->setSingleStep(step);
            w->setValue(v);
            w->setMaximumWidth(130);
            return w;
        };
        m_device = new QComboBox(sec);
        m_device->addItem(I18n::tr("sch_pc_ring_ap"));
        m_device->addItem(I18n::tr("sch_pc_ring_ad"));
        m_device->addItem(I18n::tr("sch_pc_mzi"));
        form->addRow(I18n::tr("sch_pc_device"), m_device);
        m_neff = spin(1.0, 5.0, 2.44, 4, 0.01);
        m_ng   = spin(1.0, 8.0, 4.2, 4, 0.01);
        m_loss = spin(0.0, 100.0, 2.0, 3, 0.1);
        form->addRow(I18n::tr("sch_pc_neff"), m_neff);
        form->addRow(I18n::tr("sch_pc_ng"), m_ng);
        form->addRow(I18n::tr("sch_pc_loss"), m_loss);
        m_radius = spin(0.5, 1000.0, 10.0, 3, 0.5);
        m_k1 = spin(0.0, 1.0, 0.25, 4, 0.01);
        m_k2 = spin(0.0, 1.0, 0.25, 4, 0.01);
        form->addRow(I18n::tr("sch_pc_radius"), m_radius);
        form->addRow(I18n::tr("sch_pc_k1"), m_k1);
        form->addRow(I18n::tr("sch_pc_k2"), m_k2);
        m_dL = spin(0.1, 100000.0, 100.0, 3, 1.0);
        m_shift = spin(-100.0, 100.0, 0.0, 4, 0.1);
        form->addRow(I18n::tr("sch_pc_dl"), m_dL);
        form->addRow(I18n::tr("sch_pc_shift"), m_shift);
        auto *range = new QHBoxLayout();
        m_lam1 = spin(200.0, 20000.0, 1540.0, 3, 1.0);
        m_lam2 = spin(200.0, 20000.0, 1560.0, 3, 1.0);
        m_points = new QSpinBox(sec);
        m_points->setRange(11, 200001);
        m_points->setValue(4001);
        m_points->setMaximumWidth(110);
        range->addWidget(m_lam1);
        range->addWidget(new QLabel(QStringLiteral("〜"), sec));
        range->addWidget(m_lam2);
        range->addSpacing(8);
        range->addWidget(new QLabel(I18n::tr("sch_pc_points"), sec));
        range->addWidget(m_points);
        range->addStretch(1);
        form->addRow(I18n::tr("sch_pc_range"), range);
        sec->vbox()->addLayout(form);
        auto *run = new QPushButton(I18n::tr("sch_pc_run"), sec);
        connect(run, &QPushButton::clicked, this, &SchematicTab::runCircuitSim);
        sec->vbox()->addWidget(run);
        m_spectrum = new MiniPlot(sec);
        m_spectrum->setLabels(I18n::tr("sch_pc_x"), I18n::tr("sch_pc_y"));
        m_spectrum->setMinimumHeight(180);
        sec->vbox()->addWidget(m_spectrum);
        m_simResult = new QLabel(sec);
        m_simResult->setWordWrap(true);
        sec->vbox()->addWidget(m_simResult);
        auto *note = new QLabel(I18n::tr("sch_pc_note"), sec);
        note->setWordWrap(true);
        note->setStyleSheet("font-size:11px; color:palette(mid);");
        sec->vbox()->addWidget(note);
        v->addWidget(sec);
        // runCircuitSim() はここでは呼ばない。熱光学シフトが「ノイズ・温度
        // 効果」セクション (この下で組み立てる) の温度を読むため、ここで
        // 呼ぶとまだ存在しないウィジェットを触ることになる。
        // コンストラクタの最後にまとめて呼ぶ。
    }

    // ── ノイズ・温度効果 ───────────────────────────────────────────────────
    auto *sNo = new SectionBox(I18n::tr("sch_noise_section"), body);
    m_shot = new QCheckBox(I18n::tr("sch_shot"), sNo);
    m_shot->setChecked(true);
    m_thermal = new QCheckBox(I18n::tr("sch_thermal"), sNo);
    m_thermal->setChecked(true);
    auto *n1 = new QHBoxLayout();
    n1->addWidget(m_shot);
    n1->addWidget(m_thermal);
    n1->addStretch(1);
    sNo->vbox()->addLayout(n1);

    m_rin   = new QCheckBox(I18n::tr("sch_rin"), sNo);
    m_phase = new QCheckBox(I18n::tr("sch_phase"), sNo);
    auto *n2 = new QHBoxLayout();
    n2->addWidget(m_rin);
    n2->addWidget(m_phase);
    n2->addStretch(1);
    sNo->vbox()->addLayout(n2);

    m_temp = new QLineEdit("25", sNo);
    m_temp->setMaximumWidth(60);
    auto *tRow = new QHBoxLayout();
    tRow->addWidget(m_temp);
    tRow->addWidget(new QLabel(QString::fromUtf8("°C"), sNo));
    tRow->addStretch(1);
    sNo->form()->addRow(I18n::tr("sch_temp"), tRow);

    m_toShift = new QCheckBox(I18n::tr("sch_to_shift"), sNo);
    m_toShift->setChecked(true);
    sNo->form()->addRow(m_toShift);
    // 位相雑音だけは強度雑音への換算モデルが無い (絶対規則 5)
    m_phase->setEnabled(false);
    sNo->form()->addRow(tabhelp::unwiredNote(sNo, I18n::tr("sch_uw_thermo"),
                                             I18n::tr("sch_uw_thermo_ok")));
    v->addWidget(sNo);

    // ── 受光器の雑音収支 (core/ReceiverNoise) ──────────────────────────────
    auto *sRx = new SectionBox(I18n::tr("sch_rx_sec"), body);
    auto mkEdit = [sRx](const char *initial) {
        auto *e = new QLineEdit(QString::fromLatin1(initial), sRx);
        e->setMaximumWidth(90);
        return e;
    };
    m_rxPower = mkEdit("1.0");
    m_rxResp  = mkEdit("0.9");
    m_rxLoad  = mkEdit("50");
    m_rxBw    = mkEdit("1.0");
    m_rxRin   = mkEdit("-155");
    sRx->form()->addRow(I18n::tr("sch_rx_power"), m_rxPower);
    sRx->form()->addRow(I18n::tr("sch_rx_resp"), m_rxResp);
    sRx->form()->addRow(I18n::tr("sch_rx_load"), m_rxLoad);
    sRx->form()->addRow(I18n::tr("sch_rx_bw"), m_rxBw);
    sRx->form()->addRow(I18n::tr("sch_rx_rin"), m_rxRin);
    m_rxResult = new QLabel(sRx);
    m_rxResult->setWordWrap(true);
    sRx->vbox()->addWidget(m_rxResult);
    sRx->vbox()->addWidget(mutedLabel(I18n::tr("sch_rx_note"), sRx));
    v->addWidget(sRx);

    // 雑音項のチェック・温度・受光器の設定はすべて雑音収支へ入る。
    // 温度は熱光学シフト (素子応答) にも効くので両方を更新する。
    for (QCheckBox *c : { m_shot, m_thermal, m_rin })
        connect(c, &QCheckBox::toggled, this,
                [this](bool) { updateNoiseBudget(); });
    for (QLineEdit *e : { m_rxPower, m_rxResp, m_rxLoad, m_rxBw, m_rxRin })
        connect(e, &QLineEdit::editingFinished, this,
                [this] { updateNoiseBudget(); });
    connect(m_temp, &QLineEdit::editingFinished, this, [this] {
        runCircuitSim();        // 熱光学シフト
        updateNoiseBudget();    // 熱雑音
    });
    connect(m_toShift, &QCheckBox::toggled, this,
            [this](bool) { runCircuitSim(); });

    v->addStretch(1);
    // 全セクションを組み立ててから初回の応答を出す (熱光学シフト込み)
    runCircuitSim();
    updateNoiseBudget();

    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::loaded, this, &SchematicTab::refreshNetlist);
    refreshNetlist();
}

// モデル → 表 (m_updating ガード付き。行数が変わるので毎回作り直す)
void SchematicTab::refreshNetlist()
{
    // 表を作り直したら経路表示も更新する (呼び忘れを作らない)
    struct PathSync { SchematicTab *t; ~PathSync() { t->refreshNetPath(); } } sync{ this };
    m_updating = true;
    const QVector<PhotonicNetRow> &net = m_p->photonicNetlist();
    m_net->setRowCount(net.size());
    for (int i = 0; i < net.size(); ++i) {
        const PhotonicNetRow &r = net[i];
        auto *no = new QTableWidgetItem(QString::number(i + 1));
        no->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        no->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                     Qt::ItemIsUserCheckable);
        no->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        m_net->setItem(i, 0, no);
        m_net->setItem(i, 1, new QTableWidgetItem(r.from));
        m_net->setItem(i, 2, new QTableWidgetItem(r.to));
        auto *wl = new QTableWidgetItem(r.wavelength);
        wl->setFont(m_netFont);
        m_net->setItem(i, 3, wl);
    }
    m_updating = false;
}

// 表 → モデル (1 セル分)
void SchematicTab::onNetItemChanged(QTableWidgetItem *item)
{
    if (m_updating || !item) return;
    const int row = item->row();
    QVector<PhotonicNetRow> &net = m_p->photonicNetlist();
    if (row < 0 || row >= net.size()) return;
    switch (item->column()) {
    case 0: net[row].enabled = (item->checkState() == Qt::Checked); break;
    case 1: net[row].from = item->text(); break;
    case 2: net[row].to = item->text(); break;
    case 3: net[row].wavelength = item->text(); break;
    default: return;
    }
    m_p->touch();
}

// 素子の S 行列 → 波長掃引 → 指標。数字が読めないときは理由を出す。
void SchematicTab::runCircuitSim()
{
    using namespace ofd::optics;
    if (!m_spectrum || !m_simResult) return;
    const double l1 = m_lam1->value(), l2 = m_lam2->value();
    const int n = m_points->value();
    if (!(l2 > l1)) { m_simResult->setText(QStringLiteral("λ1 < λ2")); return; }

    Waveguide wg;
    wg.neff = m_neff->value();
    wg.ng = m_ng->value();
    wg.lambda0_nm = 0.5 * (l1 + l2);
    wg.loss_dBcm = m_loss->value();

    // 熱光学シフト — 「ノイズ・温度効果」の温度と有効化チェックを実際に読む。
    // neff(T) = neff + (dn/dT)(T − 25 ℃)。Si の dn/dT = 1.86e-4 /K。
    // 共振は Δλ = λ·Δn/ng だけ長波長側へ動く (掃引結果に現れる)。
    double shift_nm = 0.0;
    if (m_toShift && m_toShift->isChecked() && m_temp) {
        bool tok = false;
        const double tC = m_temp->text().trimmed().toDouble(&tok);
        if (tok) {
            const double dT = tC - kToRefTemp_C;
            wg.neff = thermoOpticNeff(wg.neff, kToDnDt_Si, tC, kToRefTemp_C);
            shift_nm = thermoOpticShift_nm(wg.lambda0_nm, kToDnDt_Si, dT,
                                           wg.ng);
        }
    }

    std::vector<SweepPoint> sweep;
    double length_um = 0.0;
    const int dev = m_device->currentIndex();
    if (dev <= 1) {
        RingResonator ring;
        ring.wg = wg;
        ring.radius_um = m_radius->value();
        ring.kappa1 = m_k1->value();
        ring.kappa2 = (dev == 1) ? m_k2->value() : 0.0;
        length_um = ring.circumference_um();
        sweep = sweepRing(ring, l1, l2, n);
    } else {
        MachZehnder mzi;
        mzi.wg = wg;
        mzi.length1_um = 100.0;
        mzi.length2_um = 100.0 + m_dL->value();
        mzi.phaseShift_rad = m_shift->value();
        length_um = m_dL->value();
        sweep = sweepMzi(mzi, l1, l2, n);
    }

    // ── 時間領域モード — 同じ素子の H(λ) からインパルス応答を作る ────────
    // 周波数掃引と**同じ素子・同じ設定**から作るので、2 つのモードが
    // 食い違うことはない。
    if (m_mode && m_mode->currentIndex() == 1) {
        showTimeDomain(wg, length_um, dev);
        return;
    }

    QVector<QPointF> thr, drp;
    thr.reserve(int(sweep.size()));
    for (const SweepPoint &p : sweep) {
        thr.push_back(QPointF(p.lambda_nm, p.through_dB));
        if (p.drop_dB > -299.0) drp.push_back(QPointF(p.lambda_nm, p.drop_dB));
    }
    QVector<MiniSeries> series;
    MiniSeries a; a.pts = thr; a.color = QColor("#0078D4"); a.label = "through";
    series.push_back(a);
    if (!drp.isEmpty()) {
        MiniSeries b; b.pts = drp; b.color = QColor("#E8A33D"); b.label = "drop";
        series.push_back(b);
    }
    m_spectrum->setSeries(series);

    const ResonatorMetrics m = analyseSweep(sweep);
    const double fsrTheory = analyticFsr_nm(0.5 * (l1 + l2), wg.ng, length_um);
    if (m.valid && m.fsr_nm > 0.0) {
        m_simResult->setText(I18n::tr("sch_pc_res")
            .arg(m.fsr_nm, 0, 'g', 5).arg(fsrTheory, 0, 'g', 5)
            .arg(m.fwhm_nm, 0, 'g', 4)
            .arg(m.qFactor, 0, 'f', 0)
            .arg(m.finesse, 0, 'f', 1)
            .arg(m.extinction_dB, 0, 'f', 2)
            .arg(m.resonance_nm, 0, 'g', 7));
    } else {
        m_simResult->setText(I18n::tr("sch_pc_partial")
            .arg(m.extinction_dB, 0, 'f', 2)
            .arg(QString::fromStdString(m.note)));
    }
    // 熱光学シフトを効かせたときは、その量を結果へ添える (効いたことが
    // 数字で分かるように — 内部で静かに変えて終わりにしない)
    if (shift_nm != 0.0)
        m_simResult->setText(m_simResult->text() + QLatin1Char('\n')
                             + I18n::tr("sch_to_applied")
                                   .arg(m_temp->text().trimmed())
                                   .arg(shift_nm, 0, 'f', 3));
}

// ── 時間領域 (複素包絡線のインパルス応答) ─────────────────────────────────
// リングなら 1 周ごとの遅延パルス列、MZI なら 2 本のアーム。搬送波
// (193 THz) は標本化できないので、**λ0 まわりの帯域だけを見た包絡線**を
// 出す — その約束と、窓から溢れた分 (尾) を必ず画面に書く。
void SchematicTab::showTimeDomain(const ofd::optics::Waveguide &wg,
                                  double length_um, int dev)
{
    using namespace ofd::optics;
    if (!m_spectrum || !m_simResult) return;

    const double lam0 = 0.5 * (m_lam1->value() + m_lam2->value());
    const double C0 = 299792458.0;
    // 素子の周期 (リングは 1 周、MZI はアーム長差) から帯域を決める
    const double tau = wg.ng * (length_um * 1e-6) / C0;
    if (!(tau > 0.0)) {
        m_simResult->setText(I18n::tr("sch_td_nolen"));
        return;
    }

    pic::SpectrumFn H;
    RingResonator ring;
    MachZehnder mzi;
    if (dev <= 1) {
        ring.wg = wg;
        ring.radius_um = m_radius->value();
        ring.kappa1 = m_k1->value();
        ring.kappa2 = (dev == 1) ? m_k2->value() : 0.0;
        H = [ring](double lam) { return ring.through(lam); };
    } else {
        mzi.wg = wg;
        mzi.length1_um = 100.0;
        mzi.length2_um = 100.0 + m_dL->value();
        mzi.phaseShift_rad = m_shift->value();
        H = [mzi](double lam) { return mzi.bar(lam); };
    }

    pic::ImpulseConfig cfg;
    cfg.lambda0_nm = lam0;
    cfg.fsrHint_Hz = 1.0 / tau;      // 素子の周期
    cfg.fsrMultiple = 32;            // 1 周を 32 点で刻む
    cfg.points = 1024;               // 32 周ぶん見える
    const pic::ImpulseResult r = pic::impulse(H, cfg);
    if (!r.ok()) { m_simResult->setText(I18n::tr("sch_td_fail")); return; }

    MiniSeries a;
    a.color = QColor("#0078D4");
    a.label = "|h(t)|";
    for (std::size_t i = 0; i < r.h.size(); ++i)
        a.pts.push_back(QPointF(i * r.dt_s * 1e12, std::abs(r.h[i])));
    m_spectrum->setLabels(I18n::tr("sch_td_x"), I18n::tr("sch_td_y"));
    m_spectrum->setSeries({ a });

    QString note = I18n::tr("sch_td_res")
                       .arg(r.mainDelay_s * 1e12, 0, 'f', 3)
                       .arg(r.tapSpacing_s * 1e12, 0, 'f', 3)
                       .arg(tau * 1e12, 0, 'f', 3)
                       .arg(r.decayRatio, 0, 'f', 4)
                       .arg(r.span_s * 1e12, 0, 'f', 1);
    // 窓から溢れた分は必ず書く (リングの応答は無限に続くので必ず溢れる)
    if (r.tailFraction > 0.01)
        note += I18n::tr("sch_td_tail").arg(r.tailFraction * 100.0, 0, 'f', 1);
    m_simResult->setText(note);
}

// ── ネットリストから素子のつながりを辿る ───────────────────────────────────
// 表は「接続の記録」だったが、辿った結果を出すようにする。回路レベルの
// 掛け算はまだ 1 素子ぶんなので、経路が出せることと、その経路がどの素子を
// 通るかまでを示す (掛け算の自動化は未対応であることも併せて出す)。
void SchematicTab::refreshNetPath()
{
    using namespace ofd::optics;
    if (!m_netPath) return;
    std::vector<NetLink> links;
    for (const PhotonicNetRow &r : m_p->photonicNetlist()) {
        if (!r.enabled) continue;
        if (r.from.trimmed().isEmpty() || r.to.trimmed().isEmpty()) continue;
        links.push_back(parseLink(r.from.trimmed().toStdString(),
                                  r.to.trimmed().toStdString()));
    }
    if (links.empty()) {
        m_netPath->setText(I18n::tr("sch_net_empty"));
        return;
    }
    const std::vector<std::string> src = sourceNodes(links);
    if (src.empty()) {
        m_netPath->setText(I18n::tr("sch_net_nosrc"));
        return;
    }
    QStringList lines;
    for (const std::string &s0 : src) {
        const NetPath p = tracePath(links, s0);
        QStringList nodes;
        for (const std::string &n : p.nodes)
            nodes << QString::fromStdString(n);
        const QString chain = nodes.join(QStringLiteral(" → "));
        lines << (p.complete
                      ? I18n::tr("sch_net_path_ok").arg(chain)
                      : I18n::tr("sch_net_path_cut")
                            .arg(chain, QString::fromStdString(p.note)));
    }
    m_netPath->setText(lines.join(QLatin1Char('\n')));
}

// ── 受光器の雑音収支 ───────────────────────────────────────────────────────
// 「ノイズ・温度効果」のチェックと温度、下の受光器の設定から
// core/ReceiverNoise で計算する。位相雑音だけはモデルが無いので数えない。
void SchematicTab::updateNoiseBudget()
{
    if (!m_rxResult) return;
    rxnoise::Receiver rx;
    auto num = [](QLineEdit *e, double fallback) {
        bool ok = false;
        const double v = e->text().trimmed().toDouble(&ok);
        return ok ? v : fallback;
    };
    rx.opticalPower_W = num(m_rxPower, 1.0) * 1.0e-3;   // mW → W
    rx.responsivity_A_W = num(m_rxResp, 0.9);
    rx.loadResistance_ohm = num(m_rxLoad, 50.0);
    rx.bandwidth_Hz = num(m_rxBw, 1.0) * 1.0e9;         // GHz → Hz
    rx.rin_dBHz = num(m_rxRin, -155.0);
    rx.temperature_C = num(m_temp, kToRefTemp_C);
    rx.shot = m_shot->isChecked();
    rx.thermal = m_thermal->isChecked();
    rx.rin = m_rin->isChecked();

    const rxnoise::Noise n = rxnoise::analyze(rx);
    if (!n.valid) {
        m_rxResult->setText(I18n::tr("sch_rx_bad"));
        return;
    }
    if (!n.snrValid) {
        m_rxResult->setText(I18n::tr("sch_rx_nosnr"));
        return;
    }
    auto frac = [&n](double part) {
        return QString::number(n.total_A2 > 0.0 ? part / n.total_A2 : 0.0,
                               'f', 3);
    };
    m_rxResult->setText(I18n::tr("sch_rx_fmt")
        .arg(QString::number(n.photocurrent_A * 1.0e3, 'g', 4),
             QString::number(n.rms_A * 1.0e9, 'g', 4),
             QString::number(n.snr_dB, 'f', 1),
             QString::number(n.nep_W_rtHz * 1.0e12, 'g', 3),
             frac(n.shot_A2), frac(n.thermal_A2), frac(n.rin_A2)));
}
