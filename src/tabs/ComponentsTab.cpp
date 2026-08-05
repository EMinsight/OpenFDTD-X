// ComponentsTab.cpp
#include "ComponentsTab.h"
#include "../core/ComponentCatalog.h" // 部品表とドメイン許可表 (Viewport3D と共有)
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
    // ドロップで何が .ofd に追加されるかの対応表 (音源設定の 2 系統の混乱対策:
    // スピーカーは音源リストではなくソルバ波源 feed になる)
    ofd::I18n::reg("cl_drop_map_hint",
        "配置されるもの: スピーカー = 点音源 (feed)、マイクロホン = 観測点、"
        "パネル/形状 = 物体 — いずれも .ofd に追加されます",
        "What gets placed: loudspeaker = point source (feed), microphone = "
        "observation point, panels/shapes = objects — all added to the .ofd");
    ofd::I18n::reg("cl_favorites", "お気に入り",                  "Favorites");
    ofd::I18n::reg("cl_fav_hint",  "カードの ☆ で登録",           "Star a card to add it");
    ofd::I18n::reg("cl_recent",    "最近使用",                    "Recently used");
    // 最近使用 = 実際に 3D シーンへ配置したコンポーネントの履歴 (QSettings)
    ofd::I18n::reg("cl_rec_hint",
                   "3D シーンへ配置すると、ここに新しい順で残ります",
                   "Components you drop onto the 3D scene appear here, newest "
                   "first");
    ofd::I18n::reg("cl_rec_clear", "履歴を消去", "Clear history");
    // 水中音響ドメイン: 配置部品が存在しない理由の説明 (グリッドの代わりに表示)
    ofd::I18n::reg("cl_uw_note",
        "水中音響 (BELLHOP) は海洋環境タブの SSP・海底・ソナー設定から"
        "入力を生成します。形状・波源・モニターの配置は計算に使われない"
        "ため、このドメインに配置部品はありません。",
        "Underwater acoustics (BELLHOP) builds its input from the SSP / "
        "seabed / sonar settings in the Ocean Environment tab. Placed "
        "shapes, sources and monitors are not used in the computation, so "
        "there are no placeable components in this domain.");
    return true;
}();

// コンポーネント一覧 — core/ComponentCatalog.h に移動した (部品→ドメイン
// 許可表を Viewport3D・selftest と共有するため。二重管理の防止)。
using Component = ofd::ComponentCatalog::Component;
using ofd::ComponentCatalog::kComponents;

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

// ドメインに関係するカテゴリのみ表示 (それ以外は非表示)。
// カテゴリ内の個々の部品はさらに ComponentCatalog::allowedInDomain の
// 許可表でフィルタされる (例: source は EM/光ドメインでも部品毎に絞られる)。
QStringList allowedCats(const QString &d)
{
    if (d == "em")
        // metal (プラズモニクス) は光専用なので EM には出さない
        return { "basic", "antenna", "source", "monitor", "imported" };
    if (d == "optical")
        return { "basic", "photonic", "grating", "lens", "metal", "source",
                 "monitor", "imported" };
    if (d == "acoustic")
        // 点音源は acoustic カテゴリの Loudspeaker が担う — EM/光専用の
        // source カテゴリは出さない (monitor は Point/Time 等が残る)
        return { "basic", "acoustic", "monitor", "imported" };
    if (d == "underwater")
        // BELLHOP は海洋環境タブの設定だけから入力を生成し、形状・波源・
        // モニターの配置を一切使わない — 配置部品は存在しない (説明を表示)
        return {};
    return { "basic", "monitor", "imported" };
}

// 許可カテゴリ内での優先順 (グリッドの並び)
QStringList priorityCats(const QString &d)
{
    if (d == "em")
        return { "antenna", "source", "monitor", "basic" };
    if (d == "optical")
        return { "photonic", "grating", "lens", "metal", "source", "monitor", "basic" };
    if (d == "acoustic")
        return { "acoustic", "monitor", "basic" };
    if (d == "underwater")
        return {};
    return { "basic" };
}

// 名前からコンポーネント定義を引く (お気に入りチップはカテゴリを持たない)
const Component *findComponent(const QString &name)
{
    return ofd::ComponentCatalog::findByName(name);
}

// ── 3D ビューへのドラッグ元 ────────────────────────────────────────────────
// 押下位置からしきい値以上動いたら QDrag を開始する。運ぶのは
// ComponentDrop の MIME (カテゴリ + 名前) だけで、実際に何を作るかは
// ドロップ先 (Viewport3D) が決める。
// 戻り値 = ドロップ先が受理した (= 実際に配置された) か。履歴の記録に使う
// ので、掴んだだけ・取り消した場合は false を返す。
bool beginComponentDrag(QWidget *src, const QString &cat, const QString &name,
                        const QString &domain, const QPoint &hotSpot)
{
    if (cat.isEmpty() || name.isEmpty()) return false;
    // ドロップしても配置できないものは掴めないようにする (空振りを作らない)。
    // ドメイン許可表込みの判定 (お気に入り経由のドラッグもここを通る)
    if (!ofd::ComponentDrop::canPlace(cat, name, domain)) return false;

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
    // domain はドラッグ開始時の判定に使う (ドメイン切替で親の行/グリッドが
    // 再構築されるので、構築時のドメインを保持すればよい)
    DragSource(const QString &cat, const QString &name, const QString &domain,
               QWidget *parent, PlacedCb onPlaced = PlacedCb())
        : Base(parent), m_cat(cat), m_name(name), m_domain(domain),
          m_placed(std::move(onPlaced)) {}

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
        const QString cat = m_cat, name = m_name, domain = m_domain;
        PlacedCb cb = m_placed;
        if (beginComponentDrag(this, cat, name, domain, m_press) && cb)
            cb(name);
    }

private:
    QString  m_cat, m_name, m_domain;
    QPoint   m_press;
    PlacedCb m_placed;
};

using DragCard = DragSource<QFrame>;
using DragChip = DragSource<QLabel>;

// ドラッグ元ウィジェットの見た目 (カーソル) とツールチップ。
// 配置できないコンポーネントは掴める見た目にせず、理由をツールチップに出す。
// 判定はドメイン許可表込み (beginComponentDrag と同じ)。
void applyDragAffordance(QWidget *w, const QString &cat, const QString &name,
                         const QString &domain)
{
    QString why;
    if (cat.isEmpty() || name.isEmpty()) {   // 対応表に無い項目 (ドラッグ不可)
        w->setCursor(Qt::ArrowCursor);
        return;
    }
    if (ofd::ComponentDrop::canPlace(cat, name, domain, &why)) {
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
    m_dragHint = new QLabel(QStringLiteral("▸ ") + I18n::tr("cl_drag_hint"),
                            m_gridSection);
    m_dragHint->setWordWrap(true);
    m_gridSection->vbox()->addWidget(m_dragHint);
    // ドロップで .ofd に何が追加されるかの対応 (スピーカー = 点音源 feed 等)
    m_mapHint = new QLabel(QStringLiteral("▸ ")
                               + I18n::tr("cl_drop_map_hint"),
                           m_gridSection);
    m_mapHint->setWordWrap(true);
    m_mapHint->setStyleSheet("font-size:11px; color:gray;");
    m_gridSection->vbox()->addWidget(m_mapHint);
    // 水中音響: 配置部品が無い理由 (グリッドの位置に説明を出す — 「(0)」の
    // 空グリッドより説明が前面に出るようにする)
    m_uwNote = new QLabel(I18n::tr("cl_uw_note"), m_gridSection);
    m_uwNote->setWordWrap(true);
    m_uwNote->setVisible(false);
    m_gridSection->vbox()->addWidget(m_uwNote);
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

// お気に入りチップ行 (空なら登録方法のヒントを出す)。
// 現ドメインで意味を持たない部品は非表示にする (登録自体は消さないので、
// ドメインを戻せば再び現れる)。
void ComponentsTab::rebuildFavorites()
{
    while (QLayoutItem *it = m_favRow->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    const QString domain = domainKey(m_p->activeDomain());
    int shown = 0;
    for (const QString &name : m_favorites) {
        // カードと同じくドラッグ元になる (カテゴリは名前から引く)
        const Component *c = findComponent(name);
        if (!c) continue;                                  // 定義が消えた項目
        if (!ComponentCatalog::allowedInDomain(name, domain))
            continue;                                      // 別ドメインの登録
        const QString cat = QString::fromUtf8(c->cat);
        auto *chip = new DragChip(cat, name, domain, m_favSection,
                                  [this](const QString &n) { recordRecent(n); });
        chip->setText(QStringLiteral("★ ") + name);
        chip->setStyleSheet("border:1px solid palette(mid); border-radius:3px;"
                            "padding:1px 6px; font-size:11px;");
        applyDragAffordance(chip, cat, name, domain);
        m_favRow->addWidget(chip);
        ++shown;
    }
    if (shown == 0) {
        auto *hint = new QLabel(I18n::tr("cl_fav_hint"), m_favSection);
        hint->setStyleSheet("font-size:11px; color:gray;");
        m_favRow->addWidget(hint);
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
        // 配置部品が無いドメイン (水中音響) はカテゴリボタン自体を出さない
        if (allowed.isEmpty()) break;
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
    // お気に入り・最近使用のチップもドメイン許可表で絞り直す
    // (ドメイン外のお気に入りは非表示にするだけで、登録は消さない)
    rebuildFavorites();
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
    const QString domain = domainKey(m_p->activeDomain());
    int shown = 0;
    for (const QString &name : m_recent) {
        const Component *c = findComponent(name);
        if (!c) continue;                                  // 定義が消えた項目
        // 別ドメインの履歴は非表示 (部品単位のドメイン許可表で判定)
        if (!ComponentCatalog::allowedInDomain(name, domain)) continue;
        const QString cat = QString::fromUtf8(c->cat);
        auto *b = new DragChip(cat, name, domain, m_recentSection,
                               [this](const QString &n) { recordRecent(n); });
        b->setText(QString::fromUtf8(c->icon) + " " + name);
        b->setStyleSheet("border:1px solid palette(mid); border-radius:3px;"
                         "padding:1px 6px; font-size:11px;");
        applyDragAffordance(b, cat, name, domain);
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

    // 水中音響: 配置部品が無い (BELLHOP は海洋環境タブの設定だけを使う) —
    // 空の「(0)」グリッドではなく理由の説明を前面に出す
    const bool uw = (domain == QLatin1String("underwater"));
    m_uwNote->setVisible(uw);
    m_dragHint->setVisible(!uw);
    m_mapHint->setVisible(!uw);
    if (uw) {
        m_gridSection->setTitle(I18n::tr("cl_components"));
        return;
    }

    const QStringList allowed = allowedCats(domain);
    const QStringList priority = priorityCats(domain);
    const QString q = m_search->text().trimmed();

    QVector<const Component *> filtered;
    for (const Component &c : kComponents) {
        if (!allowed.contains(c.cat)) continue;
        // カテゴリ許可に加えて部品単位のドメイン許可表でも絞る
        // (例: source カテゴリでも Mode source / Gaussian beam は光のみ)
        if (!ComponentCatalog::allowedInDomain(QString::fromUtf8(c.name),
                                               domain))
            continue;
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
        auto *card = new DragCard(QString::fromUtf8(c.cat), name, domain,
                                  m_gridSection,
                                  [this](const QString &n) { recordRecent(n); });
        card->setFrameShape(QFrame::StyledPanel);
        applyDragAffordance(card, QString::fromUtf8(c.cat), name, domain);
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
