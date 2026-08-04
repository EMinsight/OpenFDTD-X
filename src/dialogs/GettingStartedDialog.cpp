// GettingStartedDialog.cpp
#include "GettingStartedDialog.h"
#include "../I18n.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

using namespace ofd;

// ── ダイアログ固有語彙 (gsd_) — file-local 登録 ─────────────────────────────
namespace {
const bool s_i18n = [] {
    // OpenFDTD-X 自身のワークフロー案内 (外部製品のチュートリアルではない)
    ofd::I18n::reg("gsd_title",
        "🎓 はじめてのシミュレーション",
        "🎓 My first simulation");
    ofd::I18n::reg("gsd_hint",
        "OpenFDTD-X の標準ワークフローを 9 ステップで案内します。"
        "各ステップのボタンで該当画面へ移動します。",
        "A nine-step guide through the standard OpenFDTD-X workflow. "
        "Each step's button takes you to the matching screen.");
    ofd::I18n::reg("gsd_close", "閉じる", "Close");
    ofd::I18n::reg("gsd_from_template", "▶ テンプレートから始める",
                   "▶ Start from a template");
    return true;
}();

// ── 9 ステップ (mock の steps 配列をそのまま転記) ───────────────────────────
struct Step {
    int         n;
    const char *title;
    const char *desc;
    const char *action;
    const char *jump;
};

// 記載は OpenFDTD-X の実装済み機能に合わせる (未実装の操作を案内しない)
const Step kSteps[] = {
    { 1, "テンプレートを選択",
      "アプリケーションギャラリーからテンプレートを選ぶと、シナリオに応じた"
      "メッシュ・物性値・形状・波源・周波数を投入した新規プロジェクトが"
      "作られます。空のプロジェクトから始めることも可能。",
      "ギャラリーを開く", "gallery" },
    { 2, "形状を配置",
      "「形状」タブで直方体・球・円柱などの形状を追加し、座標を入力。"
      "STL の取込にも対応 (取込は「形状」タブの CAD セクション)。",
      "形状タブへ", "geometry" },
    { 3, "物性値を割当",
      "「物性値」タブで誘電体・金属・分散材料を定義し、各形状に番号で割り当て。",
      "物性値タブへ", "material" },
    { 4, "ソルバ領域を確認",
      "PML 層数は「ソルバ領域」タブで設定 (計算範囲はメッシュタブの節点定義"
      "から決まる)。",
      "ソルバ領域タブへ", "solverregion" },
    { 5, "波源を配置",
      "「波源」タブで給電点や平面波など、検査対象に応じて選択。",
      "波源タブへ", "source" },
    { 6, "観測点を確認",
      "「ポスト処理」の周波数・観測面の設定と合わせて、結果の取得内容を決める。",
      "モニタータブへ", "monitors" },
    { 7, "計算実行",
      "ツールバーの「▶ 計算」ボタンでカーネル (ofd 等) を起動。"
      "カーネル未導入なら「ツール > カーネルパスの設定…」で場所を指定。",
      "▶ 実行", "run" },
    { 8, "結果を確認",
      "計算後、「2D 断面」ビューに HDF5 結果が自動反映され (USE_HDF5 ビルド)、"
      "「H5アニメ」タブで time_series_data.h5 を閲覧できます。",
      "Datasetsへ", "datasets" },
    { 9, "精度を確認",
      "「検証」タブはエキスパート表示にあります (ボタンで切替えて移動)。"
      "自動チェック機能は未実装で、表示中の数値はサンプルです。",
      "検証タブへ", "verification" }
};
} // namespace

GettingStartedDialog::GettingStartedDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::tr("gsd_title"));
    setModal(true);
    setMinimumWidth(720);
    resize(760, 640);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *hint = new QLabel(I18n::tr("gsd_hint"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:11px; color:palette(mid); padding:12px 16px 8px 16px;");
    v->addWidget(hint);

    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    v->addWidget(sep);

    // ── ステップカード (縦並び, スクロール可) ───────────────────────────────
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget(scroll);
    auto *bv = new QVBoxLayout(body);
    bv->setContentsMargins(16, 12, 16, 12);
    bv->setSpacing(8);

    for (const Step &s : kSteps) {
        auto *card = new QFrame(body);
        card->setObjectName("stepCard");
        card->setStyleSheet("#stepCard { background:palette(alternate-base);"
                            " border:1px solid palette(mid); border-radius:3px; }");
        auto *h = new QHBoxLayout(card);
        h->setContentsMargins(12, 10, 12, 10);
        h->setSpacing(12);

        // 番号バッジ (32px 円, アクセント地に白文字)
        auto *num = new QLabel(QString::number(s.n), card);
        num->setFixedSize(32, 32);
        num->setAlignment(Qt::AlignCenter);
        num->setStyleSheet("background:#0078D4; color:#fff; font-weight:700;"
                           " font-size:14px; border-radius:16px;");
        h->addWidget(num, 0, Qt::AlignVCenter);

        auto *textCol = new QVBoxLayout();
        textCol->setContentsMargins(0, 0, 0, 0);
        textCol->setSpacing(3);
        auto *ttl = new QLabel(QString::fromUtf8(s.title), card);
        ttl->setStyleSheet("font-size:12px; font-weight:600;");
        ttl->setWordWrap(true);
        textCol->addWidget(ttl);
        auto *desc = new QLabel(QString::fromUtf8(s.desc), card);
        desc->setStyleSheet("font-size:11px; color:palette(mid);");
        desc->setWordWrap(true);
        textCol->addWidget(desc);
        h->addLayout(textCol, 1);

        auto *go = new QPushButton(QString::fromUtf8(s.action), card);
        const QString target = QString::fromUtf8(s.jump);
        connect(go, &QPushButton::clicked, this, [this, target] {
            emit jumpTo(target);
            accept();
        });
        h->addWidget(go, 0, Qt::AlignVCenter);

        bv->addWidget(card);
    }
    bv->addStretch(1);
    scroll->setWidget(body);
    v->addWidget(scroll, 1);

    // ── フッタ ──────────────────────────────────────────────────────────────
    auto *foot = new QWidget(this);
    auto *fh = new QHBoxLayout(foot);
    fh->setContentsMargins(12, 8, 12, 8);
    fh->addStretch(1);
    auto *close = new QPushButton(I18n::tr("gsd_close"), foot);
    auto *fromTpl = new QPushButton(I18n::tr("gsd_from_template"), foot);
    fromTpl->setDefault(true);
    fh->addWidget(close);
    fh->addWidget(fromTpl);
    v->addWidget(foot);

    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(fromTpl, &QPushButton::clicked, this, [this] {
        emit jumpTo(QStringLiteral("gallery"));
        accept();
    });
}
