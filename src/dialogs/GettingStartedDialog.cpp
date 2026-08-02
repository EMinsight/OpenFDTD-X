// GettingStartedDialog.cpp
#include "GettingStartedDialog.h"
#include "../I18n.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

using namespace ofd;

// ── ダイアログ固有語彙 (gsd_) — file-local 登録 ─────────────────────────────
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("gsd_title",
        "🎓 はじめてのシミュレーション / My First Simulation (Lumerical風ガイド)",
        "🎓 My First Simulation (Lumerical-style guide)");
    ofd::I18n::reg("gsd_hint",
        "Ansys Lumerical FDTDの標準ワークフローを9ステップでガイドします。"
        "各ステップをクリックすると該当タブにジャンプ。",
        "A nine-step guide through the standard Ansys Lumerical FDTD workflow. "
        "Click a step to jump to the matching tab.");
    ofd::I18n::reg("gsd_close", "閉じる", "Close");
    ofd::I18n::reg("gsd_course", "📺 Ansys 公式コース", "📺 Official Ansys course");
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

const Step kSteps[] = {
    { 1, "テンプレートを選択",
      "アプリケーションギャラリーから類似プロジェクトを選んで開く。"
      "空白から始めるより推奨。",
      "ギャラリーを開く", "gallery" },
    { 2, "形状を配置",
      "「形状」タブ、またはコンポーネントライブラリから3Dオブジェクトをドラッグ。"
      "STL/GDSの取込も可能。",
      "形状タブへ", "geometry" },
    { 3, "物性値を割当",
      "「物性値」タブで誘電体・金属・分散材料を定義し、各形状に番号で割り当て。",
      "物性値タブへ", "material" },
    { 4, "ソルバ領域を定義",
      "「ソルバ領域」タブで計算範囲・メッシュ精度(1〜8)・境界条件(PML推奨)を設定。",
      "ソルバ領域タブへ", "solverregion" },
    { 5, "波源を配置",
      "ガウシアンパルス/平面波/モード源など、検査対象に応じて選択。",
      "波源タブへ", "source" },
    { 6, "モニターを配置",
      "結果取得点。Field/Mode/Flux/NTFFなど目的別に。"
      "位置は波源・物体から十分離す。",
      "モニタータブへ", "monitors" },
    { 7, "計算実行",
      "ツールバーの「▶ 計算」ボタン。停止するとカーネルを終了させるため、"
      "その時点までの出力が残るかはカーネル側の実装に依存します。",
      "▶ 実行", "run" },
    { 8, "結果を確認",
      "「Datasets」または「ポスト処理」で2D/3Dプロット。"
      "Touchstone/HDF5でエクスポート。",
      "Datasetsへ", "datasets" },
    { 9, "精度を検証",
      "「検証」タブ (エキスパート表示) にメッシュ収束・PML反射・時間精度の"
      "確認画面があります。自動チェック機能は未実装で、表示中の数値は"
      "サンプルです。",
      "検証タブへ", "verification" }
};

const char kCourseUrl[] =
    "https://innovationspace.ansys.com/courses/courses/"
    "lumerical-fdtd-my-first-simulation/";
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
    auto *course = new QPushButton(I18n::tr("gsd_course"), foot);
    auto *fromTpl = new QPushButton(I18n::tr("gsd_from_template"), foot);
    fromTpl->setDefault(true);
    fh->addWidget(close);
    fh->addWidget(course);
    fh->addWidget(fromTpl);
    v->addWidget(foot);

    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(course, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kCourseUrl)));
    });
    connect(fromTpl, &QPushButton::clicked, this, [this] {
        emit jumpTo(QStringLiteral("gallery"));
        accept();
    });
}
