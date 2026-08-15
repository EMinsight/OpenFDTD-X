// RefractiveIndexDialog.cpp — n,k の取り込み (RefractiveIndexDialog.h 参照)
#include "RefractiveIndexDialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "../I18n.h"

#ifdef OFD_USE_NETWORK
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

using namespace ofd;

namespace {

// データベースの配布元 (公開リポジトリ)。ファイル冒頭に CC0 1.0 と明記されている。
// ブランチは **main** — 上流の README にある通り 2026-01-07 に master から
// 改名された。改名時点では master も同じ内容を返す (実測で catalog-nk.yml が
// バイト一致) が、いずれ消える側を指し続けると静かに壊れるので既定を指す。
const char *kBase =
    "https://raw.githubusercontent.com/polyanskiy/"
    "refractiveindex.info-database/main/database/";
const char *kSite = "https://refractiveindex.info/";

// 一覧に出す上限。カタログは 7000 件を超えるので全部並べると操作できない。
// 超えたときは画面にそう出す (下の ri_truncated)。
const int kListLimit = 3000;

void registerStrings()
{
    static bool done = false;
    if (done) return;
    done = true;
    I18n::reg("ri_title", "refractiveindex.info から取り込む",
              "Import from refractiveindex.info");
    I18n::reg("ri_intro",
              "公開の屈折率データベース (CC0 1.0 / パブリックドメイン) から "
              "n,k を取り込みます。押したときだけ通信します。通信先:",
              "Import n,k from the public refractive index database "
              "(CC0 1.0, public domain). It only connects when you press a "
              "button. It connects to:");
    I18n::reg("ri_fetch_catalog", "カタログを取得", "Fetch the catalog");
    I18n::reg("ri_filter", "材料名で絞り込み", "Filter by material");
    I18n::reg("ri_take", "選んだ材料を取り込む", "Import the selected material");
    I18n::reg("ri_busy", "%1 を取得しています…", "Fetching %1…");
    I18n::reg("ri_shown", "%2 件のうち %1 件を表示しています。",
              "Showing %1 of %2 entries.");
    I18n::reg("ri_cat_empty",
              "カタログを読めませんでした (項目が 0 件)。書式が変わった可能性があります。",
              "Could not read the catalog (0 entries). Its format may have "
              "changed.");
    I18n::reg("ri_cat_fail", "カタログを取得できませんでした: %1",
              "Could not fetch the catalog: %1");
    I18n::reg("ri_data_fail", "データを取得できませんでした: %1",
              "Could not fetch the data: %1");
    I18n::reg("ri_parse_fail", "読めませんでした: %1", "Could not read it: %1");
    I18n::reg("ri_unsupported",
              "この項目の式 (formula %1) は仕様が確認できず、対応していません。"
              "対応しているのは式 1〜9 (上流仕様書の全定義) と表形式です。",
              "This entry uses formula %1, whose definition is not available, "
              "so it is not supported. Supported: formulas 1-9 (all defined in "
              "the upstream specification) and tabulated data.");
    I18n::reg("ri_taken", "%1 点 (%2–%3 µm) を取り込みます。",
              "Importing %1 points (%2–%3 µm).");
    I18n::reg("ri_nonet",
              "この構成では Qt6::Network が無いため直接取得できません。"
              "配布ページを開いて手元に保存し、「📁 n,k 取込」で読み込んでください。",
              "This build has no Qt6::Network, so it cannot fetch directly. "
              "Open the distribution page, save the file, and load it with the "
              "n,k import button.");
    I18n::reg("ri_open_site", "配布ページを開く", "Open the distribution page");
    I18n::reg("ri_truncated",
              "一覧は %1 件で打ち切りました。絞り込むと残りも探せます。",
              "The list is truncated at %1 entries. Use the filter to reach "
              "the rest.");
    I18n::reg("ri_detail", "取得先: %1", "Will fetch: %1");
    I18n::reg("ri_source", "出典: %1", "Source: %1");
    I18n::reg("ri_pick", "先に一覧から材料を選んでください。",
              "Select a material from the list first.");
}

} // namespace

namespace ofd {

#ifdef OFD_USE_NETWORK
class RefractiveIndexDialog::Impl : public QObject {
public:
    QNetworkAccessManager nam;
    QNetworkReply *reply = nullptr;
};
#else
class RefractiveIndexDialog::Impl : public QObject {};
#endif

RefractiveIndexDialog::RefractiveIndexDialog(QWidget *parent)
    : QDialog(parent), m_impl(new Impl)
{
    registerStrings();
    setWindowTitle(I18n::tr("ri_title"));
    resize(720, 560);

    auto *v = new QVBoxLayout(this);

    auto *intro = new QLabel(I18n::tr("ri_intro"), this);
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#555; font-size:11px;");
    v->addWidget(intro);

    // 通信先をそのまま出す (どこへ繋ぐかを隠さない)
    auto *host = new QLabel(QString::fromLatin1(kBase), this);
    host->setWordWrap(true);
    host->setTextInteractionFlags(Qt::TextSelectableByMouse);
    host->setStyleSheet("color:#333; font-size:10px; font-family:monospace;");
    v->addWidget(host);

    m_fetchBtn = new QPushButton(I18n::tr("ri_fetch_catalog"), this);
    connect(m_fetchBtn, &QPushButton::clicked,
            this, &RefractiveIndexDialog::fetchCatalog);
    v->addWidget(m_fetchBtn);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(I18n::tr("ri_filter"));
    connect(m_filter, &QLineEdit::textChanged,
            this, &RefractiveIndexDialog::applyFilter);
    v->addWidget(m_filter);

    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::currentRowChanged,
            this, &RefractiveIndexDialog::showSelected);
    v->addWidget(m_list, 1);

    m_detail = new QLabel(this);
    m_detail->setWordWrap(true);
    m_detail->setStyleSheet("color:#555; font-size:11px;");
    v->addWidget(m_detail);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("font-size:11px;");
    v->addWidget(m_status);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_takeBtn = bb->addButton(I18n::tr("ri_take"), QDialogButtonBox::AcceptRole);
    connect(m_takeBtn, &QPushButton::clicked,
            this, &RefractiveIndexDialog::fetchSelected);
    auto *site = bb->addButton(I18n::tr("ri_open_site"),
                               QDialogButtonBox::ActionRole);
    connect(site, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kSite)));
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);

#ifndef OFD_USE_NETWORK
    m_fetchBtn->setEnabled(false);
    m_takeBtn->setEnabled(false);
    m_filter->setEnabled(false);
    m_status->setText(I18n::tr("ri_nonet"));
#endif
}

RefractiveIndexDialog::~RefractiveIndexDialog() { delete m_impl; }

void RefractiveIndexDialog::setBusy(bool on, const QString &what)
{
    if (m_fetchBtn) m_fetchBtn->setEnabled(!on);
    if (m_takeBtn)  m_takeBtn->setEnabled(!on);
    if (on) m_status->setText(I18n::tr("ri_busy").arg(what));
}

void RefractiveIndexDialog::applyFilter()
{
    const QString needle = m_filter ? m_filter->text().trimmed() : QString();
    m_list->clear();
    m_shown.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        const QString label = m_entries[i].label();
        if (!needle.isEmpty() &&
            !label.contains(needle, Qt::CaseInsensitive)) continue;
        m_shown.push_back(i);
        m_list->addItem(label);
        if (m_shown.size() >= kListLimit) break;
    }
    if (m_entries.isEmpty() || !m_status) return;

    // 件数と「打ち切った」を**ここで一緒に**出す。呼び出し側が別に status を
    // 書くと打ち切りの断りが消えるので (実際に一度そうなった)、状態表示は
    // この関数に集約する。
    QString msg = I18n::tr("ri_shown").arg(m_shown.size()).arg(m_entries.size());
    if (m_shown.size() >= kListLimit)
        msg += " " + I18n::tr("ri_truncated").arg(kListLimit);
    m_status->setStyleSheet("font-size:11px; color:#555;");
    m_status->setText(msg);
}

// 選択した項目について「どのファイルを取りに行くか」を出す。
// 通信先を隠さない方針の続きで、押す前に対象が分かるようにする。
void RefractiveIndexDialog::showSelected()
{
    if (!m_detail) return;
    const int row = m_list ? m_list->currentRow() : -1;
    if (row < 0 || row >= m_shown.size()) { m_detail->clear(); return; }
    m_detail->setText(I18n::tr("ri_detail")
                          .arg("data/" + m_entries[m_shown[row]].dataPath));
}

#ifdef OFD_USE_NETWORK

void RefractiveIndexDialog::fetchCatalog()
{
    setBusy(true, QStringLiteral("catalog-nk.yml"));
    QNetworkRequest req{ QUrl(QString::fromLatin1(kBase) + "catalog-nk.yml") };
    QNetworkReply *r = m_impl->nam.get(req);
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        setBusy(false, QString());
        if (r->error() != QNetworkReply::NoError) {
            m_status->setText(I18n::tr("ri_cat_fail").arg(r->errorString()));
            m_status->setStyleSheet("font-size:11px; color:#C0392B;");
            return;
        }
        m_entries = parseRiCatalog(r->readAll());
        applyFilter();                    // 件数・打ち切りの表示はこの中
        if (m_entries.isEmpty()) {
            m_status->setStyleSheet("font-size:11px; color:#C0392B;");
            m_status->setText(I18n::tr("ri_cat_empty"));
        }
    });
}

void RefractiveIndexDialog::fetchSelected()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_shown.size()) {
        m_status->setStyleSheet("font-size:11px; color:#C0392B;");
        m_status->setText(I18n::tr("ri_pick"));
        return;
    }
    const RiEntry e = m_entries[m_shown[row]];
    setBusy(true, e.label());

    QNetworkRequest req{ QUrl(QString::fromLatin1(kBase) + "data/" + e.dataPath) };
    QNetworkReply *r = m_impl->nam.get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, e] {
        r->deleteLater();
        setBusy(false, QString());
        if (r->error() != QNetworkReply::NoError) {
            m_status->setStyleSheet("font-size:11px; color:#C0392B;");
            m_status->setText(I18n::tr("ri_data_fail").arg(r->errorString()));
            return;
        }
        const RiData d = parseRiData(r->readAll());
        if (!d.ok) {
            m_status->setStyleSheet("font-size:11px; color:#C0392B;");
            // 式が未対応なら「未対応」とはっきり言う (黙って近似しない)
            if (d.hasFormula() && !riFormulaSupported(d.formula))
                m_status->setText(I18n::tr("ri_unsupported").arg(d.formula));
            else
                m_status->setText(I18n::tr("ri_parse_fail").arg(d.error));
            return;
        }
        const NkTable t = riToNkTable(d);
        if (!t.ok) {
            m_status->setStyleSheet("font-size:11px; color:#C0392B;");
            m_status->setText(I18n::tr("ri_parse_fail").arg(t.error));
            return;
        }
        m_table = t;
        m_name  = e.label();
        m_reference = riPlainText(d.reference);
        m_status->setStyleSheet("font-size:11px; color:#555;");
        m_status->setText(I18n::tr("ri_taken")
                              .arg(t.rows)
                              .arg(t.minLambda_um(), 0, 'g', 4)
                              .arg(t.maxLambda_um(), 0, 'g', 4));
        accept();
    });
}

#else   // Qt6::Network 無し

void RefractiveIndexDialog::fetchCatalog()  { m_status->setText(I18n::tr("ri_nonet")); }
void RefractiveIndexDialog::fetchSelected() { m_status->setText(I18n::tr("ri_nonet")); }

#endif

} // namespace ofd
