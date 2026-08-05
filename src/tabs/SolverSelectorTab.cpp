// SolverSelectorTab.cpp
#include "SolverSelectorTab.h"
#include "../core/Project.h"
#include "../core/RoomAcoustics.h"
#include "../core/SolverSelection.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 ssel_) ────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("ssel_title_fmt", "ソルバ選択 (%1ドメイン · %2種)",
              "Solver (%1 domain · %2 types)");
    I18n::reg("ssel_hint",
              "CST Studio Suite 風のソルバ選択。問題に最適な解法を選ぶ。\n"
              "現在のドメインで意味のあるソルバのみ表示。",
              "CST Studio-style solver picker. Choose the method best suited to the problem.\n"
              "Only solvers meaningful in the current domain are shown.");
    I18n::reg("ssel_recommended", "推奨", "Recommended");
    I18n::reg("ssel_suggest_title", "選定の目安 (現在の設定から算出)",
              "Selection guide (computed from the current settings)");
    I18n::reg("ssel_unwired",
              "ソルバ選択の実行連携は未実装です (実行は常に ofd 系カーネル)。"
              "カードは一覧表示のみで、クリックしても選択は反映されません。",
              "Solver selection is not wired to execution yet (runs always use "
              "the ofd-family kernels). Cards are display-only; clicking does "
              "not select anything.");
    // mock i18n の sv_bem (ソルバ共通語彙)。カード名だけ日英を切り替える。
    I18n::reg("ssel_bem", "BEM (境界要素)", "BEM");

    // ── 算出値の説明 (共通) ──
    I18n::reg("ssel_calc_note",
              "▸ 下の数値はプロジェクトの寸法・メッシュ・周波数 (音響は室容積と"
              "残響時間、水中はソナー周波数と距離) から算出した設計指標です。"
              "ソルバーの実行結果ではありません。判定基準の出典は "
              "src/core/SolverSelection.h に記載。",
              "▸ The numbers below are design metrics computed from the "
              "project's dimensions, mesh and frequency (room volume and "
              "reverberation time for acoustics; sonar frequency and range "
              "underwater). They are not solver results. Sources for the "
              "criteria are listed in src/core/SolverSelection.h.");
    I18n::reg("ssel_na", "未計算", "not computed");
    I18n::reg("ssel_need_mesh",
              "メッシュが未設定のため算出できません (メッシュタブで設定)",
              "Cannot be computed: no mesh is defined (set it in the Mesh tab)");
    I18n::reg("ssel_need_freq",
              "解析周波数が未設定のため算出できません (全般タブで設定)",
              "Cannot be computed: no analysis frequency (set it in the General "
              "tab)");
    // EM / 光 共通
    I18n::reg("ssel_em_size", "電気サイズ L/λ:", "Electrical size L/λ:");
    I18n::reg("ssel_em_size_fmt", "L/λ = %1", "L/λ = %1");
    I18n::reg("ssel_em_size_small",
              "領域長 %1 / 波長 %2 → 波長オーダーの構造。FDTD (Transient) が適合",
              "domain %1 / wavelength %2 → structure of the order of a "
              "wavelength; FDTD (transient) is suitable");
    I18n::reg("ssel_em_size_mid",
              "領域長 %1 / 波長 %2 → FDTD で解けるが規模大。MoM も検討",
              "domain %1 / wavelength %2 → FDTD is feasible but large; also "
              "consider MoM");
    I18n::reg("ssel_em_size_big",
              "領域長 %1 / 波長 %2 → 電気的に非常に大きい。漸近解法 (SBR/PO) が"
              "現実的",
              "domain %1 / wavelength %2 → electrically very large; asymptotic "
              "methods (SBR/PO) are realistic");
    I18n::reg("ssel_res", "格子分解能:", "Grid resolution:");
    I18n::reg("ssel_res_fmt", "λ/Δx = %1", "λ/Δx = %1");
    I18n::reg("ssel_res_ok",
              "最大セル %1 で 1 波長あたり %2 セル (10〜20 が目安)",
              "largest cell %1 gives %2 cells per wavelength (10-20 is the "
              "guideline)");
    I18n::reg("ssel_res_ng",
              "最大セル %1 で 1 波長あたり %2 セルしかない — 数値分散が大きい",
              "largest cell %1 gives only %2 cells per wavelength — numerical "
              "dispersion will be significant");
    I18n::reg("ssel_qres", "分解できる Q の上限:", "Resolvable Q (upper bound):");
    I18n::reg("ssel_qres_fmt", "Q ≤ %1", "Q ≤ %1");
    I18n::reg("ssel_qres_t",
              "実行長 T = %1 ステップ × Δt = %2 に対し Q ≤ f·T。"
              "これを超える高 Q 共振は Frequency Domain / Eigenmode 向き",
              "with a run length T = %1 steps × Δt = %2, Q ≤ f·T. Sharper "
              "resonances belong to a frequency-domain or eigenmode solver");
    I18n::reg("ssel_abc", "吸収境界:", "Absorbing boundary:");
    I18n::reg("ssel_abc_pml", "PML %1 層 — 開放領域の放射問題に対応",
              "PML, %1 layers — suitable for open-region radiation problems");
    I18n::reg("ssel_abc_mur",
              "1 次 Mur — 斜入射の反射が大きい。遠方界を精密に見るなら PML か MoM",
              "first-order Mur — large reflection at oblique incidence; use PML "
              "or MoM for accurate far fields");
    // 光
    I18n::reg("ssel_lambda", "解析波長:", "Analysis wavelength:");
    I18n::reg("ssel_lambda_t", "光ドメイン設定の λ = %1〜%2 nm (中心 %3 nm)",
              "optical settings λ = %1-%2 nm (centre %3 nm)");
    // 音響
    I18n::reg("ssel_band", "対象帯域:", "Frequency band:");
    I18n::reg("ssel_band_t", "音響タブの設定 (%1)", "from the acoustic tab (%1)");
    I18n::reg("ssel_schroeder", "Schroeder 周波数:", "Schroeder frequency:");
    I18n::reg("ssel_schroeder_fmt", "f_c = %1 Hz", "f_c = %1 Hz");
    I18n::reg("ssel_schroeder_t",
              "V = %1 m³ · T60(500Hz) = %2 s → f_c 未満は波動論 (FDTD/モーダル)、"
              "以上は幾何音響 (Ray/Image-Source) の領域",
              "V = %1 m³, T60(500 Hz) = %2 s → below f_c use wave-based methods "
              "(FDTD/modal), above it geometrical acoustics (ray / image "
              "source)");
    I18n::reg("ssel_schroeder_na",
              "室容積または残響時間が未設定のため算出できません",
              "Cannot be computed: room volume or reverberation time is not set");
    // 水中
    I18n::reg("ssel_range", "伝搬距離:", "Propagation range:");
    I18n::reg("ssel_range_fmt", "%1 km", "%1 km");
    I18n::reg("ssel_range_t",
              "球面拡散 + 吸収の伝搬損失 TL ≈ %1 dB。長距離はレイ法 (Bellhop) が"
              "現実的",
              "spherical spreading plus absorption gives TL ≈ %1 dB; ray methods "
              "(Bellhop) are realistic at long range");
    I18n::reg("ssel_freq", "ソナー周波数:", "Sonar frequency:");
    I18n::reg("ssel_freq_fmt", "%1 kHz", "%1 kHz");
    I18n::reg("ssel_freq_t",
              "Thorp の吸収係数 α = %1 dB/km (全行程で %2 dB)。低周波・中距離は "
              "PE も適合",
              "Thorp absorption α = %1 dB/km (%2 dB over the whole path); at low "
              "frequency and medium range a PE solver also fits");
    return true;
}();

// ドメインビット
enum : unsigned { EM = 1, OPT = 2, AC = 4, UW = 8 };

unsigned domainBit(ofd::Domain d)
{
    switch (d) {
        case ofd::Domain::Optical:    return OPT;
        case ofd::Domain::Acoustic:   return AC;
        case ofd::Domain::Underwater: return UW;
        default:                      return EM;
    }
}

// mock の all[] をそのまま転記 (表示データ)。カード名・説明文は mock のハード
// コード表記そのまま (日本語固定)。nameKey が非 nullptr のものだけ、mock の i18n
// テーブルに ja/en があるのでカード名を I18n 経由で切り替える。
struct SolverDef {
    const char *ic, *name, *s, *best;
    unsigned domains, rec;
    const char *nameKey;
};
const SolverDef kSolvers[15] = {
    { "⏱", "Transient (FDTD)",        "広帯域・時間応答・パルス",   "アンテナ, EMC, 透過スペクトル", EM|OPT|AC|UW, EM|OPT|AC, nullptr },
    { "〰", "Frequency Domain (FEM)",  "単一周波数・高Q構造",       "フィルタ, 共振器, 散乱",        EM|OPT|AC, 0, nullptr },
    { "⤓", "Eigenmode Solver",        "共振モード抽出",            "キャビティ, 結晶バンド構造",    EM|OPT|AC, 0, nullptr },
    { "⏧", "RCWA (Rigorous CWA)",     "周期格子・厳密結合波解析",   "DBR, メタサーフェス周期",       OPT, 0, nullptr },
    { "⫾", "STACK (Transfer Matrix)", "薄膜多層 (解析的)",          "反射防止膜, 偏光フィルタ",      OPT, 0, nullptr },
    { "⌖", "Integral Equation (MoM)", "開放領域・遠方界",          "アンテナアレイ, RCS, 大型物体", EM, 0, nullptr },
    { "⏎", "Multilayer Solver",       "層状構造",                  "PCB, 薄膜光学",                EM|OPT, 0, nullptr },
    { "☼", "Asymptotic (SBR/PO)",     "波長 ≪ 物体",              "ミリ波RCS, 大型ステルス",       EM, 0, nullptr },
    { "☄", "Ray Tracing (Geometric)", "幾何光学/音響",             "カメラ, 室内音響",              OPT|AC, 0, nullptr },
    { "⌬", "Hybrid FDTD+Ray",         "マルチスケール",            "メタレンズ + 光学系",           OPT|AC, 0, nullptr },
    // mock i18n の sv_bem (BEM (境界要素) / BEM)。境界要素法 = 開放境界の放射・散乱。
    { "◫", "BEM",                     "境界要素法・開放境界",       "音響放射, 散乱体, 水中ターゲット", AC|UW, 0, "ssel_bem" },
    { "🐬", "Bellhop (Gauss. beam)",   "水中音響レイトレース",       "SOFAR, 長距離(>10km)",         UW, UW, nullptr },
    { "~",  "Parabolic Equation (PE)", "放物方程式・RAM/RAMGeo",    "中距離水中, 低周波",            UW, 0, nullptr },
    { "🏛", "Image-Source Method",     "鏡像法",                    "直方体ホール, 初期反射",        AC, 0, nullptr },
    { "🎵", "Modal Analysis",          "室内モーダル",              "小規模ルーム<200Hz",           AC, 0, nullptr },
};

void clearLayout(QLayout *lay)
{
    if (!lay) return;
    while (QLayoutItem *it = lay->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        // QLayout は QLayoutItem 派生なので it->layout() == it。
        // 中身を再帰的に空にしたら delete は下の一回だけ (二重解放になる)。
        else if (QLayout *l = it->layout()) clearLayout(l);
        delete it;
    }
}

// ── 算出ヘルパー (Qt 非依存の式は core/SolverSelection にある) ───────────────

// 有効な軸メッシュから領域の最大辺長と最大セル幅 [m] を取る。
// 有効な軸が 1 本も無ければ false (「未計算」表示にする)。
bool meshExtents(const ofd::Project &p, double &maxLen_m, double &dxMax_m)
{
    maxLen_m = 0.0;
    dxMax_m = 0.0;
    bool any = false;
    for (int a = 0; a < 3; ++a) {
        const ofd::MeshAxis &m = p.mesh(a);
        if (!m.isValid()) continue;
        any = true;
        maxLen_m = std::max(maxLen_m, m.max() - m.min());
        for (int i = 0; i < m.divs.size(); ++i)
            dxMax_m = std::max(dxMax_m,
                               (m.nodes[i + 1] - m.nodes[i]) / m.divs[i]);
    }
    return any && maxLen_m > 0.0 && dxMax_m > 0.0;
}

// 表示用の書式 (VerificationTab の formatFreq / formatLength と同形式)
QString fmtLength(double m)
{
    const double a = std::fabs(m);
    if (!(a > 0.0) || !std::isfinite(m)) return QStringLiteral("—");
    if (a < 1e-6) return QStringLiteral("%1 nm").arg(m * 1e9, 0, 'g', 3);
    if (a < 1e-3) return QStringLiteral("%1 µm").arg(m * 1e6, 0, 'g', 3);
    if (a < 1.0)  return QStringLiteral("%1 mm").arg(m * 1e3, 0, 'g', 3);
    return QStringLiteral("%1 m").arg(m, 0, 'g', 3);
}

QString fmtTime(double s)
{
    const double a = std::fabs(s);
    if (!(a > 0.0) || !std::isfinite(s)) return QStringLiteral("—");
    if (a < 1e-12) return QStringLiteral("%1 fs").arg(s * 1e15, 0, 'g', 3);
    if (a < 1e-9)  return QStringLiteral("%1 ps").arg(s * 1e12, 0, 'g', 3);
    if (a < 1e-6)  return QStringLiteral("%1 ns").arg(s * 1e9, 0, 'g', 3);
    if (a < 1e-3)  return QStringLiteral("%1 µs").arg(s * 1e6, 0, 'g', 3);
    if (a < 1.0)   return QStringLiteral("%1 ms").arg(s * 1e3, 0, 'g', 3);
    return QStringLiteral("%1 s").arg(s, 0, 'g', 3);
}

// 大きい数を読みやすく (Q の上限表示用)
QString fmtBig(double v)
{
    if (!(v > 0.0) || !std::isfinite(v)) return QStringLiteral("—");
    if (v >= 1e4) return QStringLiteral("%1").arg(v, 0, 'g', 3);
    return QStringLiteral("%1").arg(v, 0, 'f', (v < 10.0) ? 2 : 0);
}

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)        css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "warn") == 0) css += "background:#FFF4CE; color:#9D5D00;";
    else if (qstrcmp(kind, "acc") == 0)  css += "background:#DEECF9; color:#0078D4;";
    else                                  css += "background:palette(midlight);";
    b->setStyleSheet(css);
    return b;
}
} // namespace

// ── SolverSelectorTab ───────────────────────────────────────────────────────
SolverSelectorTab::SolverSelectorTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    m_cardSection = new SectionBox(QString(), body);
    auto *hint = new QLabel(I18n::tr("ssel_hint"), m_cardSection);
    hint->setWordWrap(true);
    m_cardSection->vbox()->addWidget(hint);
    // カードは表示のみ — クリックしても実行ソルバは変わらない (絶対規則 5)
    auto *unwired = new QLabel(I18n::tr("ssel_unwired"), m_cardSection);
    unwired->setWordWrap(true);
    unwired->setStyleSheet("font-size:11px; color:palette(mid);");
    m_cardSection->vbox()->addWidget(unwired);
    m_cardGrid = new QGridLayout();
    m_cardGrid->setSpacing(6);
    m_cardSection->vbox()->addLayout(m_cardGrid);
    v->addWidget(m_cardSection);

    m_hintSection = new SectionBox(I18n::tr("ssel_suggest_title"), body);
    // 数値は全てプロジェクト設定からの算出値 (ソルバー実行結果ではない)
    auto *calcNote = new QLabel(I18n::tr("ssel_calc_note"), m_hintSection);
    calcNote->setWordWrap(true);
    calcNote->setStyleSheet("font-size:11px; color:palette(mid);");
    m_hintSection->vbox()->addWidget(calcNote);
    m_hintBox = new QVBoxLayout();
    m_hintBox->setSpacing(4);
    m_hintSection->vbox()->addLayout(m_hintBox);
    v->addWidget(m_hintSection);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &SolverSelectorTab::rebuild);
    // 目安の数値はプロジェクト設定から算出するので、編集・読込に追従させる
    connect(project, &Project::changed, this, &SolverSelectorTab::refreshHints);
    connect(project, &Project::loaded, this, &SolverSelectorTab::rebuild);
    rebuild();
}

void SolverSelectorTab::rebuild()
{
    rebuildCards();
    refreshHints();
}

void SolverSelectorTab::rebuildCards()
{
    const Domain d = m_p->activeDomain();
    const unsigned bit = domainBit(d);

    // ── ソルバカード ──
    clearLayout(m_cardGrid);
    // 推奨: rec にドメインを含むもの。無ければ先頭の可視ソルバ (mock と同じ)
    bool anyRec = false;
    for (const SolverDef &s : kSolvers)
        if ((s.domains & bit) && (s.rec & bit)) { anyRec = true; break; }

    int slot = 0, visibleCount = 0;
    bool first = true;
    for (const SolverDef &s : kSolvers) {
        if (!(s.domains & bit)) continue;
        ++visibleCount;
        const bool recommended = anyRec ? (s.rec & bit) != 0 : first;
        first = false;

        auto *card = new QFrame(widget());
        card->setObjectName("solverCard");
        // クリックしても何も起きない (選択未実装) のでリンク風カーソルにしない
        card->setCursor(Qt::ArrowCursor);
        card->setStyleSheet(QString("#solverCard { border:1px solid %1; border-radius:4px; }")
                                .arg(recommended ? QStringLiteral("#0078D4")
                                                 : QStringLiteral("palette(mid)")));
        auto *cv = new QVBoxLayout(card);
        cv->setContentsMargins(10, 8, 10, 8);
        cv->setSpacing(3);
        auto *head = new QHBoxLayout();
        auto *ic = new QLabel(QString::fromUtf8(s.ic), card);
        ic->setStyleSheet("font-size:18px; color:#0078D4;");
        head->addWidget(ic);
        auto *nameL = new QLabel(s.nameKey ? I18n::tr(s.nameKey)
                                           : QString::fromUtf8(s.name), card);
        nameL->setStyleSheet("font-size:12px; font-weight:600;");
        head->addWidget(nameL);
        head->addStretch(1);
        if (recommended)
            head->addWidget(makeBadge(I18n::tr("ssel_recommended"), "acc", card));
        cv->addLayout(head);
        auto *subL = new QLabel(QString::fromUtf8(s.s), card);
        subL->setStyleSheet("font-size:10px;");
        subL->setWordWrap(true);
        cv->addWidget(subL);
        auto *bestL = new QLabel(QString::fromUtf8(s.best), card);
        bestL->setStyleSheet("font-size:10px;");
        bestL->setWordWrap(true);
        cv->addWidget(bestL);

        m_cardGrid->addWidget(card, slot / 2, slot % 2);
        ++slot;
    }
    m_cardSection->setTitle(I18n::tr("ssel_title_fmt")
        .arg(domainKey(d).toUpper()).arg(visibleCount));
}

// ── 自動選定ヒント (プロジェクト設定からの算出値) ───────────────────────────
void SolverSelectorTab::refreshHints()
{
    const Domain d = m_p->activeDomain();
    clearLayout(m_hintBox);
    auto addHint = [this](const QString &badgeText, const char *kind,
                          const QString &mono, const QString &note) {
        auto *row = new QHBoxLayout();
        row->setSpacing(6);
        row->addWidget(makeBadge(badgeText, kind, widget()));
        if (!mono.isEmpty()) {
            auto *m = new QLabel(mono, widget());
            m->setStyleSheet(Theme::monoQss());
            row->addWidget(m);
        }
        auto *n = new QLabel(note, widget());
        n->setWordWrap(true);
        row->addWidget(n, 1);
        m_hintBox->addLayout(row);
    };
    const QString na = I18n::tr("ssel_na");
    double maxLen = 0.0, dxMax = 0.0;
    const bool haveMesh = meshExtents(*m_p, maxLen, dxMax);

    switch (d) {
    case Domain::EM:
    case Domain::Optical: {
        // 波長: EM は解析周波数1の中心から、光は光ドメイン設定の λ から取る
        double lambda_m = 0.0;
        if (d == Domain::Optical) {
            const OpticalOpts &o = m_p->optical();
            const double lc_nm = 0.5 * (o.lambdaMin + o.lambdaMax);
            lambda_m = lc_nm * 1e-9;
            addHint(I18n::tr("ssel_lambda"), "acc",
                    QStringLiteral("λ = %1 nm").arg(lc_nm, 0, 'f', 1),
                    I18n::tr("ssel_lambda_t")
                        .arg(o.lambdaMin, 0, 'f', 1)
                        .arg(o.lambdaMax, 0, 'f', 1)
                        .arg(lc_nm, 0, 'f', 1));
        } else {
            const GeneralOpts &g = m_p->general();
            const double fc = g.hasF1 ? 0.5 * (g.f1min + g.f1max) : 0.0;
            lambda_m = selsolver::wavelength(selsolver::kC0, fc);
        }

        // ① 電気サイズ L/λ
        if (!haveMesh) {
            addHint(I18n::tr("ssel_em_size"), "plain", na,
                    I18n::tr("ssel_need_mesh"));
        } else if (!(lambda_m > 0.0)) {
            addHint(I18n::tr("ssel_em_size"), "plain", na,
                    I18n::tr("ssel_need_freq"));
        } else {
            const double lo = selsolver::electricalSize(maxLen, lambda_m);
            const char *kind = (lo <= 20.0) ? "ok" : "warn";
            const char *key = (lo <= 10.0) ? "ssel_em_size_small"
                            : (lo <= 100.0) ? "ssel_em_size_mid"
                                            : "ssel_em_size_big";
            addHint(I18n::tr("ssel_em_size"), kind,
                    I18n::tr("ssel_em_size_fmt").arg(lo, 0, 'f', 2),
                    I18n::tr(key).arg(fmtLength(maxLen), fmtLength(lambda_m)));
        }

        // ② 格子分解能 λ/Δx
        if (!haveMesh) {
            addHint(I18n::tr("ssel_res"), "plain", na, I18n::tr("ssel_need_mesh"));
        } else if (!(lambda_m > 0.0)) {
            addHint(I18n::tr("ssel_res"), "plain", na, I18n::tr("ssel_need_freq"));
        } else {
            const double cpw = selsolver::cellsPerWavelength(lambda_m, dxMax);
            addHint(I18n::tr("ssel_res"), cpw >= 10.0 ? "ok" : "warn",
                    I18n::tr("ssel_res_fmt").arg(cpw, 0, 'f', 1),
                    I18n::tr(cpw >= 10.0 ? "ssel_res_ok" : "ssel_res_ng")
                        .arg(fmtLength(dxMax))
                        .arg(cpw, 0, 'f', 1));
        }

        // ③ 実行長から決まる Q の分解限界 (Q ≤ f·T)
        const GeneralOpts &g = m_p->general();
        const double dt = (g.dt > 0.0) ? g.dt : m_p->courantDt();
        const double fq = (d == Domain::Optical)
                              ? ((lambda_m > 0.0) ? selsolver::kC0 / lambda_m : 0.0)
                              : (g.hasF1 ? 0.5 * (g.f1min + g.f1max) : 0.0);
        const double T = dt * g.maxiter;
        const double qmax = selsolver::maxResolvableQ(fq, T);
        if (qmax > 0.0)
            addHint(I18n::tr("ssel_qres"), "ok",
                    I18n::tr("ssel_qres_fmt").arg(fmtBig(qmax)),
                    I18n::tr("ssel_qres_t")
                        .arg(g.maxiter).arg(fmtTime(dt)));
        else
            addHint(I18n::tr("ssel_qres"), "plain", na,
                    I18n::tr(haveMesh ? "ssel_need_freq" : "ssel_need_mesh"));

        // ④ 吸収境界の設定 (プロジェクトの実設定)
        addHint(I18n::tr("ssel_abc"), g.abc == 1 ? "ok" : "warn", QString(),
                g.abc == 1 ? I18n::tr("ssel_abc_pml").arg(g.pmlL)
                           : I18n::tr("ssel_abc_mur"));
        break;
    }
    case Domain::Acoustic: {
        const AcousticOpts &a = m_p->acoustic();
        // 対象帯域はタブの設定そのもの (実データ)
        const QString band = (a.bandRange == 0) ? QStringLiteral("125 Hz〜")
                           : (a.bandRange == 1) ? QStringLiteral("500 Hz〜2 kHz")
                                                : QStringLiteral("125 Hz〜16 kHz");
        addHint(I18n::tr("ssel_band"), "ok",
                band + (a.thirdOctave ? QStringLiteral(" · 1/3 oct")
                                      : QStringLiteral(" · 1/1 oct")),
                I18n::tr("ssel_band_t")
                    .arg(a.thirdOctave ? QStringLiteral("1/3 oct")
                                       : QStringLiteral("1/1 oct")));

        // Schroeder 周波数 (室容積と 500 Hz 帯の残響時間から)
        const double t60 = roomac::rt60(a, 2);      // band 2 = 500 Hz
        const double fc = selsolver::schroederFrequency(t60, a.volume);
        if (fc > 0.0)
            addHint(I18n::tr("ssel_schroeder"), "ok",
                    I18n::tr("ssel_schroeder_fmt").arg(fc, 0, 'f', 1),
                    I18n::tr("ssel_schroeder_t")
                        .arg(a.volume, 0, 'f', 0)
                        .arg(t60, 0, 'f', 2));
        else
            addHint(I18n::tr("ssel_schroeder"), "plain", na,
                    I18n::tr("ssel_schroeder_na"));
        break;
    }
    case Domain::Underwater: {
        const UnderwaterOpts &u = m_p->underwater();
        const double alpha = selsolver::thorpAbsorption_dBkm(u.sonarFreq_kHz);
        const double tl = selsolver::sphericalTransmissionLoss_dB(u.rangeMax_km,
                                                                 alpha);
        if (tl > 0.0)
            addHint(I18n::tr("ssel_range"), "ok",
                    I18n::tr("ssel_range_fmt").arg(u.rangeMax_km, 0, 'f', 1),
                    I18n::tr("ssel_range_t").arg(tl, 0, 'f', 1));
        else
            addHint(I18n::tr("ssel_range"), "plain", na, QString());
        addHint(I18n::tr("ssel_freq"), "ok",
                I18n::tr("ssel_freq_fmt").arg(u.sonarFreq_kHz, 0, 'f', 2),
                I18n::tr("ssel_freq_t")
                    .arg(alpha, 0, 'f', 3)
                    .arg(alpha * u.rangeMax_km, 0, 'f', 1));
        break;
    }
    }
}
