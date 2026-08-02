// SolverSelectorTab.cpp
#include "SolverSelectorTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

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
    I18n::reg("ssel_suggest_title", "自動選定ヒント", "Auto-suggest");
    // mock i18n の sv_bem (ソルバ共通語彙)。カード名だけ日英を切り替える。
    I18n::reg("ssel_bem", "BEM (境界要素)", "BEM");

    // EM
    I18n::reg("ssel_em_size", "電気サイズ判定:", "Electrical size check:");
    I18n::reg("ssel_em_size_t", "→ FDTD or MoM が適合", "→ FDTD or MoM suitable");
    I18n::reg("ssel_q", "Q値推定:", "Estimated Q:");
    I18n::reg("ssel_emq_t", "→ Frequency Domain も検討", "→ also consider Frequency Domain");
    I18n::reg("ssel_pml", "PML遮断:", "PML cutoff:");
    I18n::reg("ssel_pml_t", "遠方場必要 → MoM 強化", "far field needed → strengthen MoM");
    // Optical
    I18n::reg("ssel_ratio", "波長/構造比:", "Wavelength/feature ratio:");
    I18n::reg("ssel_ratio_t", "→ FDTD 必須 (回折優位)", "→ FDTD required (diffraction dominant)");
    I18n::reg("ssel_optq_t", "→ Eigenmode で精密抽出推奨",
              "→ Eigenmode recommended for precise extraction");
    I18n::reg("ssel_macro", "大規模光学系:", "Large optical system:");
    I18n::reg("ssel_macro_t", "マクロスケール → Hybrid 推奨", "macro scale → Hybrid recommended");
    // Acoustic
    I18n::reg("ssel_band", "周波数帯:", "Frequency band:");
    I18n::reg("ssel_band_t", "→ 低域FDTD + 中高域Ray", "→ low band FDTD + mid/high band Ray");
    I18n::reg("ssel_hall", "ホール形状:", "Hall geometry:");
    I18n::reg("ssel_hall_t", "複雑 → Ray Tracing 推奨", "complex → Ray Tracing recommended");
    // Underwater
    I18n::reg("ssel_range", "伝搬距離:", "Propagation range:");
    I18n::reg("ssel_range_t", "→ Bellhop 推奨", "→ Bellhop recommended");
    I18n::reg("ssel_freq", "周波数:", "Frequency:");
    I18n::reg("ssel_freq_t", "→ PE も適合", "→ PE also suitable");
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
    m_cardGrid = new QGridLayout();
    m_cardGrid->setSpacing(6);
    m_cardSection->vbox()->addLayout(m_cardGrid);
    v->addWidget(m_cardSection);

    m_hintSection = new SectionBox(I18n::tr("ssel_suggest_title"), body);
    m_hintBox = new QVBoxLayout();
    m_hintBox->setSpacing(4);
    m_hintSection->vbox()->addLayout(m_hintBox);
    v->addWidget(m_hintSection);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &SolverSelectorTab::rebuild);
    rebuild();
}

void SolverSelectorTab::rebuild()
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
        card->setCursor(Qt::PointingHandCursor);
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

    // ── 自動選定ヒント ──
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
    switch (d) {
    case Domain::EM:
        addHint(I18n::tr("ssel_em_size"), "ok", "L/λ = 8.4", I18n::tr("ssel_em_size_t"));
        addHint(I18n::tr("ssel_q"), "ok", "~10⁴", I18n::tr("ssel_emq_t"));
        addHint(I18n::tr("ssel_pml"), "warn", QString(), I18n::tr("ssel_pml_t"));
        break;
    case Domain::Optical:
        addHint(I18n::tr("ssel_ratio"), "ok", "λ/d = 3.1", I18n::tr("ssel_ratio_t"));
        addHint(I18n::tr("ssel_q"), "ok", "~10⁵", I18n::tr("ssel_optq_t"));
        addHint(I18n::tr("ssel_macro"), "plain", QString(), I18n::tr("ssel_macro_t"));
        break;
    case Domain::Acoustic:
        addHint(I18n::tr("ssel_band"), "ok", "63Hz~16kHz", I18n::tr("ssel_band_t"));
        addHint(I18n::tr("ssel_hall"), "plain", QString(), I18n::tr("ssel_hall_t"));
        break;
    case Domain::Underwater:
        addHint(I18n::tr("ssel_range"), "ok", "50 km", I18n::tr("ssel_range_t"));
        addHint(I18n::tr("ssel_freq"), "ok", "3.5 kHz", I18n::tr("ssel_freq_t"));
        break;
    }
}
