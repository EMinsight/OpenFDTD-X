// MeshTab.cpp
#include "MeshTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

using namespace ofd;

// ── タブ固有の翻訳キー (mst_) — file-local 登録 (既存 me_ は I18n.cpp) ───────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("mst_method", "入力方法", "Input method");
    I18n::reg("mst_with", "説明あり", "Annotated");
    I18n::reg("mst_without", "説明なし", "Plain");
    I18n::reg("mst_name", "名前", "Name");
    // 軸別セクション見出しと座標列の見出し (mock: msh_axis_* / msh_coord)
    I18n::reg("mst_axis_x", "X方向メッシュ", "X mesh");
    I18n::reg("mst_axis_y", "Y方向メッシュ", "Y mesh");
    I18n::reg("mst_axis_z", "Z方向メッシュ", "Z mesh");
    I18n::reg("mst_coord", "座標値", "Coord");
    I18n::reg("mst_lambda_check", "λ/n チェック", "λ/n check");
    I18n::reg("mst_section", "メッシュ統計", "Mesh Statistics");
    I18n::reg("mst_dx_min", "最小Δx", "min Δx");
    I18n::reg("mst_lambda_fmt", "→ λ/%1 @ %2", "→ λ/%1 @ %2");
    I18n::reg("mst_ok", "OK", "OK");
    I18n::reg("mst_coarse", "粗い", "Coarse");
    I18n::reg("mst_ng", "分割不足", "Too coarse");
    I18n::reg("mst_cfl", "CFL Δt", "CFL Δt");
    I18n::reg("mst_memory", "推定メモリ", "Est. memory");
    return true;
}();

// mock の CSS クラス相当 (最小限のスタイル)
const char kMono[]  = "font-family:'Consolas','Menlo',monospace;";
const char kMuted[] = "color:#888888;";

// mock の badge 色 (ok / warn / err)
const char kOk[]   = "#2E8B57";
const char kWarn[] = "#B45309";
const char kErr[]  = "#B91C1C";

QLabel *styledLabel(QWidget *parent, const char *css)
{
    auto *l = new QLabel(parent);
    l->setStyleSheet(QString::fromLatin1(css));
    return l;
}

// 2.5e9 → "2.5 GHz" (mock の "@ 2.5 GHz" 相当)
QString formatFreq(double f)
{
    if (f >= 1e9) return QStringLiteral("%1 GHz").arg(f / 1e9, 0, 'g', 3);
    if (f >= 1e6) return QStringLiteral("%1 MHz").arg(f / 1e6, 0, 'g', 3);
    if (f >= 1e3) return QStringLiteral("%1 kHz").arg(f / 1e3, 0, 'g', 3);
    return QStringLiteral("%1 Hz").arg(f, 0, 'g', 3);
}

// 5e-4 → "0.500e-3" (mock の "0.500e-3 m" と同じ仮数 [0.1,1) 表記)
QString formatSci(double v)
{
    if (!(v > 0) || v >= 1e308) return QStringLiteral("—");
    const int e = int(std::floor(std::log10(v))) + 1;
    const double m = v / std::pow(10.0, double(e));
    return QStringLiteral("%1e%2").arg(m, 0, 'f', 3).arg(e);
}

const double kC0 = 2.99792458e8;   // 真空中の光速 [m/s]

} // namespace

MeshTab::MeshTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    m_total = new QLabel(body);
    v->addWidget(m_total);

    // ── 入力方法 / Input method (mock の先頭セクション) ──────────────────────
    // 説明あり = 各メッシュ行に名前 (注記) 列を出す。λ/n チェックは統計セクション
    // の分割数バッジの表示に反映する。いずれもローカル状態。
    auto *im = new SectionBox(I18n::tr("mst_method"), body);
    auto *imRow = new QHBoxLayout();
    m_method = new QComboBox(im);
    m_method->addItem(I18n::tr("mst_with"));
    m_method->addItem(I18n::tr("mst_without"));
    m_method->setCurrentIndex(0);
    m_lambdaCheck = new QCheckBox(I18n::tr("mst_lambda_check"), im);
    m_lambdaCheck->setChecked(true);
    imRow->addWidget(m_method);
    imRow->addWidget(m_lambdaCheck);
    imRow->addStretch(1);
    im->vbox()->addLayout(imRow);
    v->addWidget(im);

    // 見出しは mock (tabs.jsx MeshTab) の msh_axis_* / msh_coord に合わせる
    static const char *secKey[3] = { "mst_axis_x", "mst_axis_y", "mst_axis_z" };
    for (int a = 0; a < 3; ++a) {
        auto *s = new SectionBox(I18n::tr(secKey[a]), body);

        m_table[a] = new QTableWidget(0, 3, s);
        m_table[a]->setHorizontalHeaderLabels(
            { I18n::tr("mst_coord"), I18n::tr("me_div"), I18n::tr("mst_name") });
        // 単位はモックの見出しに無いので tooltip で補う (値は [m])
        if (auto *h = m_table[a]->horizontalHeaderItem(0))
            h->setToolTip(I18n::tr("me_coord"));
        m_table[a]->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_table[a]->verticalHeader()->setDefaultSectionSize(22);
        m_table[a]->setMinimumHeight(110);
        s->vbox()->addWidget(m_table[a]);

        auto *btnRow = new QHBoxLayout();
        auto *add = new QPushButton(I18n::tr("me_add"), s);
        auto *del = new QPushButton(I18n::tr("me_del"), s);
        m_info[a] = new QLabel(s);
        btnRow->addWidget(add);
        btnRow->addWidget(del);
        btnRow->addStretch(1);
        btnRow->addWidget(m_info[a]);
        s->vbox()->addLayout(btnRow);
        v->addWidget(s);

        connect(add, &QPushButton::clicked, this, [this, a] {
            MeshAxis &ax = m_p->mesh(a);
            const double last = ax.nodes.isEmpty() ? 0.0 : ax.nodes.last();
            const double step = ax.nodes.size() >= 2
                ? ax.nodes.last() - ax.nodes[ax.nodes.size()-2] : 0.05;
            ax.nodes.push_back(last + (step > 0 ? step : 0.05));
            ax.divs.push_back(10);
            m_names[a].append(QString());
            refresh();
            m_p->touch();
        });
        connect(del, &QPushButton::clicked, this, [this, a] {
            MeshAxis &ax = m_p->mesh(a);
            if (ax.nodes.size() <= 2) return;
            const int row = m_table[a]->currentRow();
            const int i = (row >= 0 && row < ax.nodes.size())
                          ? row : ax.nodes.size() - 1;
            ax.nodes.removeAt(i);
            ax.divs.removeAt(qMin(i, ax.divs.size() - 1));
            if (i < m_names[a].size()) m_names[a].removeAt(i);
            refresh();
            m_p->touch();
        });
        connect(m_table[a], &QTableWidget::cellChanged, this,
                [this, a](int row, int col) {
            if (m_updating) return;
            if (col == 2) {   // 名前列はローカル注記のみ → モデルには書かない
                if (row >= 0 && row < m_names[a].size()) {
                    auto *it = m_table[a]->item(row, 2);
                    m_names[a][row] = it ? it->text() : QString();
                }
                return;
            }
            applyAxis(a);
            refreshAxisInfo(a);
            refreshStats();
            m_p->touch();
        });
    }

    // ── メッシュ統計 / Mesh Statistics (mock の最終セクション) ───────────────
    // 表示専用。値は Project::totalCells()/courantDt()/estimatedMemoryMB() から。
    auto *st = new SectionBox(I18n::tr("mst_section"), body);

    auto *cellsRow = new QHBoxLayout();
    m_statCells      = styledLabel(st, kMono);
    m_statCellsBreak = styledLabel(st, kMuted);
    cellsRow->addWidget(m_statCells);
    cellsRow->addWidget(m_statCellsBreak);
    cellsRow->addStretch(1);
    st->form()->addRow(I18n::tr("me_total"), cellsRow);

    auto *dxRow = new QHBoxLayout();
    m_statDxMin  = styledLabel(st, kMono);
    m_statLambda = styledLabel(st, kMuted);
    m_statBadge  = new QLabel(st);
    dxRow->addWidget(m_statDxMin);
    dxRow->addWidget(m_statLambda);
    dxRow->addWidget(m_statBadge);
    dxRow->addStretch(1);
    st->form()->addRow(I18n::tr("mst_dx_min"), dxRow);

    m_statCfl = styledLabel(st, kMono);
    st->form()->addRow(I18n::tr("mst_cfl"), m_statCfl);

    m_statMem = styledLabel(st, kMono);
    st->form()->addRow(I18n::tr("mst_memory"), m_statMem);
    v->addWidget(st);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // 入力方法 / λ/n チェックはローカル状態 (Project 非永続) → touch() しない
    connect(m_method, &QComboBox::currentIndexChanged,
            this, &MeshTab::updateMethodView);
    connect(m_lambdaCheck, &QCheckBox::toggled, this, &MeshTab::updateMethodView);
    updateMethodView();

    connect(project, &Project::loaded, this, &MeshTab::refresh);
    refresh();
}

void MeshTab::updateMethodView()
{
    const bool annotated = (m_method->currentIndex() == 0);
    for (int a = 0; a < 3; ++a)
        m_table[a]->setColumnHidden(2, !annotated);
    const bool lam = m_lambdaCheck->isChecked();
    m_statLambda->setVisible(lam);
    m_statBadge->setVisible(lam);
}

void MeshTab::applyAxis(int a)
{
    MeshAxis &ax = m_p->mesh(a);
    const int rows = m_table[a]->rowCount();
    ax.nodes.resize(rows);
    ax.divs.resize(qMax(0, rows - 1));
    for (int r = 0; r < rows; ++r) {
        if (auto *it = m_table[a]->item(r, 0))
            ax.nodes[r] = it->text().toDouble();
        if (r < rows - 1) {
            if (auto *it = m_table[a]->item(r, 1))
                ax.divs[r] = qMax(1, it->text().toInt());
        }
    }
}

void MeshTab::refreshAxisInfo(int a)
{
    const MeshAxis &ax = m_p->mesh(a);
    m_info[a]->setText(QStringLiteral("%1: %2   %3: %4")
        .arg(I18n::tr("me_cells")).arg(ax.totalCells())
        .arg(I18n::tr("me_dmin"),
             ax.isValid() ? QString::number(ax.minSpacing(), 'g', 4) : "—"));
    m_total->setText(QStringLiteral("%1: %L2")
        .arg(I18n::tr("me_total")).arg(m_p->totalCells()));
}

// メッシュ統計 / Mesh Statistics — 表示専用 (mock の最終セクション)。
// 総セル数 = nx×ny×nz、最小Δx を frequency1 中心波長で評価しバッジを出す。
void MeshTab::refreshStats()
{
    const QLocale loc;
    m_statCells->setText(loc.toString(qlonglong(m_p->totalCells())));
    m_statCellsBreak->setText(QStringLiteral("(%1 × %2 × %3)")
        .arg(m_p->mesh(0).totalCells())
        .arg(m_p->mesh(1).totalCells())
        .arg(m_p->mesh(2).totalCells()));

    // 最小Δx = 3軸のセル幅の最小値 (どれか1軸でも不正なら "—")
    double dmin = 1e308;
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &ax = m_p->mesh(a);
        if (!ax.isValid()) { dmin = 1e308; break; }
        dmin = qMin(dmin, ax.minSpacing());
    }
    const bool dxOk = (dmin > 0 && dmin < 1e308);
    m_statDxMin->setText(dxOk ? QStringLiteral("%1 m").arg(formatSci(dmin))
                              : QStringLiteral("—"));

    // λ/N @ f — 解析周波数1 の中心周波数で評価 (mock: "→ λ/22 @ 2.5 GHz")
    const GeneralOpts &g = m_p->general();
    const double fc = g.hasF1 ? 0.5 * (g.f1min + g.f1max) : 0.0;
    if (dxOk && fc > 0) {
        const double lambda = kC0 / fc;
        const qint64 n = qint64(qBound(0.0, lambda / dmin + 0.5, 1e12));
        m_statLambda->setText(I18n::tr("mst_lambda_fmt")
                                  .arg(n).arg(formatFreq(fc)));
        const char *key = (n >= 20) ? "mst_ok" : (n >= 10) ? "mst_coarse" : "mst_ng";
        const char *col = (n >= 20) ? kOk     : (n >= 10) ? kWarn        : kErr;
        m_statBadge->setText(I18n::tr(key));
        m_statBadge->setStyleSheet(QStringLiteral("color:%1; font-weight:600;")
                                       .arg(QLatin1String(col)));
    } else {
        m_statLambda->clear();
        m_statBadge->clear();
        m_statBadge->setStyleSheet(QString());
    }

    const double dt = m_p->courantDt();
    m_statCfl->setText(dt > 0 ? QStringLiteral("%1 s").arg(dt, 0, 'e', 2)
                              : QStringLiteral("—"));

    const double mb = m_p->estimatedMemoryMB();
    m_statMem->setText(mb >= 1024.0
        ? QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2)
        : QStringLiteral("%1 MB").arg(mb, 0, 'f', 1));
}

void MeshTab::refresh()
{
    m_updating = true;
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &ax = m_p->mesh(a);
        m_table[a]->setRowCount(ax.nodes.size());
        while (m_names[a].size() < ax.nodes.size()) m_names[a].append(QString());
        while (m_names[a].size() > ax.nodes.size()) m_names[a].removeLast();
        for (int r = 0; r < ax.nodes.size(); ++r) {
            m_table[a]->setItem(r, 0, new QTableWidgetItem(
                QString::number(ax.nodes[r], 'g', 10)));
            auto *divItem = new QTableWidgetItem(
                r < ax.divs.size() ? QString::number(ax.divs[r]) : QString("—"));
            if (r >= ax.divs.size())
                divItem->setFlags(divItem->flags() & ~Qt::ItemIsEditable);
            m_table[a]->setItem(r, 1, divItem);
            // 名前列 (説明あり) — ローカル注記。モデルには反映しない。
            m_table[a]->setItem(r, 2, new QTableWidgetItem(m_names[a].at(r)));
        }
        refreshAxisInfo(a);
    }
    refreshStats();
    m_updating = false;
}
