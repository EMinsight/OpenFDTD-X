// ComponentsTab.cpp
#include "ComponentsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

using namespace ofd;

// ── file-local i18n vocabulary (キーは i18n.js の cl_* と同一) ───────────────
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("cl_search",    "コンポーネントを検索…",      "Search components…");
    ofd::I18n::reg("cl_search_ph", "🔎 検索 / search",            "🔎 search");
    ofd::I18n::reg("cl_all",       "すべて",                      "All");
    ofd::I18n::reg("cl_basic",     "基本形状",                    "Basic shapes");
    ofd::I18n::reg("cl_photonic",  "フォトニクス",                "Photonics");
    ofd::I18n::reg("cl_metal",     "金属・プラズモニクス",        "Metal & plasmonics");
    ofd::I18n::reg("cl_grating",   "格子・周期構造",              "Gratings & periodic");
    ofd::I18n::reg("cl_lens",      "レンズ・自由曲面",            "Lens & freeform");
    ofd::I18n::reg("cl_antenna",   "アンテナ",                    "Antenna");
    ofd::I18n::reg("cl_acoustic",  "音響",                        "Acoustic");
    ofd::I18n::reg("cl_source",    "波源",                        "Sources");
    ofd::I18n::reg("cl_monitor",   "モニター",                    "Monitors");
    ofd::I18n::reg("cl_imported",  "取込モデル",                  "Imported models");
    ofd::I18n::reg("cl_components","コンポーネント",              "Components");
    ofd::I18n::reg("cl_drag_hint", "ドラッグでビューポートへ配置",
                   "Drag into the viewport to place");
    ofd::I18n::reg("cl_favorites", "お気に入り",                  "Favorites");
    ofd::I18n::reg("cl_fav_hint",  "カードの ☆ で登録",           "Star a card to add it");
    ofd::I18n::reg("cl_recent",    "最近使用",                    "Recently used");
    return true;
}();

// コンポーネント一覧 — Ansys Lumerical FDTD が同梱するものに基づく (mock 同値)
struct Component { const char *cat, *icon, *name, *sub; };
const Component kComponents[] = {
    // Basic shapes
    { "basic",    "▭",  "Rectangle",            "直方体" },
    { "basic",    "⬭",  "Circle/Disk",          "円柱" },
    { "basic",    "○",  "Sphere",               "球" },
    { "basic",    "▱",  "Pyramid",              "四角錐" },
    { "basic",    "⏃",  "Triangle",             "三角形" },
    { "basic",    "⏆",  "Polygon",              "多角形" },
    { "basic",    "⌒",  "Spline",               "自由曲線" },
    // Photonics
    { "photonic", "▬",  "Waveguide (rib)",      "リブ導波路 Si/SiO₂" },
    { "photonic", "⌑",  "Ring resonator",       "リング共振器" },
    { "photonic", "≡",  "Bragg grating (DBR)",  "分布Bragg反射器" },
    { "photonic", "⫝̸",  "Y-branch splitter",    "光分波器" },
    { "photonic", "⋊",  "Directional coupler",  "方向性結合器" },
    { "photonic", "⨯",  "MMI splitter",         "多モード干渉計" },
    { "photonic", "◇",  "Photonic crystal",     "フォトニック結晶" },
    { "photonic", "◈",  "Grating coupler",      "格子結合器" },
    { "photonic", "▷◁", "MZI",                  "Mach-Zehnder干渉計" },
    { "photonic", "⊙",  "Quantum dot",          "量子ドット波源" },
    // Metal / plasmonics
    { "metal",    "◉",  "Nanoparticle (Au/Ag)", "プラズモニックNP" },
    { "metal",    "⫾",  "Nanorod",              "ナノロッド" },
    { "metal",    "◫",  "Nanowire grid",        "ワイヤグリッド偏光子" },
    { "metal",    "⊞",  "Bow-tie antenna",      "光アンテナ" },
    // Gratings / periodic
    { "grating",  "▦",  "1D Grating",           "1次元格子" },
    { "grating",  "⬚",  "2D Grating",           "2次元格子" },
    { "grating",  "⌗",  "Metasurface unit",     "メタサーフェス単位胞" },
    { "grating",  "⌖",  "Blazed grating",       "ブレーズド格子" },
    { "grating",  "⎈",  "Polarization grating", "偏光格子" },
    // Lens
    { "lens",     "◐",  "Plano-convex lens",    "平凸レンズ" },
    { "lens",     "◑",  "Biconvex lens",        "両凸レンズ" },
    { "lens",     "◖",  "Aspheric lens",        "非球面" },
    { "lens",     "⏥",  "Metalens",             "メタレンズ" },
    { "lens",     "⌧",  "GRIN lens",            "屈折率分布レンズ" },
    { "lens",     "╲",  "Mirror",               "反射鏡" },
    { "lens",     "⨀",  "Aperture / Stop",      "絞り" },
    // Antenna
    { "antenna",  "⊥",  "Dipole",               "ダイポール" },
    { "antenna",  "▥",  "Patch antenna",        "パッチアンテナ" },
    { "antenna",  "▽",  "Horn",                 "ホーンアンテナ" },
    { "antenna",  "⌬",  "Helix",                "ヘリカル" },
    { "antenna",  "⊟",  "Yagi-Uda",             "八木宇田" },
    { "antenna",  "▣",  "Array (8×8)",          "アレイアンテナ" },
    // Acoustic
    { "acoustic", "♫",  "Loudspeaker",          "スピーカー" },
    { "acoustic", "⌖",  "Microphone",           "マイクロホン" },
    { "acoustic", "▙",  "Absorber panel",       "吸音パネル" },
    { "acoustic", "⫽",  "Diffuser (QRD)",       "拡散体" },
    { "acoustic", "▓",  "Audience block",       "客席ブロック" },
    // Sources
    { "source",   "⚡", "Dipole source",        "電気/磁気/光双極子" },
    { "source",   "⫴",  "Mode source",          "モード波源" },
    { "source",   "⤓",  "Plane wave",           "平面波" },
    { "source",   "☼",  "Gaussian beam",        "ガウシアンビーム" },
    { "source",   "⌖",  "TFSF (全/散乱場)",     "TFSF波源" },
    { "source",   "⮃",  "Import source",        "スペクトル取込" },
    // Monitors
    { "monitor",  "⊙",  "Point monitor",        "点モニター" },
    { "monitor",  "━",  "Line monitor",         "線モニター" },
    { "monitor",  "▭",  "Plane monitor",        "面モニター" },
    { "monitor",  "▦",  "Volume monitor",       "体積モニター" },
    { "monitor",  "⊛",  "Mode expansion",       "モード展開モニター" },
    { "monitor",  "▶",  "Movie monitor",        "動画" },
    { "monitor",  "≡",  "Flux monitor",         "電力 (Poynting)" },
    { "monitor",  "⌛", "Time monitor",         "時間応答" },
    // Imported models — 取込モデル (GeometryTab の STL/OBJ 取込 と LayoutGDS に対応)
    { "imported", "⧉",  "Imported mesh",        "取込3Dモデル (STL/OBJ)" },
    { "imported", "▤",  "GDSII layout",         "レイアウト取込 (GDS)" },
};

// カテゴリボタン定義 ("all" + 9 カテゴリ)
struct Cat { const char *v, *labelKey, *icon; };
const Cat kCats[] = {
    { "all",      "cl_all",      "☰" },
    { "basic",    "cl_basic",    "▭" },
    { "photonic", "cl_photonic", "✦" },
    { "metal",    "cl_metal",    "◉" },
    { "grating",  "cl_grating",  "▦" },
    { "lens",     "cl_lens",     "◐" },
    { "antenna",  "cl_antenna",  "⊥" },
    { "acoustic", "cl_acoustic", "♫" },
    { "source",   "cl_source",   "⚡" },
    { "monitor",  "cl_monitor",  "⊙" },
    { "imported", "cl_imported", "⧉" },
};

// ドメインに関係するカテゴリのみ表示 (それ以外は非表示)
// 取込モデル (imported) は形状取込なのでどのドメインでも意味を持つ。
QStringList allowedCats(const QString &d)
{
    if (d == "em")
        return { "basic", "antenna", "metal", "source", "monitor", "imported" };
    if (d == "optical")
        return { "basic", "photonic", "grating", "lens", "metal", "source",
                 "monitor", "imported" };
    if (d == "acoustic" || d == "underwater")
        return { "basic", "acoustic", "source", "monitor", "imported" };
    return { "basic", "source", "monitor", "imported" };
}

// 許可カテゴリ内での優先順 (グリッドの並び)
QStringList priorityCats(const QString &d)
{
    if (d == "em")
        return { "antenna", "source", "monitor", "basic", "metal" };
    if (d == "optical")
        return { "photonic", "grating", "lens", "metal", "source", "monitor", "basic" };
    if (d == "acoustic" || d == "underwater")
        return { "acoustic", "source", "monitor", "basic" };
    return { "basic" };
}

void clearGrid(QGridLayout *g)
{
    while (QLayoutItem *it = g->takeAt(0)) {
        delete it->widget();
        delete it;
    }
}
} // namespace

// ── ComponentsTab ───────────────────────────────────────────────────────────
ComponentsTab::ComponentsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 検索 + カテゴリフィルタ
    auto *sTop = new SectionBox(I18n::tr("cl_search"), body);
    m_search = new QLineEdit(sTop);
    m_search->setPlaceholderText(I18n::tr("cl_search_ph"));
    sTop->vbox()->addWidget(m_search);
    m_catGrid = new QGridLayout();
    m_catGrid->setSpacing(3);
    sTop->vbox()->addLayout(m_catGrid);
    v->addWidget(sTop);

    // コンポーネントグリッド
    m_gridSection = new SectionBox(I18n::tr("cl_components"), body);
    auto *hint = new QLabel(QStringLiteral("▸ ") + I18n::tr("cl_drag_hint"),
                            m_gridSection);
    hint->setWordWrap(true);
    m_gridSection->vbox()->addWidget(hint);
    m_grid = new QGridLayout();
    m_grid->setSpacing(6);
    m_gridSection->vbox()->addLayout(m_grid);
    v->addWidget(m_gridSection);

    // お気に入り (カードの ☆ で登録/解除 — mock の cl_favorites)
    m_favSection = new SectionBox(I18n::tr("cl_favorites"), body);
    m_favRow = new QHBoxLayout();
    m_favSection->vbox()->addLayout(m_favRow);
    v->addWidget(m_favSection);

    // 最近使用
    auto *sRecent = new SectionBox(I18n::tr("cl_recent"), body);
    auto *recentRow = new QHBoxLayout();
    static const char *kRecent[] = {
        "パッチアンテナ", "リング共振器 R=5μm", "DBRミラー 25層" };
    for (const char *r : kRecent) {
        auto *b = new QLabel(QString::fromUtf8(r), sRecent);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("border:1px solid palette(mid); border-radius:3px;"
                         "padding:1px 6px; font-size:11px;");
        recentRow->addWidget(b);
    }
    recentRow->addStretch(1);
    sRecent->vbox()->addLayout(recentRow);
    v->addWidget(sRecent);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_search, &QLineEdit::textChanged,
            this, &ComponentsTab::rebuildGrid);
    connect(project, &Project::domainChanged,
            this, &ComponentsTab::rebuildCats);
    connect(project, &Project::loaded,
            this, &ComponentsTab::rebuildCats);

    rebuildFavorites();
    rebuildCats();
}

// お気に入りチップ行 (空なら登録方法のヒントを出す)
void ComponentsTab::rebuildFavorites()
{
    while (QLayoutItem *it = m_favRow->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    if (m_favorites.isEmpty()) {
        auto *hint = new QLabel(I18n::tr("cl_fav_hint"), m_favSection);
        hint->setStyleSheet("font-size:11px; color:gray;");
        m_favRow->addWidget(hint);
    } else {
        for (const QString &name : m_favorites) {
            auto *chip = new QLabel(QStringLiteral("★ ") + name, m_favSection);
            chip->setCursor(Qt::PointingHandCursor);
            chip->setStyleSheet("border:1px solid palette(mid); border-radius:3px;"
                                "padding:1px 6px; font-size:11px;");
            m_favRow->addWidget(chip);
        }
    }
    m_favRow->addStretch(1);
}

// ドメインが変わった → 表示カテゴリボタンを作り直し、選択をリセット
void ComponentsTab::rebuildCats()
{
    m_catButtons.clear();
    clearGrid(m_catGrid);

    const QString domain = domainKey(m_p->activeDomain());
    const QStringList allowed = allowedCats(domain);
    if (m_cat != "all" && !allowed.contains(m_cat))
        m_cat = "all";

    int i = 0;
    for (const Cat &c : kCats) {
        if (QString(c.v) != "all" && !allowed.contains(c.v)) continue;
        auto *btn = new QPushButton(
            QString::fromUtf8(c.icon) + " " + I18n::tr(c.labelKey), m_gridSection);
        btn->setCheckable(true);
        btn->setChecked(m_cat == c.v);
        btn->setStyleSheet("font-size:11px; padding:1px 6px;");
        const QString val = c.v;
        connect(btn, &QPushButton::clicked, this, [this, val] {
            m_cat = val;
            for (QPushButton *b : m_catButtons)
                b->setChecked(b->property("cat").toString() == m_cat);
            rebuildGrid();
        });
        btn->setProperty("cat", val);
        m_catGrid->addWidget(btn, i / 5, i % 5);
        m_catButtons.push_back(btn);
        ++i;
    }
    rebuildGrid();
}

// 検索/カテゴリ/ドメインの現状態でコンポーネントカードを並べ直す
void ComponentsTab::rebuildGrid()
{
    clearGrid(m_grid);

    const QString domain = domainKey(m_p->activeDomain());
    const QStringList allowed = allowedCats(domain);
    const QStringList priority = priorityCats(domain);
    const QString q = m_search->text().trimmed();

    QVector<const Component *> filtered;
    for (const Component &c : kComponents) {
        if (!allowed.contains(c.cat)) continue;
        if (m_cat != "all" && m_cat != c.cat) continue;
        if (!q.isEmpty()
            && !QString::fromUtf8(c.name).contains(q, Qt::CaseInsensitive)
            && !QString::fromUtf8(c.sub).contains(q))
            continue;
        filtered.push_back(&c);
    }
    std::stable_sort(filtered.begin(), filtered.end(),
        [&priority](const Component *a, const Component *b) {
            int ia = priority.indexOf(a->cat);
            int ib = priority.indexOf(b->cat);
            if (ia < 0) ia = 99;
            if (ib < 0) ib = 99;
            return ia < ib;
        });

    m_gridSection->setTitle(QStringLiteral("%1 (%2)")
        .arg(I18n::tr("cl_components")).arg(filtered.size()));

    const QString acc = accentColor(m_p->activeDomain());
    const int cols = 4;
    for (int i = 0; i < filtered.size(); ++i) {
        const Component &c = *filtered[i];
        auto *card = new QFrame(m_gridSection);
        card->setFrameShape(QFrame::StyledPanel);
        card->setCursor(Qt::OpenHandCursor);
        auto *cv = new QVBoxLayout(card);
        cv->setContentsMargins(8, 6, 8, 6);
        cv->setSpacing(2);
        auto *top = new QHBoxLayout();
        top->setSpacing(6);
        auto *ic = new QLabel(QString::fromUtf8(c.icon), card);
        ic->setStyleSheet(QStringLiteral("font-size:16px; color:%1;").arg(acc));
        const QString name = QString::fromUtf8(c.name);
        auto *nm = new QLabel(name, card);
        nm->setStyleSheet("font-size:11px; font-weight:600;");
        top->addWidget(ic);
        top->addWidget(nm, 1);
        // ☆ / ★ = お気に入り登録トグル
        auto *fav = new QPushButton(m_favorites.contains(name)
                                        ? QStringLiteral("★") : QStringLiteral("☆"),
                                    card);
        fav->setFlat(true);
        fav->setCursor(Qt::PointingHandCursor);
        fav->setToolTip(I18n::tr("cl_favorites"));
        fav->setFixedWidth(20);
        fav->setStyleSheet("border:none; font-size:12px;");
        connect(fav, &QPushButton::clicked, this, [this, name, fav] {
            if (m_favorites.contains(name)) m_favorites.removeAll(name);
            else                            m_favorites.append(name);
            fav->setText(m_favorites.contains(name) ? QStringLiteral("★")
                                                    : QStringLiteral("☆"));
            rebuildFavorites();
        });
        top->addWidget(fav);
        cv->addLayout(top);
        auto *sub = new QLabel(QString::fromUtf8(c.sub), card);
        sub->setStyleSheet("font-size:10px; color:gray;");
        sub->setWordWrap(true);
        cv->addWidget(sub);
        m_grid->addWidget(card, i / cols, i % cols);
    }
}
