// ComponentsTab.cpp
#include "ComponentsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../widgets/Viewport3D.h"   // ComponentDrop (3D ビューへの D&D 契約)
#include "../I18n.h"

#include <QApplication>
#include <QDrag>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <utility>

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
    ofd::I18n::reg("cl_drag_hint",
        "カードを中央の「🧊 3D シーン」へドラッグすると、その位置に "
        "形状 / 波源 / 観測点を追加します (床面との交点に配置)",
        "Drag a card onto the centre \"🧊 3D scene\" to add a shape / source "
        "/ observation point there (placed where the view hits the floor)");
    ofd::I18n::reg("cl_drag_tip",
        "3D シーンへドラッグして配置",
        "Drag onto the 3D scene to place it");
    ofd::I18n::reg("cl_favorites", "お気に入り",                  "Favorites");
    ofd::I18n::reg("cl_fav_hint",  "カードの ☆ で登録",           "Star a card to add it");
    ofd::I18n::reg("cl_recent",    "最近使用",                    "Recently used");
    // 最近使用 = 実際に 3D シーンへ配置したコンポーネントの履歴 (QSettings)
    ofd::I18n::reg("cl_rec_hint",
                   "3D シーンへ配置すると、ここに新しい順で残ります",
                   "Components you drop onto the 3D scene appear here, newest "
                   "first");
    ofd::I18n::reg("cl_rec_clear", "履歴を消去", "Clear history");
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
    if (d == "acoustic")
        return { "basic", "acoustic", "source", "monitor", "imported" };
    if (d == "underwater")
        // 音響カテゴリの部材 (吸音パネル/客席ブロック等) は室内音響用で、
        // 水中音響では意味を持たないため表示しない
        return { "basic", "source", "monitor", "imported" };
    return { "basic", "source", "monitor", "imported" };
}

// 許可カテゴリ内での優先順 (グリッドの並び)
QStringList priorityCats(const QString &d)
{
    if (d == "em")
        return { "antenna", "source", "monitor", "basic", "metal" };
    if (d == "optical")
        return { "photonic", "grating", "lens", "metal", "source", "monitor", "basic" };
    if (d == "acoustic")
        return { "acoustic", "source", "monitor", "basic" };
    if (d == "underwater")
        return { "source", "monitor", "basic" };
    return { "basic" };
}

// 名前からコンポーネント定義を引く (お気に入りチップはカテゴリを持たない)
// 名前には非 ASCII を含むもの ("TFSF (全/散乱場)" 等) があるので UTF-8 で比較する
const Component *findComponent(const QString &name)
{
    for (const Component &c : kComponents)
        if (name == QString::fromUtf8(c.name)) return &c;
    return nullptr;
}

// ── 3D ビューへのドラッグ元 ────────────────────────────────────────────────
// 押下位置からしきい値以上動いたら QDrag を開始する。運ぶのは
// ComponentDrop の MIME (カテゴリ + 名前) だけで、実際に何を作るかは
// ドロップ先 (Viewport3D) が決める。
// 戻り値 = ドロップ先が受理した (= 実際に配置された) か。履歴の記録に使う
// ので、掴んだだけ・取り消した場合は false を返す。
bool beginComponentDrag(QWidget *src, const QString &cat, const QString &name,
                        const QPoint &hotSpot)
{
    if (cat.isEmpty() || name.isEmpty()) return false;
    // ドロップしても配置できないものは掴めないようにする (空振りを作らない)
    if (!ofd::ComponentDrop::canPlace(cat, name)) return false;

    auto *mime = new QMimeData();
    mime->setData(ofd::ComponentDrop::mimeType(),
                  ofd::ComponentDrop::encode(cat, name));
    mime->setText(name);                       // 他アプリへは名前だけ渡る
    auto *drag = new QDrag(src);
    drag->setMimeData(mime);
    drag->setPixmap(src->grab());              // カードの見た目をカーソルに
    drag->setHotSpot(hotSpot);
    // Viewport3D::dropEvent が acceptProposedAction() したときだけ CopyAction
    return drag->exec(Qt::CopyAction) == Qt::CopyAction;
}

// ドラッグ元になる小さな入れ物 (カード = QFrame / チップ = QLabel)。
// 子ラベルは WA_TransparentForMouseEvents にしてあるので押下はここへ届く。
// onPlaced は「配置された」ときだけ呼ばれる (最近使用履歴の記録用)。
template <class Base>
class DragSource : public Base {
public:
    using PlacedCb = std::function<void(const QString &name)>;
    DragSource(const QString &cat, const QString &name, QWidget *parent,
               PlacedCb onPlaced = PlacedCb())
        : Base(parent), m_cat(cat), m_name(name), m_placed(std::move(onPlaced)) {}

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_press = e->position().toPoint();
            e->accept();                        // 以降の move をここで受ける
            return;
        }
        Base::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!(e->buttons() & Qt::LeftButton)) { Base::mouseMoveEvent(e); return; }
        if ((e->position().toPoint() - m_press).manhattanLength()
            < QApplication::startDragDistance())
            return;
        // ドラッグ中 (ネストしたイベントループ) と配置後のコールバックで
        // this が消え得るので、必要な値は先にコピーしておく
        const QString cat = m_cat, name = m_name;
        PlacedCb cb = m_placed;
        if (beginComponentDrag(this, cat, name, m_press) && cb)
            cb(name);
    }

private:
    QString  m_cat, m_name;
    QPoint   m_press;
    PlacedCb m_placed;
};

using DragCard = DragSource<QFrame>;
using DragChip = DragSource<QLabel>;

// ドラッグ元ウィジェットの見た目 (カーソル) とツールチップ。
// 配置できないコンポーネントは掴める見た目にせず、理由をツールチップに出す。
void applyDragAffordance(QWidget *w, const QString &cat, const QString &name)
{
    QString why;
    if (cat.isEmpty() || name.isEmpty()) {   // 対応表に無い項目 (ドラッグ不可)
        w->setCursor(Qt::ArrowCursor);
        return;
    }
    if (ofd::ComponentDrop::canPlace(cat, name, &why)) {
        w->setCursor(Qt::OpenHandCursor);
        w->setToolTip(ofd::I18n::tr("cl_drag_tip"));
    } else {
        w->setCursor(Qt::ArrowCursor);
        w->setToolTip(why);
    }
}

// お気に入り / 最近使用の QSettings 永続化キー (アプリ再起動をまたいで保持)
const char kFavSettingsKey[] = "components/favorites";
const char kRecentSettingsKey[] = "components/recent";
const int  kRecentMax = 8;      // チップ行に収まる件数で打ち切る

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

    // 最近使用 — 実際に 3D シーンへ配置したコンポーネントの履歴 (QSettings)
    m_recentSection = new SectionBox(I18n::tr("cl_recent"), body);
    m_recentRow = new QHBoxLayout();
    m_recentSection->vbox()->addLayout(m_recentRow);
    auto *recClear = new QPushButton(I18n::tr("cl_rec_clear"), m_recentSection);
    recClear->setStyleSheet("font-size:11px; padding:1px 6px;");
    auto *recBtnRow = new QHBoxLayout();
    recBtnRow->addWidget(recClear);
    recBtnRow->addStretch(1);
    m_recentSection->vbox()->addLayout(recBtnRow);
    connect(recClear, &QPushButton::clicked, this, [this] {
        m_recent.clear();
        QSettings().setValue(QString::fromLatin1(kRecentSettingsKey), m_recent);
        rebuildRecent();
    });
    v->addWidget(m_recentSection);

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

    // お気に入り・最近使用は QSettings に永続化する
    // (以前は再起動で消える揮発状態 / 固定サンプルだった)
    m_favorites = QSettings().value(QString::fromLatin1(kFavSettingsKey))
                      .toStringList();
    m_recent = QSettings().value(QString::fromLatin1(kRecentSettingsKey))
                   .toStringList();

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
            // カードと同じくドラッグ元になる (カテゴリは名前から引く)
            const Component *c = findComponent(name);
            const QString cat = c ? QString::fromUtf8(c->cat) : QString();
            auto *chip = new DragChip(cat, name, m_favSection,
                                      [this](const QString &n) { recordRecent(n); });
            chip->setText(QStringLiteral("★ ") + name);
            chip->setStyleSheet("border:1px solid palette(mid); border-radius:3px;"
                                "padding:1px 6px; font-size:11px;");
            applyDragAffordance(chip, cat, name);
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
    rebuildRecent();
}

// 実際に 3D シーンへ配置されたコンポーネントを履歴の先頭へ入れる。
// (Viewport3D がドロップを受理したときだけ呼ばれる — DragSource の onPlaced)
void ComponentsTab::recordRecent(const QString &name)
{
    m_recent.removeAll(name);            // 同じものは 1 件に畳んで先頭へ
    m_recent.prepend(name);
    while (m_recent.size() > kRecentMax) m_recent.removeLast();
    QSettings().setValue(QString::fromLatin1(kRecentSettingsKey), m_recent);
    rebuildRecent();
}

// 最近使用チップ行の再構築 — 履歴 (QSettings) のうち現ドメインで配置できる
// ものだけを新しい順に並べる。履歴が無ければ作り方のヒントを出す。
void ComponentsTab::rebuildRecent()
{
    // チップ自身のドラッグ完了から呼ばれることがあるので即 delete しない
    // (自分のイベントハンドラの内側で消えるとぶら下がりポインタになる)
    while (QLayoutItem *it = m_recentRow->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete it;
    }
    const QStringList allowed = allowedCats(domainKey(m_p->activeDomain()));
    int shown = 0;
    for (const QString &name : m_recent) {
        const Component *c = findComponent(name);
        if (!c) continue;                                  // 定義が消えた項目
        const QString cat = QString::fromUtf8(c->cat);
        if (!allowed.contains(cat)) continue;              // 別ドメインの履歴
        auto *b = new DragChip(cat, name, m_recentSection,
                               [this](const QString &n) { recordRecent(n); });
        b->setText(QString::fromUtf8(c->icon) + " " + name);
        b->setStyleSheet("border:1px solid palette(mid); border-radius:3px;"
                         "padding:1px 6px; font-size:11px;");
        applyDragAffordance(b, cat, name);
        m_recentRow->addWidget(b);
        ++shown;
    }
    if (shown == 0) {
        auto *hint = new QLabel(I18n::tr("cl_rec_hint"), m_recentSection);
        hint->setStyleSheet("font-size:11px; color:gray;");
        m_recentRow->addWidget(hint);
    }
    m_recentRow->addStretch(1);
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
        const QString name = QString::fromUtf8(c.name);
        // 3D シーンへのドラッグ元。掴める見た目にするのは配置できるものだけ。
        // 配置に成功したら最近使用へ記録する
        auto *card = new DragCard(QString::fromUtf8(c.cat), name, m_gridSection,
                                  [this](const QString &n) { recordRecent(n); });
        card->setFrameShape(QFrame::StyledPanel);
        applyDragAffordance(card, QString::fromUtf8(c.cat), name);
        auto *cv = new QVBoxLayout(card);
        cv->setContentsMargins(8, 6, 8, 6);
        cv->setSpacing(2);
        auto *top = new QHBoxLayout();
        top->setSpacing(6);
        auto *ic = new QLabel(QString::fromUtf8(c.icon), card);
        ic->setStyleSheet(QStringLiteral("font-size:16px; color:%1;").arg(acc));
        auto *nm = new QLabel(name, card);
        nm->setStyleSheet("font-size:11px; font-weight:600;");
        // ラベルはマウスを素通りさせ、カード本体でドラッグを開始させる
        ic->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        nm->setAttribute(Qt::WA_TransparentForMouseEvents, true);
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
            // QSettings に保存 (再起動しても保持される)
            QSettings().setValue(QString::fromLatin1(kFavSettingsKey),
                                 m_favorites);
            fav->setText(m_favorites.contains(name) ? QStringLiteral("★")
                                                    : QStringLiteral("☆"));
            rebuildFavorites();
        });
        top->addWidget(fav);
        cv->addLayout(top);
        auto *sub = new QLabel(QString::fromUtf8(c.sub), card);
        sub->setStyleSheet("font-size:10px; color:gray;");
        sub->setWordWrap(true);
        sub->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        cv->addWidget(sub);
        m_grid->addWidget(card, i / cols, i % cols);
    }
}
