// ToleranceTab.cpp
#include "ToleranceTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 tol_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("tol_title_fmt", "製造ばらつき・歩留まり解析 (%1)",
              "Tolerance & Yield (%1)");
    // 誇大ヒントの是正 (CLAUDE.md 絶対規則 5): 未実装であることを明記する
    I18n::reg("tol_hint",
              "製造誤差/環境変動をモンテカルロでサンプリングし、性能分布と歩留まりを評価。\n"
              "(モンテカルロは未実装 — 画面は設計モック)",
              "Samples manufacturing and environmental variations by Monte Carlo, then "
              "evaluates the performance distribution and yield.\n"
              "(Monte Carlo not implemented — this screen is a design mock.)");

    I18n::reg("tol_sources", "ばらつき要因", "Variation sources");
    I18n::reg("tol_c_item", "項目", "Item");
    I18n::reg("tol_c_dist", "分布", "Distribution");
    I18n::reg("tol_c_sigma", "σ (1σ)", "σ (1σ)");
    I18n::reg("tol_c_unit", "単位", "Unit");

    I18n::reg("tol_mc", "モンテカルロ設定", "Monte Carlo settings");
    I18n::reg("tol_samples", "サンプル数", "Samples");
    I18n::reg("tol_method", "サンプリング法", "Sampling method");
    I18n::reg("tol_random", "完全ランダム", "Pure random");
    I18n::reg("tol_lhs", "Latin Hypercube", "Latin Hypercube");
    I18n::reg("tol_sobol", "Sobol系列", "Sobol sequence");

    I18n::reg("tol_criteria", "合格条件", "Pass criteria");
    I18n::reg("tol_results", "結果", "Results");
    // モンテカルロは未実装なので「完了」を偽装しない
    I18n::reg("tol_lastrun", "最終ラン: 未実行", "Last run: not run yet");
    I18n::reg("tol_yield", "歩留まり 87.4%", "Yield 87.4%");
    I18n::reg("tol_3sigma", "3σ range = %1", "3σ range = %1");
    I18n::reg("tol_density", "density", "density");
    I18n::reg("tol_report", "📤 統計レポート (PDF)", "📤 Statistics report (PDF)");
    I18n::reg("tol_sensitivity", "📊 Sensitivity 解析", "📊 Sensitivity analysis");
    I18n::reg("tol_robust", "🎯 ロバスト最適化", "🎯 Robust optimization");
    return true;
}();

// mock の sources[domain] をそのまま転記
struct SourceRow { bool ck; const char *name, *dist, *sigma, *unit; };
const SourceRow kEmSrc[6] = {
    { true,  "パッチ寸法",     "正規", "0.05",  "mm" },
    { true,  "基板誘電率 εr",  "正規", "0.05",  "—"  },
    { true,  "基板厚さ",       "正規", "0.025", "mm" },
    { true,  "給電位置",       "正規", "0.1",   "mm" },
    { false, "はんだ位置",     "一様", "±0.1",  "mm" },
    { false, "温度変動",       "正規", "10",    "K"  },
};
const SourceRow kOptSrc[6] = {
    { true,  "導波路幅",       "正規",     "5",     "nm"     },
    { true,  "導波路厚さ",     "正規",     "3",     "nm"     },
    { true,  "側壁ラフネス",   "レイリー", "2.5",   "nm RMS" },
    { true,  "結合間隙 (gap)", "正規",     "10",    "nm"     },
    { false, "屈折率 n",       "正規",     "0.001", "—"      },
    { true,  "温度変動",       "正規",     "5",     "K"      },
};
const SourceRow kAcSrc[5] = {
    { true,  "吸音率α",      "正規", "0.05", "—"    },
    { true,  "壁面位置",     "正規", "0.1",  "m"    },
    { true,  "客席占有率",   "一様", "±20%", "—"    },
    { false, "室温",         "正規", "5",    "K"    },
    { false, "湿度",         "一様", "30~70", "%RH" },
};
const SourceRow kUwSrc[5] = {
    { true,  "音速プロファイル", "正規",     "5",   "m/s"      },
    { true,  "底質特性",         "離散",     "-",   "砂/泥/岩" },
    { true,  "水温変動",         "正規",     "2",   "K"        },
    { false, "塩分",             "正規",     "0.5", "psu"      },
    { false, "波高",             "レイリー", "1.5", "m"        },
};

// mock の criteria[domain]
struct Criteria { const char *goal, *val, *at, *unit, *range, *xLabel; };
const Criteria kEmCrit  = { "S11 ≤",       "-10",      "@ 2.45GHz",
                            "dB", "-7.8 ~ -16.2 dB (S11)",  "S11" };
const Criteria kOptCrit = { "透過率 ≥",    "0.7",      "@ 1550 nm",
                            "—",  "0.61 ~ 0.89 (T)",        "T" };
const Criteria kAcCrit  = { "RT60 (1kHz)", "1.0~1.8",  "全座席",
                            "s",  "0.95 ~ 2.10 s (RT60)",   "RT60" };
const Criteria kUwCrit  = { "TL ≤",        "90",       "@ 50km, 3.5kHz",
                            "dB", "78 ~ 102 dB (TL)",       "TL" };

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

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}
} // namespace

// ── ToleranceTab ────────────────────────────────────────────────────────────
ToleranceTab::ToleranceTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 製造ばらつき・歩留まり解析 (説明) ──────────────────────────────────
    m_titleSec = new SectionBox(QString(), body);
    m_titleSec->vbox()->addWidget(hintLabel(I18n::tr("tol_hint"), m_titleSec));
    v->addWidget(m_titleSec);

    // ── ばらつき要因 / Variation sources ───────────────────────────────────
    auto *sSrc = new SectionBox(I18n::tr("tol_sources"), body);
    m_sources = new QTableWidget(0, 5, sSrc);
    m_sources->setHorizontalHeaderLabels({ QString(), I18n::tr("tol_c_item"),
        I18n::tr("tol_c_dist"), I18n::tr("tol_c_sigma"), I18n::tr("tol_c_unit") });
    m_sources->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sources->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_sources->horizontalHeader()->resizeSection(0, 24);
    m_sources->verticalHeader()->setVisible(false);
    m_sources->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sources->setMinimumHeight(200);
    sSrc->vbox()->addWidget(m_sources);
    // ばらつき要因の編集はどこにも読まれない (Project 書込ゼロ)
    sSrc->vbox()->addWidget(tabhelp::unwiredNote(sSrc));
    v->addWidget(sSrc);

    // ── モンテカルロ設定 ───────────────────────────────────────────────────
    auto *sMc = new SectionBox(I18n::tr("tol_mc"), body);
    m_samples = new QLineEdit("1000", sMc);
    m_samples->setMaximumWidth(80);
    sMc->form()->addRow(I18n::tr("tol_samples"), m_samples);
    m_sampling = new QComboBox(sMc);
    m_sampling->addItem(I18n::tr("tol_random"));
    m_sampling->addItem(I18n::tr("tol_lhs"));
    m_sampling->addItem(I18n::tr("tol_sobol"));
    m_sampling->setCurrentIndex(1);          // 既定 "lhs"
    sMc->form()->addRow(I18n::tr("tol_method"), m_sampling);
    // サンプル数・サンプリング法はどこにも読まれない
    sMc->form()->addRow(tabhelp::unwiredNote(sMc));
    v->addWidget(sMc);

    // ── 合格条件 / Pass criteria ───────────────────────────────────────────
    auto *sCrit = new SectionBox(I18n::tr("tol_criteria"), body);
    auto *critRow = new QHBoxLayout();
    m_goal = new QLabel(sCrit);
    m_goalVal = new QLineEdit(sCrit);
    m_goalVal->setMaximumWidth(100);
    m_goalUnit = new QLabel(sCrit);
    m_goalAt = new QLabel(sCrit);
    critRow->addWidget(m_goal);
    critRow->addWidget(m_goalVal);
    critRow->addWidget(m_goalUnit);
    critRow->addWidget(m_goalAt);
    critRow->addStretch(1);
    sCrit->vbox()->addLayout(critRow);
    // 合格条件はどこにも読まれない
    sCrit->vbox()->addWidget(tabhelp::unwiredNote(sCrit));
    v->addWidget(sCrit);

    // ── 結果 / Results ─────────────────────────────────────────────────────
    auto *sRes = new SectionBox(I18n::tr("tol_results"), body);
    sRes->vbox()->addWidget(hintLabel(I18n::tr("tol_lastrun"), sRes));
    auto *yRow = new QHBoxLayout();
    m_yield = makeBadge(I18n::tr("tol_yield"), "ok", sRes);
    yRow->addWidget(m_yield);
    m_sigma3 = new QLabel(sRes);
    yRow->addWidget(m_sigma3);
    yRow->addStretch(1);
    sRes->vbox()->addLayout(yRow);

    m_hist = new MiniPlot(sRes);
    {
        // mock: 30点, x = 0.5 + i*0.015, y = exp(-((x-0.78)/0.07)²)
        MiniSeries s;
        for (int i = 0; i < 30; ++i) {
            const double x = 0.5 + i * 0.015;
            const double y = std::exp(-std::pow((x - 0.78) / 0.07, 2.0));
            s.pts.append(QPointF(x, y));
        }
        m_hist->setSeries({ s });
    }
    m_hist->setMinimumHeight(120);
    sRes->vbox()->addWidget(m_hist);
    // 歩留まりバッジ・3σ範囲・ヒストグラムはモック合成値 (MC 未実行)
    sRes->vbox()->addWidget(tabhelp::sampleNote(sRes));

    auto *btnRow = new QHBoxLayout();
    // 3 ボタンとも未配線 → 無効化 + 「未実装」ツールチップ
    auto *reportBtn = new QPushButton(I18n::tr("tol_report"), sRes);
    auto *sensBtn   = new QPushButton(I18n::tr("tol_sensitivity"), sRes);
    auto *robustBtn = new QPushButton(I18n::tr("tol_robust"), sRes);
    for (QPushButton *b : { reportBtn, sensBtn, robustBtn }) {
        tabhelp::markNotImplemented(b);
        btnRow->addWidget(b);
    }
    btnRow->addStretch(1);
    sRes->vbox()->addLayout(btnRow);
    v->addWidget(sRes);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &ToleranceTab::rebuildDomain);
    rebuildDomain();
}

void ToleranceTab::rebuildDomain()
{
    const Domain d = m_p->activeDomain();
    m_titleSec->setTitle(I18n::tr("tol_title_fmt").arg(domainKey(d).toUpper()));

    // ── ばらつき要因 ───────────────────────────────────────────────────────
    const SourceRow *rows = kEmSrc;
    int n = 6;
    switch (d) {
        case Domain::Optical:    rows = kOptSrc; n = 6; break;
        case Domain::Acoustic:   rows = kAcSrc;  n = 5; break;
        case Domain::Underwater: rows = kUwSrc;  n = 5; break;
        default:                 rows = kEmSrc;  n = 6; break;
    }
    m_sources->setRowCount(n);
    for (int r = 0; r < n; ++r) {
        auto *ck = new QTableWidgetItem;
        ck->setCheckState(rows[r].ck ? Qt::Checked : Qt::Unchecked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_sources->setItem(r, 0, ck);
        m_sources->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(rows[r].name)));
        m_sources->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(rows[r].dist)));
        auto *sig = new QTableWidgetItem(QString::fromUtf8(rows[r].sigma));
        sig->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_sources->setItem(r, 3, sig);
        m_sources->setItem(r, 4, new QTableWidgetItem(QString::fromUtf8(rows[r].unit)));
    }

    // ── 合格条件 / 結果 ───────────────────────────────────────────────────
    const Criteria &c = (d == Domain::Optical)    ? kOptCrit
                      : (d == Domain::Acoustic)   ? kAcCrit
                      : (d == Domain::Underwater) ? kUwCrit
                                                  : kEmCrit;
    m_goal->setText(QString::fromUtf8(c.goal));
    m_goalVal->setText(QString::fromUtf8(c.val));
    m_goalUnit->setText(QString::fromUtf8(c.unit));
    m_goalAt->setText(QString::fromUtf8(c.at));
    m_sigma3->setText(I18n::tr("tol_3sigma").arg(QString::fromUtf8(c.range)));
    m_hist->setLabels(QString::fromUtf8(c.xLabel), I18n::tr("tol_density"));
}
