// FamilySolverTab.cpp
#include "FamilySolverTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 fam_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("fam_title", "姉妹ソルバ一覧 (ss023804.stars.ne.jp)",
              "OpenFDTD Family (ss023804.stars.ne.jp)");
    I18n::reg("fam_intro",
              "<b>OpenFDTD家族</b> — 姉妹ソルバの一覧。"
              "ソルバ切替の実行連携は未実装 — 表示のみで、実行されるのは常に FDTD 系 (ofd) です。",
              "<b>OpenFDTD family</b> — overview of the sibling solvers. "
              "Running the selected solver is not implemented — display only; "
              "execution always uses the FDTD kernels (ofd).");
    I18n::reg("fam_selected", "選択", "Selected");
    I18n::reg("fam_detail_title", "選択中: %1 — 詳細設定", "Selected: %1 — Details");
    I18n::reg("fam_strengths", "強み:", "Strengths:");
    I18n::reg("fam_example", "例:", "Example:");

    // FDTD
    I18n::reg("fam_fdtd_hint", "FDTD詳細は「ソルバ領域」「メッシュ詳細」タブで設定",
              "Configure FDTD details in the Solver Region and Mesh tabs");
    I18n::reg("fam_fdtd_compat", "本家OpenFDTD互換", "Upstream OpenFDTD compatible");
    I18n::reg("fam_fdtd_ofd", ".ofd 形式で読み書き", "Read/write .ofd format");
    I18n::reg("fam_fdtd_binary", "バイナリ", "Binary");
    I18n::reg("fam_fdtd_binary_auto", "ofd / ofd_mpi / ofd_cuda / ofd_cuda_mpi 自動選択",
              "auto-select ofd / ofd_mpi / ofd_cuda / ofd_cuda_mpi");
    I18n::reg("fam_fdtd_output", "出力", "Output");

    // OpenRTM
    I18n::reg("fam_rtm_rays", "光線数", "Ray count");
    I18n::reg("fam_rtm_maxrefl", "最大反射回数", "Max reflections");
    I18n::reg("fam_rtm_txpos", "送信アンテナ位置", "Tx antenna position");
    I18n::reg("fam_rtm_rxarea", "受信エリア", "Rx area");
    I18n::reg("fam_rtm_scene", "シーン", "Scene");
    I18n::reg("fam_rtm_scene_indoor", "屋内 (オフィス・住宅)", "Indoor (office / residential)");
    I18n::reg("fam_rtm_scene_urban", "都市部 (ビル街)", "Urban (buildings)");
    I18n::reg("fam_rtm_scene_suburb", "郊外", "Suburban");
    I18n::reg("fam_rtm_scene_custom", "カスタム DXF/STL", "Custom DXF/STL");
    I18n::reg("fam_rtm_wall", "壁透過モデル", "Wall transmission model");
    I18n::reg("fam_rtm_scatter", "散乱モデル (Lambert)", "Scattering model (Lambert)");
    I18n::reg("fam_rtm_doppler", "ドップラー効果", "Doppler effect");
    I18n::reg("fam_rtm_pol", "偏波追跡", "Polarization tracking");

    // OpenTHFD
    I18n::reg("fam_thfd_hint", "周波数領域FDM — 単一周波数で精密。高Q構造に最適。",
              "Frequency-domain FDM — precise at a single frequency. Ideal for high-Q structures.");
    I18n::reg("fam_freq", "周波数", "Frequency");
    I18n::reg("fam_thfd_quasi", "準静電界モード", "Quasi-static mode");
    I18n::reg("fam_thfd_approx", "ωμ→0 近似", "ωμ→0 approximation");
    I18n::reg("fam_thfd_lowfreq", "低周波 <100MHz 高速化", "speeds up low frequency <100MHz");
    I18n::reg("fam_solver", "ソルバ", "Solver");
    I18n::reg("fam_thfd_direct", "直接法 (LU)", "Direct (LU)");
    I18n::reg("fam_converge", "収束判定", "Convergence");

    // OpenMOM
    I18n::reg("fam_mom_hint", "アンテナ専用 — 開放領域を扱うので吸収境界条件不要。",
              "Antenna-specific — handles open regions, no absorbing boundary needed.");
    I18n::reg("fam_mom_conductor", "導体モデル", "Conductor model");
    I18n::reg("fam_mom_wire", "線状 (細線)", "Wire (thin)");
    I18n::reg("fam_mom_surface", "面状 (RWG)", "Surface (RWG)");
    I18n::reg("fam_mom_mixed", "線+面 混合", "Wire + surface mixed");
    I18n::reg("fam_mom_basis", "基底関数", "Basis functions");
    I18n::reg("fam_mom_pulse", "パルス", "Pulse");
    I18n::reg("fam_mom_tri", "三角", "Triangle");
    I18n::reg("fam_mom_sin", "正弦波", "Sinusoidal");
    I18n::reg("fam_mom_seglen", "セグメント長 / λ", "Segment length / λ");
    I18n::reg("fam_mom_input", "入力", "Input");
    I18n::reg("fam_mom_nec", "NECフォーマット (.nec)", "NEC format (.nec)");

    // OpenSTF
    I18n::reg("fam_stf_hint", "DC電界 — Poisson方程式を SOR/CG法で解く。",
              "DC field — solves the Poisson equation with SOR/CG.");
    I18n::reg("fam_stf_mg", "マルチグリッド", "Multigrid");
    I18n::reg("fam_stf_relax", "緩和係数 (SOR)", "Relaxation factor (SOR)");
    I18n::reg("fam_stf_electrodes", "電極", "Electrodes");
    I18n::reg("fam_stf_volt", "電圧 [V]", "Voltage [V]");
    I18n::reg("fam_stf_shape", "形状", "Shape");
    I18n::reg("fam_stf_name", "名前", "Name");
    I18n::reg("fam_stf_elec_note",
              "電極候補はプロジェクトの導体形状 (材質 1 = PEC) から自動抽出して"
              "います。電圧は .ofd / .ofdx に持たない量なので「—」です "
              "(OpenSTF への受け渡しは未実装)。",
              "Electrode candidates are extracted from this project's conductor "
              "shapes (material 1 = PEC). Voltage is not part of .ofd / .ofdx, "
              "so it shows \"—\" (hand-off to OpenSTF is not implemented).");
    I18n::reg("fam_stf_elec_empty",
              "PEC (材質 1) の形状がありません — 形状タブで導体を追加すると"
              "ここに一覧されます",
              "No PEC (material 1) shapes — add a conductor in the Geometry tab "
              "and it will be listed here");

    // トモグラフィー
    I18n::reg("fam_tomo_hint", "FDTD+逆問題で誘電率分布を再構成。乳腺/木材を画像化。",
              "Reconstruct the permittivity map via FDTD + inverse problem. Images breast tissue / wood.");
    I18n::reg("fam_tomo_antennas", "送受信アンテナ数", "Tx/Rx antenna count");
    I18n::reg("fam_tomo_tx", "送信", "Tx");
    I18n::reg("fam_tomo_rx", "受信", "Rx");
    I18n::reg("fam_tomo_algo", "再構成アルゴリズム", "Reconstruction algorithm");
    I18n::reg("fam_tomo_region", "再構成領域", "Reconstruction region");
    I18n::reg("fam_tomo_multifreq", "多周波再構成 (周波数ホッピング)",
              "Multi-frequency reconstruction (frequency hopping)");

    // ソルバ間連携
    I18n::reg("fam_cross_title", "ソルバ間連携", "Cross-solver workflow");
    I18n::reg("fam_cross_hint", "複数ソルバを組み合わせた解析パイプライン",
              "Analysis pipelines combining multiple solvers");
    I18n::reg("fam_cross_1", "OpenSTF → OpenFDTD: DC電界初期条件をFDTDへ渡す",
              "OpenSTF → OpenFDTD: pass the DC-field initial condition to FDTD");
    I18n::reg("fam_cross_2", "OpenMOM ↔ OpenFDTD: アンテナ単体MoM + 環境FDTD ハイブリッド",
              "OpenMOM ↔ OpenFDTD: antenna-only MoM + environment FDTD hybrid");
    I18n::reg("fam_cross_3", "OpenRTM ← OpenFDTD: 近傍界をFDTD、遠方伝搬をRTM",
              "OpenRTM ← OpenFDTD: near field in FDTD, far propagation in RTM");
    I18n::reg("fam_cross_4", "OpenTHFD vs OpenFDTD: 同じ問題を2手法で比較し検証",
              "OpenTHFD vs OpenFDTD: verify by comparing two methods on the same problem");
    I18n::reg("fam_uw_link", "他ソルバーとの連携チェック",
              "the solver-interoperation check boxes");
    I18n::reg("fam_uw_fdtd", "FDTD ページの設定 (互換モード・実行バイナリ・出力)",
              "the FDTD page settings (compatibility mode, binary, outputs)");
    I18n::reg("fam_uw_rtm", "RTM ページの設定 (シーン・観測条件)",
              "the RTM page settings (scene and observation conditions)");
    I18n::reg("fam_uw_thfd", "THFD ページの設定 (周波数・準静近似・ソルバ・収束条件)",
              "the THFD page settings (frequency, quasi-static option, solver, convergence)");
    I18n::reg("fam_uw_mom", "MoM ページの設定 (導体・基底関数・セグメント長・周波数)",
              "the MoM page settings (conductor, basis function, segment length, frequency)");
    I18n::reg("fam_uw_stf", "STF ページの設定 (電極配置ほか)",
              "the STF page settings (electrode layout and the rest)");
    I18n::reg("fam_uw_tomo", "トモグラフィページの設定 (アンテナ数・再構成法・領域)",
              "the tomography page settings (antenna count, reconstruction, region)");
    return true;
}();

// mock の family[] をそのまま転記 (表示データ)
struct FamilyDef {
    const char *id, *ver, *color, *name, *method, *use, *strengths, *example;
    bool em, optical;
};
const FamilyDef kFamily[6] = {
    { "fdtd", "4.3.2", "#0078D4", "OpenFDTD", "FDTD法 (時間領域差分)", "電磁界汎用",
      "広帯域・パルス応答・任意形状・分散材料",
      "アンテナ放射, EMC, RCS, フォトニック結晶", true, true },
    { "rtm", "1.0.2", "#F59E0B", "OpenRTM", "レイトレーシング法", "電波伝搬の評価",
      "建物スケール伝搬・到達範囲・マルチパス",
      "屋内WiFi, 基地局シミュレーション, 都市伝搬", true, false },
    { "thfd", "4.0.4", "#10B981", "OpenTHFD", "調和界差分法 (FDFD)", "準静電界〜光まで広帯域",
      "周波数領域・高Q構造・単一周波数精密",
      "光共振器, MEMS, バンド構造解析", true, true },
    { "mom", "4.2.0", "#B83280", "OpenMOM", "モーメント法 (MoM)", "線状・面状アンテナ解析に最適",
      "開放領域・遠方界精度高・小規模",
      "ダイポール, 八木, ループ, ヘリカル", true, false },
    { "stf", "4.2.4", "#7C3AED", "OpenSTF", "差分法による静電界",
      "電極と誘電体から成る系の電圧と電界分布",
      "DC定常・収束高速・大規模",
      "コンデンサ, 絶縁設計, 放電解析, バイオセンサ", true, false },
    { "tomo", "—", "#06B6D4", "マイクロ波トモグラフィー", "FDTD + 逆問題", "医療画像・非破壊検査",
      "誘電率分布の再構成",
      "乳がん検出, 木材内部画像化", true, false },
};

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}
} // namespace

// ── FamilyCard ──────────────────────────────────────────────────────────────
FamilyCard::FamilyCard(const QString &name, const QString &ver, const QString &color,
                       const QString &method, const QString &use,
                       const QString &strengths, const QString &example,
                       QWidget *parent)
    : QFrame(parent), m_color(color)
{
    setObjectName("familyCard");
    setCursor(Qt::PointingHandCursor);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(4);

    auto *head = new QHBoxLayout();
    auto *nameL = new QLabel(name, this);
    nameL->setStyleSheet(QString("font-size:14px; font-weight:700; color:%1;").arg(color));
    head->addWidget(nameL);
    // バージョンは実行バイナリから検出していない (モックの想定値) ため
    // 表示しない (絶対規則 5 — 未確認情報を事実のように見せない)
    Q_UNUSED(ver);
    head->addStretch(1);
    m_badge = new QLabel(I18n::tr("fam_selected"), this);
    m_badge->setStyleSheet("background:#DEECF9; color:#0078D4; border-radius:3px; "
                           "padding:1px 6px; font-size:11px;");
    m_badge->setVisible(false);
    head->addWidget(m_badge);
    v->addLayout(head);

    auto *methodL = new QLabel(method + QStringLiteral(" — ") + use, this);
    methodL->setStyleSheet("font-size:11px;");
    methodL->setWordWrap(true);
    v->addWidget(methodL);

    auto *bodyL = new QLabel(QString("<b>%1</b> %2<br/><b>%3</b> %4")
                                 .arg(I18n::tr("fam_strengths"), strengths,
                                      I18n::tr("fam_example"), example), this);
    bodyL->setStyleSheet("font-size:11px;");
    bodyL->setWordWrap(true);
    v->addWidget(bodyL);

    setSelected(false);
}

void FamilyCard::setSelected(bool on)
{
    m_badge->setVisible(on);
    setStyleSheet(QString("#familyCard { border:2px solid %1; border-radius:4px; }")
                      .arg(on ? m_color : QStringLiteral("palette(mid)")));
}

void FamilyCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) emit clicked();
    QFrame::mousePressEvent(e);
}

// ── FamilySolverTab ─────────────────────────────────────────────────────────
FamilySolverTab::FamilySolverTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 姉妹ソルバ一覧 (音響/水中ではカードが 0 枚 → セクションごと隠す)
    m_listSection = new SectionBox(I18n::tr("fam_title"), body);
    auto *intro = new QLabel(I18n::tr("fam_intro"), m_listSection);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    m_listSection->vbox()->addWidget(intro);
    m_cardGrid = new QGridLayout();
    m_cardGrid->setSpacing(8);
    m_listSection->vbox()->addLayout(m_cardGrid);
    v->addWidget(m_listSection);

    // 選択中ソルバの詳細設定
    m_detailSection = new SectionBox(QString(), body);
    m_detailStack = new QStackedWidget(m_detailSection);
    m_detailStack->addWidget(buildFdtdPage());   // [0]
    m_detailStack->addWidget(buildRtmPage());    // [1]
    m_detailStack->addWidget(buildThfdPage());   // [2]
    m_detailStack->addWidget(buildMomPage());    // [3]
    m_detailStack->addWidget(buildStfPage());    // [4]
    m_detailStack->addWidget(buildTomoPage());   // [5]
    m_detailSection->vbox()->addWidget(m_detailStack);
    v->addWidget(m_detailSection);

    // ソルバ間連携 (全て EM 姉妹ソルバ間のパイプライン → 音響/水中では隠す)
    m_crossSection = new SectionBox(I18n::tr("fam_cross_title"), body);
    m_crossSection->vbox()->addWidget(hintLabel(I18n::tr("fam_cross_hint"), m_crossSection));
    for (const char *key : { "fam_cross_1", "fam_cross_2", "fam_cross_3", "fam_cross_4" })
        m_crossSection->vbox()->addWidget(new QCheckBox(I18n::tr(key), m_crossSection));
    // 連携チェックはどこにも読まれていない (未実装)
    m_crossSection->vbox()->addWidget(tabhelp::unwiredNote(m_crossSection, I18n::tr("fam_uw_link")));
    v->addWidget(m_crossSection);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &FamilySolverTab::rebuildCards);
    // 形状の追加・削除・材質変更を電極一覧へ反映する
    connect(project, &Project::changed, this,
            [this] { refreshStfElectrodes(); });
    connect(project, &Project::loaded, this,
            [this] { refreshStfElectrodes(); });
    rebuildCards();
}

void FamilySolverTab::rebuildCards()
{
    for (auto *c : m_cards) {
        m_cardGrid->removeWidget(c);
        c->deleteLater();
    }
    m_cards.clear();
    m_cardIndex.clear();

    const Domain d = m_p->activeDomain();
    int slot = 0;
    for (int i = 0; i < 6; ++i) {
        const FamilyDef &f = kFamily[i];
        const bool visible = (d == Domain::EM && f.em)
                          || (d == Domain::Optical && f.optical);
        if (!visible) continue;
        auto *card = new FamilyCard(
            QString::fromUtf8(f.name), QString::fromUtf8(f.ver),
            QString::fromUtf8(f.color), QString::fromUtf8(f.method),
            QString::fromUtf8(f.use), QString::fromUtf8(f.strengths),
            QString::fromUtf8(f.example), widget());
        connect(card, &FamilyCard::clicked, this, [this, i] { select(i); });
        m_cardGrid->addWidget(card, slot / 2, slot % 2);
        m_cards.push_back(card);
        m_cardIndex.push_back(i);
        ++slot;
    }
    select(m_pick);

    // 音響/水中ドメインでは姉妹ソルバ (全て電磁/光系) が 1 枚も無い。
    // 空のカードグリッドと EM 専用の詳細/連携セクションを見せると
    // 「このドメインの機能」と誤認させるため、セクションごと非表示にする。
    const bool anyCard = !m_cards.isEmpty();
    m_listSection->setVisible(anyCard);
    m_detailSection->setVisible(anyCard);
    m_crossSection->setVisible(d == Domain::EM || d == Domain::Optical);
}

void FamilySolverTab::select(int familyIndex)
{
    m_pick = familyIndex;
    for (int s = 0; s < m_cards.size(); ++s)
        m_cards[s]->setSelected(m_cardIndex[s] == familyIndex);
    m_detailStack->setCurrentIndex(familyIndex);
    // 非表示ドメインでは mock 同様タイトルのソルバ名を空にする
    const bool visible = m_cardIndex.contains(familyIndex);
    m_detailSection->setTitle(I18n::tr("fam_detail_title")
        .arg(visible ? QString::fromUtf8(kFamily[familyIndex].name) : QString()));
}

// ── 詳細設定ページ ──────────────────────────────────────────────────────────
QWidget *FamilySolverTab::buildFdtdPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(hintLabel(I18n::tr("fam_fdtd_hint"), page));
    auto *ofdCk = new QCheckBox(I18n::tr("fam_fdtd_ofd"), page);
    ofdCk->setChecked(true);
    form->addRow(I18n::tr("fam_fdtd_compat"), ofdCk);
    auto *binCk = new QCheckBox(I18n::tr("fam_fdtd_binary_auto"), page);
    binCk->setChecked(true);
    form->addRow(I18n::tr("fam_fdtd_binary"), binCk);
    auto *outRow = new QHBoxLayout();
    auto *evCk = new QCheckBox("ev.ev2 / ev.ev3", page);
    evCk->setChecked(true);
    auto *h5Ck = new QCheckBox("HDF5", page);
    h5Ck->setChecked(true);
    outRow->addWidget(evCk);
    outRow->addWidget(h5Ck);
    outRow->addStretch(1);
    form->addRow(I18n::tr("fam_fdtd_output"), outRow);
    form->addRow(tabhelp::unwiredNote(page, I18n::tr("fam_uw_fdtd")));   // このページの設定は未配線
    return page;
}

QWidget *FamilySolverTab::buildRtmPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(I18n::tr("fam_rtm_rays"), numEdit("100000", 110, page));
    form->addRow(I18n::tr("fam_rtm_maxrefl"), numEdit("10", 70, page));
    form->addRow(I18n::tr("fam_rtm_txpos"), numEdit("0, 0, 10 m", 0, page));
    form->addRow(I18n::tr("fam_rtm_rxarea"), numEdit("-50 〜 50 m (XY)", 0, page));
    auto *scene = new QComboBox(page);
    scene->addItem(I18n::tr("fam_rtm_scene_indoor"));
    scene->addItem(I18n::tr("fam_rtm_scene_urban"));
    scene->addItem(I18n::tr("fam_rtm_scene_suburb"));
    scene->addItem(I18n::tr("fam_rtm_scene_custom"));
    form->addRow(I18n::tr("fam_rtm_scene"), scene);
    auto *row1 = new QHBoxLayout();
    auto *wallCk = new QCheckBox(I18n::tr("fam_rtm_wall"), page);
    wallCk->setChecked(true);
    row1->addWidget(wallCk);
    row1->addWidget(new QCheckBox(I18n::tr("fam_rtm_scatter"), page));
    row1->addStretch(1);
    form->addRow(row1);
    auto *row2 = new QHBoxLayout();
    row2->addWidget(new QCheckBox(I18n::tr("fam_rtm_doppler"), page));
    row2->addWidget(new QCheckBox(I18n::tr("fam_rtm_pol"), page));
    row2->addStretch(1);
    form->addRow(row2);
    form->addRow(tabhelp::unwiredNote(page, I18n::tr("fam_uw_rtm")));   // このページの設定は未配線
    return page;
}

QWidget *FamilySolverTab::buildThfdPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(hintLabel(I18n::tr("fam_thfd_hint"), page));
    auto *freqRow = new QHBoxLayout();
    freqRow->addWidget(numEdit("2.45e9", 110, page));
    freqRow->addWidget(new QLabel("Hz", page));
    freqRow->addStretch(1);
    form->addRow(I18n::tr("fam_freq"), freqRow);
    auto *quasiRow = new QHBoxLayout();
    quasiRow->addWidget(new QCheckBox(I18n::tr("fam_thfd_approx"), page));
    quasiRow->addWidget(new QLabel(I18n::tr("fam_thfd_lowfreq"), page));
    quasiRow->addStretch(1);
    form->addRow(I18n::tr("fam_thfd_quasi"), quasiRow);
    auto *solver = new QComboBox(page);
    solver->addItems({ "BiCGStab", "GMRES", I18n::tr("fam_thfd_direct") });
    form->addRow(I18n::tr("fam_solver"), solver);
    form->addRow(I18n::tr("fam_converge"), numEdit("1e-6", 110, page));
    form->addRow(tabhelp::unwiredNote(page, I18n::tr("fam_uw_thfd")));   // このページの設定は未配線
    return page;
}

QWidget *FamilySolverTab::buildMomPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(hintLabel(I18n::tr("fam_mom_hint"), page));
    auto *cond = new QComboBox(page);
    cond->addItems({ I18n::tr("fam_mom_wire"), I18n::tr("fam_mom_surface"),
                     I18n::tr("fam_mom_mixed") });
    form->addRow(I18n::tr("fam_mom_conductor"), cond);
    auto *basis = new QComboBox(page);
    basis->addItems({ I18n::tr("fam_mom_pulse"), I18n::tr("fam_mom_tri"),
                      "RWG", I18n::tr("fam_mom_sin") });
    basis->setCurrentIndex(2);
    form->addRow(I18n::tr("fam_mom_basis"), basis);
    form->addRow(I18n::tr("fam_mom_seglen"), numEdit("0.05", 70, page));
    auto *freqRow = new QHBoxLayout();
    freqRow->addWidget(numEdit("2.45e9", 110, page));
    freqRow->addWidget(new QLabel("Hz", page));
    freqRow->addStretch(1);
    form->addRow(I18n::tr("fam_freq"), freqRow);
    form->addRow(I18n::tr("fam_mom_input"),
                 new QCheckBox(I18n::tr("fam_mom_nec"), page));
    form->addRow(tabhelp::unwiredNote(page, I18n::tr("fam_uw_mom")));   // このページの設定は未配線
    return page;
}

QWidget *FamilySolverTab::buildStfPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(hintLabel(I18n::tr("fam_stf_hint"), page));
    auto *solver = new QComboBox(page);
    solver->addItems({ "SOR", "CG", I18n::tr("fam_stf_mg") });
    form->addRow(I18n::tr("fam_solver"), solver);
    form->addRow(I18n::tr("fam_stf_relax"), numEdit("1.85", 70, page));
    form->addRow(I18n::tr("fam_converge"), numEdit("1e-7", 110, page));

    // 電極一覧はプロジェクトの導体形状 (PEC) から作る実データ
    m_stfElectrodes = new QTableWidget(0, 4, page);
    m_stfElectrodes->setHorizontalHeaderLabels({ "#", I18n::tr("fam_stf_name"),
                                                 I18n::tr("fam_stf_volt"),
                                                 I18n::tr("fam_stf_shape") });
    m_stfElectrodes->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_stfElectrodes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_stfElectrodes->verticalHeader()->setVisible(false);
    m_stfElectrodes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stfElectrodes->setMaximumHeight(140);
    form->addRow(I18n::tr("fam_stf_electrodes"), m_stfElectrodes);
    auto *elecNote = hintLabel(I18n::tr("fam_stf_elec_note"), page);
    elecNote->setStyleSheet("color:#7A7A7A; font-size:11px;");
    form->addRow(elecNote);
    form->addRow(tabhelp::unwiredNote(page, I18n::tr("fam_uw_stf")));   // このページの設定は未配線
    refreshStfElectrodes();
    return page;
}

// プロジェクトの PEC 形状 (材質 1) → 電極一覧。
// 静電界解析 (OpenSTF) の電極は導体面なので、モデル中の PEC ユニットが
// そのまま電極候補になる。電圧はモデルに無いので「—」を出す (捏造しない)。
void FamilySolverTab::refreshStfElectrodes()
{
    if (!m_stfElectrodes) return;
    const QVector<Geometry> &geos = m_p->geometries();
    QVector<int> pec;
    for (int i = 0; i < geos.size(); ++i)
        if (geos[i].materialId == 1) pec.push_back(i);

    m_stfElectrodes->clearContents();
    m_stfElectrodes->clearSpans();   // 前回の結合セルを解除
    if (pec.isEmpty()) {
        m_stfElectrodes->setRowCount(1);
        m_stfElectrodes->setItem(0, 0,
            new QTableWidgetItem(I18n::tr("fam_stf_elec_empty")));
        m_stfElectrodes->setSpan(0, 0, 1, 4);
        return;
    }
    m_stfElectrodes->setRowCount(pec.size());
    for (int r = 0; r < pec.size(); ++r) {
        const Geometry &g = geos[pec[r]];
        m_stfElectrodes->setItem(r, 0,
            new QTableWidgetItem(QString::number(r + 1)));
        m_stfElectrodes->setItem(r, 1, new QTableWidgetItem(
            g.name.isEmpty() ? QStringLiteral("unit%1").arg(pec[r] + 1)
                             : g.name));
        // 電圧はモデルに存在しない量 (.ofd にも .ofdx にも無い)
        m_stfElectrodes->setItem(r, 2,
            new QTableWidgetItem(QString::fromUtf8("—")));
        // 形状名は共通キー (ge_shape_<code>) を使う。外接直方体の寸法を添える。
        const QString shape =
            I18n::tr(QStringLiteral("ge_shape_%1").arg(g.shape));
        const QString size = QStringLiteral("%1 × %2 × %3 m")
            .arg(g.g[1] - g.g[0], 0, 'g', 3)
            .arg(g.g[3] - g.g[2], 0, 'g', 3)
            .arg(g.g[5] - g.g[4], 0, 'g', 3);
        m_stfElectrodes->setItem(r, 3,
            new QTableWidgetItem(shape + "  " + size));
    }
}

QWidget *FamilySolverTab::buildTomoPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(hintLabel(I18n::tr("fam_tomo_hint"), page));
    auto *antRow = new QHBoxLayout();
    antRow->addWidget(new QLabel(I18n::tr("fam_tomo_tx"), page));
    antRow->addWidget(numEdit("16", 70, page));
    antRow->addWidget(new QLabel(I18n::tr("fam_tomo_rx"), page));
    antRow->addWidget(numEdit("16", 70, page));
    antRow->addStretch(1);
    form->addRow(I18n::tr("fam_tomo_antennas"), antRow);
    auto *algo = new QComboBox(page);
    algo->addItems({ "Born", "Rytov", "DBIM", "CSI" });
    algo->setCurrentIndex(2);
    form->addRow(I18n::tr("fam_tomo_algo"), algo);
    form->addRow(I18n::tr("fam_tomo_region"),
                 numEdit("0.10 × 0.10 × 0.05 m, 50³", 0, page));
    auto *mfCk = new QCheckBox(I18n::tr("fam_tomo_multifreq"), page);
    mfCk->setChecked(true);
    form->addRow(mfCk);
    form->addRow(tabhelp::unwiredNote(page, I18n::tr("fam_uw_tomo")));   // このページの設定は未配線
    return page;
}
