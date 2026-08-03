// AppGalleryDialog.cpp
#include "AppGalleryDialog.h"
#include "../I18n.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

using namespace ofd;

// ── ダイアログ固有語彙 (gal_) — file-local 登録 ─────────────────────────────
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("gal_title",
        "応用ギャラリー / Application Gallery — FDTDで出来ること一覧",
        "Application Gallery — what FDTD can do");
    // テンプレートは現状ドメイン切替とタイトル設定のみ (物性値・メッシュ・
    // 波源のプリセットは未実装)。実態どおりに書く (絶対規則 5)。
    ofd::I18n::reg("gal_hint",
        "プロジェクトテンプレートを選択してください。"
        "現在はドメインの切替とタイトル設定のみを行います "
        "(物性値・メッシュ・波源のプリセットは未実装)。",
        "Pick a project template. For now this only switches the domain and "
        "sets the title (material / mesh / source presets are not "
        "implemented).");
    ofd::I18n::reg("gal_count", "— %1 テンプレート", "— %1 templates");
    ofd::I18n::reg("gal_cancel", "キャンセル", "Cancel");
    ofd::I18n::reg("gal_open_file", "📂 ファイルから開く…", "📂 Open from file…");
    ofd::I18n::reg("gal_blank", "空のプロジェクトで開始", "Start with an empty project");
    return true;
}();

// ── テンプレート定義 (mock の groups 配列をそのまま転記) ────────────────────
struct Item { const char *name; const char *sub; };
struct Group {
    const char *domain;
    const char *color;
    const char *title;
    const char *icon;
    const Item *items;
    int         count;
};

const Item kEmItems[] = {
    { "アンテナ放射パターン",      "ダイポール・パッチ・アレイ・ホーン" },
    { "EMC / EMI 適合性解析",     "シールド・キャビティ・PCB放射" },
    { "RCS (レーダー断面積)",     "ステルス機・船舶・地形" },
    { "マイクロ波回路",            "導波管・フィルタ・カプラ" },
    { "MRIコイル設計",             "B1場分布・SAR評価" },
    { "ワイヤレス給電 (WPT)",      "共鳴結合・効率最適化" },
    { "生体電磁波 / SAR",          "人体モデル・温度上昇" },
    { "5G/6G ミリ波解析",          "ビームフォーミング・基地局" }
};
const Item kOpticalItems[] = {
    { "BPF (バンドパスフィルタ)",  "DBR・FBG・薄膜多層" },
    { "リング共振器・MZI",         "Si Photonics モジュレータ" },
    { "フォトニック結晶 / 欠陥モード", "バンド構造・スローライト" },
    { "メタサーフェス・メタレンズ", "位相設計・偏向" },
    { "プラズモニクス",            "金属ナノ構造・SPR" },
    { "非線形光学 / 高調波生成",   "χ(2)/χ(3) 媒質" },
    { "太陽電池 / 薄膜",            "光吸収最適化・光閉じ込め" },
    { "LiDAR / イメージング",       "TOF・FMCW・コヒーレント検出" },
    { "Raycast 大規模光学系",      "カメラ・望遠鏡・レンズ系" },
    { "ハイブリッドFDTD+Ray",      "ナノ構造+大スケール伝搬" }
};
const Item kAcousticItems[] = {
    { "コンサートホール / オペラハウス", "RT60・C80・D50 評価" },
    { "オフィス・教室",            "STI 言葉の明瞭度" },
    { "スタジオ・コントロールルーム", "モーダル解析・吸音最適化" },
    { "屋外音響伝播",              "障壁・地形・気象影響" },
    { "オーラリゼーション",         "バイノーラル/Ambisonics再生" },
    { "幾何音響レイトレース",       "Odeon/CATT-Acoustic相当" },
    { "鏡像法 (Image-Source)",     "初期反射の高速計算" },
    { "騒音解析 / 防音設計",       "壁透過・床衝撃音" }
};
const Item kUnderwaterItems[] = {
    { "海洋音響伝搬 (SOFAR)",      "Bellhop型レイトレース" },
    { "ソナー指向性",              "TX/RX ビーム・アレイ" },
    { "海底地形マッピング",         "マルチビーム・サイドスキャン" },
    { "海洋生物検出",              "魚群・鯨類エコー" },
    { "潜水艦・水中ドローン通信",   "ADCP・モデムリンク" },
    { "津波・地震波結合",          "T-wave・地中音響" },
    { "PE法 (放物方程式)",         "RAM/RAMGeo相当" }
};
const Item kTidy3dItems[] = {
    { "大規模3D光学シミュレーション", "メタサーフェスアレイ・PIC" },
    { "パラメータスイープ",        "1000+ジョブ並列実行" },
    { "形状最適化 / 逆設計",        "adjoint / topology" },
    { "ML/AI連携",                 "tidy3d-AI · データセット生成" }
};

const Group kGroups[] = {
    { "em",         "#0078D4", "電磁波 / Electromagnetic",  "⚡",
      kEmItems,         int(sizeof(kEmItems)         / sizeof(Item)) },
    { "optical",    "#B83280", "光 / Optics & Photonics",   "✦",
      kOpticalItems,    int(sizeof(kOpticalItems)    / sizeof(Item)) },
    { "acoustic",   "#2E8B57", "室内音響 / Room Acoustics", "♪",
      kAcousticItems,   int(sizeof(kAcousticItems)   / sizeof(Item)) },
    { "underwater", "#1E6FBF", "水中音響 / Underwater",     "~",
      kUnderwaterItems, int(sizeof(kUnderwaterItems) / sizeof(Item)) },
    { "tidy3d",     "#7C3AED", "クラウド / tidy3d",         "☁",
      kTidy3dItems,     int(sizeof(kTidy3dItems)     / sizeof(Item)) }
};

const int kCols = 3;    // grid: repeat(auto-fill, minmax(220px, 1fr)) の Qt 版
} // namespace

// ── GalleryCard ─────────────────────────────────────────────────────────────
GalleryCard::GalleryCard(const QString &name, const QString &sub,
                         const QString &accent, QWidget *parent)
    : QFrame(parent), m_accent(accent)
{
    setObjectName("galleryCard");
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(220);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(2);
    auto *n = new QLabel(name, this);
    n->setStyleSheet("font-size:12px; font-weight:600;");
    n->setWordWrap(true);
    v->addWidget(n);
    auto *s = new QLabel(sub, this);
    s->setStyleSheet("font-size:11px; color:palette(mid);");
    s->setWordWrap(true);
    v->addWidget(s);
    v->addStretch(1);

    applyStyle(false);
}

void GalleryCard::applyStyle(bool hover)
{
    setStyleSheet(QStringLiteral(
        "#galleryCard { border:1px solid %1; border-radius:3px; background:%2; }")
        .arg(hover ? m_accent : QStringLiteral("palette(mid)"),
             hover ? QStringLiteral("palette(midlight)")
                   : QStringLiteral("palette(alternate-base)")));
}

void GalleryCard::enterEvent(QEnterEvent *e)
{
    applyStyle(true);
    QFrame::enterEvent(e);
}

void GalleryCard::leaveEvent(QEvent *e)
{
    applyStyle(false);
    QFrame::leaveEvent(e);
}

void GalleryCard::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()))
        emit clicked();
    QFrame::mouseReleaseEvent(e);
}

// ── AppGalleryDialog ────────────────────────────────────────────────────────
AppGalleryDialog::AppGalleryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::tr("gal_title"));
    setModal(true);
    setMinimumWidth(880);
    resize(920, 700);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ── 説明 ────────────────────────────────────────────────────────────────
    auto *hint = new QLabel(I18n::tr("gal_hint"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:12px; color:palette(mid); padding:12px 20px 0 20px;");
    v->addWidget(hint);

    // ── スクロール可能なグループ + カードグリッド ──────────────────────────
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget(scroll);
    auto *bv = new QVBoxLayout(body);
    bv->setContentsMargins(20, 16, 20, 16);
    bv->setSpacing(22);

    for (const Group &g : kGroups) {
        const QString accent = QString::fromUtf8(g.color);
        const QString domain = QString::fromUtf8(g.domain);

        auto *groupBox = new QWidget(body);
        auto *gv = new QVBoxLayout(groupBox);
        gv->setContentsMargins(0, 0, 0, 0);
        gv->setSpacing(10);

        // グループ見出し: アイコン角丸 + タイトル + テンプレート数
        auto *head = new QHBoxLayout();
        head->setSpacing(8);
        auto *ic = new QLabel(QString::fromUtf8(g.icon), groupBox);
        ic->setFixedSize(24, 24);
        ic->setAlignment(Qt::AlignCenter);
        ic->setStyleSheet(QStringLiteral(
            "background:%1; color:#fff; border-radius:4px; font-size:14px;")
            .arg(accent));
        head->addWidget(ic);
        auto *ttl = new QLabel(QString::fromUtf8(g.title), groupBox);
        ttl->setStyleSheet("font-size:14px; font-weight:600;");
        head->addWidget(ttl);
        auto *cnt = new QLabel(I18n::tr("gal_count").arg(g.count), groupBox);
        cnt->setStyleSheet("font-size:11px; color:palette(mid);");
        head->addWidget(cnt);
        head->addStretch(1);
        gv->addLayout(head);

        // 見出し下の 2px ドメイン色ライン (border-bottom 相当)
        auto *rule = new QFrame(groupBox);
        rule->setFixedHeight(2);
        rule->setStyleSheet(QStringLiteral("background:%1;").arg(accent));
        gv->addWidget(rule);

        auto *grid = new QGridLayout();
        grid->setSpacing(8);
        for (int i = 0; i < g.count; ++i) {
            const QString name = QString::fromUtf8(g.items[i].name);
            auto *card = new GalleryCard(name,
                                         QString::fromUtf8(g.items[i].sub),
                                         accent, groupBox);
            connect(card, &GalleryCard::clicked, this, [this, domain, name] {
                emit templatePicked(domain, name);
                accept();
            });
            grid->addWidget(card, i / kCols, i % kCols);
        }
        for (int c = 0; c < kCols; ++c)
            grid->setColumnStretch(c, 1);
        gv->addLayout(grid);

        bv->addWidget(groupBox);
    }
    bv->addStretch(1);
    scroll->setWidget(body);
    v->addWidget(scroll, 1);

    // ── フッタ ──────────────────────────────────────────────────────────────
    auto *foot = new QWidget(this);
    auto *h = new QHBoxLayout(foot);
    h->setContentsMargins(12, 8, 12, 8);
    h->addStretch(1);
    auto *cancel = new QPushButton(I18n::tr("gal_cancel"), foot);
    auto *openFile = new QPushButton(I18n::tr("gal_open_file"), foot);
    auto *blank = new QPushButton(I18n::tr("gal_blank"), foot);
    blank->setDefault(true);
    h->addWidget(cancel);
    h->addWidget(openFile);
    h->addWidget(blank);
    v->addWidget(foot);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    // 「ファイルから開く」「空のプロジェクト」は実動作をシグナルで通知する
    // (以前は閉じるだけで何も起きなかった)
    connect(openFile, &QPushButton::clicked, this, [this] {
        emit openFileRequested();
        accept();
    });
    connect(blank, &QPushButton::clicked, this, [this] {
        emit blankRequested();
        accept();
    });
}
