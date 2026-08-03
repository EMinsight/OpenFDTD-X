// UltrasoundTab.cpp
#include "UltrasoundTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

using namespace ofd;

// ── タブ固有語彙 (us_) — file-local 登録 ────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_ultrasound", "🩺 超音波", "🩺 Ultrasound");
    // 概要
    I18n::reg("us_main_section", "超音波解析 (k-Wave / Field II 相当)",
              "Ultrasound (k-Wave / Field II class)");
    // 誇大ヒントの是正 (CLAUDE.md 絶対規則 5): 未実装であることを明記する
    I18n::reg("us_main_hint",
              "MHz帯の超音波伝搬。医療イメージング・HIFU治療・非破壊検査 (NDT)。\n"
              "非線形伝搬 (Westervelt / KZK) と減衰 (power-law) は未実装 — "
              "画面は設計モック。",
              "MHz-band ultrasound propagation: medical imaging, HIFU therapy and "
              "NDT.\nNonlinear propagation (Westervelt / KZK) and power-law "
              "absorption are not implemented — this screen is a design mock.");
    I18n::reg("us_app", "用途", "Application");
    I18n::reg("us_app_medical", "医療イメージング", "Medical imaging");
    I18n::reg("us_app_hifu", "HIFU治療", "HIFU therapy");
    I18n::reg("us_app_ndt", "NDT非破壊検査", "NDT inspection");
    I18n::reg("us_app_sonar", "パラメトリックアレイ", "Parametric array");
    // トランスデューサ
    I18n::reg("us_trans_section", "トランスデューサ", "Transducer");
    I18n::reg("us_array_type", "アレイ型式", "Array type");
    I18n::reg("us_arr_linear", "リニア", "Linear");
    I18n::reg("us_arr_convex", "コンベックス", "Convex");
    I18n::reg("us_arr_phased", "フェーズド", "Phased");
    I18n::reg("us_arr_matrix", "2Dマトリクス", "2D matrix");
    I18n::reg("us_elements", "素子数", "Elements");
    I18n::reg("us_center_freq", "中心周波数", "Center frequency");
    I18n::reg("us_bandwidth", "帯域幅", "Bandwidth");
    I18n::reg("us_focus_depth", "フォーカス深度", "Focal depth");
    I18n::reg("us_hifu_type", "型式", "Type");
    I18n::reg("us_hifu_bowl", "球面集束 (単一素子)",
              "Spherical bowl (single element)");
    I18n::reg("us_hifu_array", "256chフェーズドアレイ",
              "256-channel phased array");
    I18n::reg("us_freq", "周波数", "Frequency");
    I18n::reg("us_power", "音響出力", "Acoustic power");
    I18n::reg("us_focal_p", "焦点音圧", "Focal pressure");
    I18n::reg("us_nonlinear_zone", "非線形域", "Nonlinear regime");
    I18n::reg("us_probe", "探触子", "Probe");
    I18n::reg("us_probe_normal", "垂直", "Normal beam");
    I18n::reg("us_probe_angle", "斜角 (45/60/70°)", "Angle beam (45/60/70°)");
    I18n::reg("us_probe_paut", "フェーズドアレイ (PAUT)", "Phased array (PAUT)");
    I18n::reg("us_ndt_target", "検査対象", "Inspection target");
    I18n::reg("us_tgt_weld", "溶接部 (鋼)", "Weld (steel)");
    I18n::reg("us_tgt_cfrp", "CFRP積層板", "CFRP laminate");
    I18n::reg("us_tgt_concrete", "コンクリート", "Concrete");
    I18n::reg("us_tgt_pipe", "配管減肉", "Pipe wall thinning");
    I18n::reg("us_prim_freq", "1次周波数", "Primary frequency");
    I18n::reg("us_diff_freq", "差音", "Difference tone");
    I18n::reg("us_diff_u", "kHz (指向性スピーカー)",
              "kHz (parametric loudspeaker)");
    // 媒質
    I18n::reg("us_med_section", "媒質", "Medium properties");
    I18n::reg("us_h_tissue", "組織/材料", "Tissue / material");
    I18n::reg("us_mat_steel_l", "鋼 (縦波)", "Steel (longitudinal)");
    I18n::reg("us_mat_steel_s", "鋼 (横波)", "Steel (shear)");
    I18n::reg("us_mat_cfrp", "CFRP (0°)", "CFRP (0°)");
    I18n::reg("us_mat_water_c", "水 (接触媒質)", "Water (couplant)");
    I18n::reg("us_mat_water", "水", "Water");
    I18n::reg("us_mat_soft", "軟組織 (平均)", "Soft tissue (average)");
    I18n::reg("us_mat_fat", "脂肪", "Fat");
    I18n::reg("us_mat_liver", "肝臓", "Liver");
    I18n::reg("us_mat_bone", "骨 (皮質)", "Bone (cortical)");
    I18n::reg("us_powerlaw", "Power-law 吸収 (fractional Laplacian)",
              "Power-law absorption (fractional Laplacian)");
    I18n::reg("us_nonlinear", "非線形 (B/A)", "Nonlinear (B/A)");
    // ビームフォーミング
    I18n::reg("us_bf_section", "ビームフォーミング", "Beamforming");
    I18n::reg("us_tx_focus", "送信フォーカス遅延則", "Transmit focus delay law");
    I18n::reg("us_rx_focus", "ダイナミック受信フォーカス",
              "Dynamic receive focus");
    I18n::reg("us_apod", "アポダイゼーション (Hanning)",
              "Apodization (Hanning)");
    I18n::reg("us_planewave", "平面波コンパウンディング",
              "Plane-wave compounding");
    I18n::reg("us_steer", "ステアリング角", "Steering angle");
    // 評価・出力
    I18n::reg("us_out_section", "評価・出力", "Outputs");
    I18n::reg("us_o_beam", "ビームプロファイル (-6dB幅)",
              "Beam profile (-6 dB width)");
    I18n::reg("us_o_psf", "点像分布関数 PSF", "Point spread function (PSF)");
    I18n::reg("us_o_bmode", "Bモード画像シミュレーション",
              "B-mode image simulation");
    I18n::reg("us_o_miti", "MI / TI (安全指標)",
              "MI / TI (safety indices)");
    I18n::reg("us_o_ispta", "焦点音圧・音響強度 ISPTA",
              "Focal pressure and intensity ISPTA");
    I18n::reg("us_o_cem43", "熱線量 CEM43 (BioHeat連成)",
              "Thermal dose CEM43 (BioHeat coupling)");
    I18n::reg("us_o_cavitation", "キャビテーション閾値評価",
              "Cavitation threshold assessment");
    I18n::reg("us_o_scan", "A/B/C スキャン表示", "A / B / C scan display");
    I18n::reg("us_o_dac", "欠陥エコー DAC曲線", "Flaw echo DAC curve");
    I18n::reg("us_o_saft", "SAFT / TFM 再構成画像",
              "SAFT / TFM reconstruction");
    I18n::reg("us_o_diffeff", "差音生成効率", "Difference-tone efficiency");
    I18n::reg("us_o_pattern", "指向性ビームパターン", "Directivity beam pattern");
    I18n::reg("us_btn_beam", "📊 ビームプロファイル", "📊 Beam profile");
    I18n::reg("us_btn_anim", "🎬 伝搬アニメーション (H5)",
              "🎬 Propagation animation (H5)");
    I18n::reg("us_btn_report", "📄 レポート", "📄 Report");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-table 相当) ─────────────────────
const char kWarn[] = "#B45309";     // badge warn

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QString("color:%1; border:1px solid %1; border-radius:3px;"
                             " padding:1px 6px; font-weight:600;").arg(color));
    return l;
}

QLabel *makeHint(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QLabel *makeMono(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    QFont f = l->font();
    // 実在するファミリのみ指定 (Theme が環境ごとに解決済み)
    if (const QString mf = Theme::monoFontFamily(); !mf.isEmpty())
        f.setFamily(mf);
    f.setStyleHint(QFont::Monospace);
    l->setFont(f);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

QLineEdit *numEdit(const QString &text, QWidget *parent, int w = 80)
{
    auto *e = new QLineEdit(text, parent);
    e->setMaximumWidth(w);
    return e;
}

// <Seg> 相当 (少数選択肢の排他セグメント) — QComboBox で再現
QComboBox *makeSeg(const QStringList &items, int current, QWidget *parent)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
}

QHBoxLayout *unitRow(QWidget *w, const QString &unit, QWidget *parent)
{
    auto *h = new QHBoxLayout();
    h->addWidget(w);
    h->addWidget(new QLabel(unit, parent));
    h->addStretch(1);
    return h;
}

// mock の <Row> 相当: チェックボックスを横並びに
QHBoxLayout *checkRow(const QVector<QCheckBox *> &boxes)
{
    auto *h = new QHBoxLayout();
    h->setSpacing(8);
    for (auto *b : boxes)
        h->addWidget(b);
    h->addStretch(1);
    return h;
}

QTableWidgetItem *textItem(const QString &s) { return new QTableWidgetItem(s); }

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QTableWidget *makeTable(const QStringList &headers, int rows, QWidget *parent,
                        int minH)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
    return t;
}

// 媒質テーブル (モックの配列をそのまま): 名前キー, c, ρ, α, B/A
struct Med { const char *key; int c; int rho; const char *alpha;
             const char *ba; };
const Med kMedNdt[4] = {
    { "us_mat_steel_l", 5900, 7850, "0.02",  "—"   },
    { "us_mat_steel_s", 3230, 7850, "0.05",  "—"   },
    { "us_mat_cfrp",    3070, 1560, "1.2",   "—"   },
    { "us_mat_water_c", 1480, 1000, "0.002", "5.0" },
};
const Med kMedBio[5] = {
    { "us_mat_water", 1480, 1000, "0.002", "5.0"  },
    { "us_mat_soft",  1540, 1045, "0.54",  "6.8"  },
    { "us_mat_fat",   1450,  950, "0.48",  "10.0" },
    { "us_mat_liver", 1590, 1060, "0.5",   "6.8"  },
    { "us_mat_bone",  4080, 1900, "6.9",   "—"    },
};
} // namespace

// ── UltrasoundTab ───────────────────────────────────────────────────────────
UltrasoundTab::UltrasoundTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 超音波解析 (概要 + 用途) ────────────────────────────────────────────
    auto *sm = new SectionBox(I18n::tr("us_main_section"), body);
    sm->vbox()->addWidget(makeHint(I18n::tr("us_main_hint"), sm));
    m_app = makeSeg({ I18n::tr("us_app_medical"), I18n::tr("us_app_hifu"),
                      I18n::tr("us_app_ndt"), I18n::tr("us_app_sonar") },
                    0, sm);
    sm->form()->addRow(I18n::tr("us_app"), m_app);
    v->addWidget(sm);

    // ── トランスデューサ (用途ごとに切替) ───────────────────────────────────
    auto *st = new SectionBox(I18n::tr("us_trans_section"), body);
    m_transStack = new QStackedWidget(st);
    m_transStack->addWidget(buildMedicalTrans());   // 0 medical
    m_transStack->addWidget(buildHifuTrans());      // 1 hifu
    m_transStack->addWidget(buildNdtTrans());       // 2 ndt
    m_transStack->addWidget(buildSonarTrans());     // 3 sonar
    st->vbox()->addWidget(m_transStack);
    // トランスデューサ設定はどこにも読まれない (Project 書込ゼロ)
    st->vbox()->addWidget(tabhelp::unwiredNote(st));
    v->addWidget(st);

    // ── 媒質 ────────────────────────────────────────────────────────────────
    auto *sd = new SectionBox(I18n::tr("us_med_section"), body);
    m_medTable = makeTable({ I18n::tr("us_h_tissue"), "c [m/s]",
                             QString::fromUtf8("ρ [kg/m³]"),
                             QString::fromUtf8("α [dB/cm/MHz]"), "B/A" },
                           5, sd, 170);
    sd->vbox()->addWidget(m_medTable);
    // 媒質表はモックの固定文献値 (プロジェクトの材料とは無関係)
    sd->vbox()->addWidget(tabhelp::sampleNote(sd));
    m_powerLaw  = makeCheck(I18n::tr("us_powerlaw"),  true,  sd);
    m_nonlinear = makeCheck(I18n::tr("us_nonlinear"), false, sd);
    sd->vbox()->addLayout(checkRow({ m_powerLaw, m_nonlinear }));
    // チェック状態はどこにも読まれない
    sd->vbox()->addWidget(tabhelp::unwiredNote(sd));
    v->addWidget(sd);

    // ── ビームフォーミング ──────────────────────────────────────────────────
    auto *sb = new SectionBox(I18n::tr("us_bf_section"), body);
    m_txFocus   = makeCheck(I18n::tr("us_tx_focus"),  true,  sb);
    m_rxFocus   = makeCheck(I18n::tr("us_rx_focus"),  true,  sb);
    m_apod      = makeCheck(I18n::tr("us_apod"),      true,  sb);
    m_planeWave = makeCheck(I18n::tr("us_planewave"), false, sb);
    sb->vbox()->addLayout(checkRow({ m_txFocus, m_rxFocus }));
    sb->vbox()->addLayout(checkRow({ m_apod, m_planeWave }));
    m_steerAngle = numEdit("0", sb);
    sb->form()->addRow(I18n::tr("us_steer"),
                       unitRow(m_steerAngle, QString::fromUtf8("±45°"), sb));
    // ビームフォーミング設定はどこにも読まれない
    sb->form()->addRow(tabhelp::unwiredNote(sb));
    v->addWidget(sb);

    // ── 評価・出力 (用途ごとに切替 + 共通ボタン) ────────────────────────────
    auto *so = new SectionBox(I18n::tr("us_out_section"), body);
    m_outStack = new QStackedWidget(so);
    m_outStack->addWidget(buildMedicalOut());   // 0 medical
    m_outStack->addWidget(buildHifuOut());      // 1 hifu
    m_outStack->addWidget(buildNdtOut());       // 2 ndt
    m_outStack->addWidget(buildSonarOut());     // 3 sonar
    so->vbox()->addWidget(m_outStack);
    auto *hb = new QHBoxLayout();
    // 3 ボタンとも未配線 → 無効化 + 「未実装」ツールチップ
    auto *beamBtn   = new QPushButton(I18n::tr("us_btn_beam"), so);
    auto *animBtn   = new QPushButton(I18n::tr("us_btn_anim"), so);
    auto *reportBtn = new QPushButton(I18n::tr("us_btn_report"), so);
    for (QPushButton *b : { beamBtn, animBtn, reportBtn }) {
        tabhelp::markNotImplemented(b);
        hb->addWidget(b);
    }
    hb->addStretch(1);
    so->vbox()->addLayout(hb);
    // 出力チェックはどこにも読まれない
    so->vbox()->addWidget(tabhelp::unwiredNote(so));
    v->addWidget(so);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_app, &QComboBox::currentIndexChanged, this, [this](int i) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_appIdx = i;
        onAppChanged();
    });
    connect(project, &Project::loaded, this, &UltrasoundTab::refresh);
    refresh();
}

void UltrasoundTab::refresh()
{
    m_updating = true;
    m_app->setCurrentIndex(m_appIdx);
    m_updating = false;
    onAppChanged();
}

void UltrasoundTab::onAppChanged()
{
    const bool ndt  = (m_appIdx == 2);
    const bool hifu = (m_appIdx == 1);
    m_transStack->setCurrentIndex(m_appIdx);
    m_outStack->setCurrentIndex(m_appIdx);
    fillMediumTable(ndt);
    // mock: <Check label="非線形 (B/A)" checked={app==="hifu"} />
    m_nonlinear->setChecked(hifu);
}

void UltrasoundTab::fillMediumTable(bool ndt)
{
    const Med *rows = ndt ? kMedNdt : kMedBio;
    const int n = ndt ? 4 : 5;
    m_medTable->clearContents();
    m_medTable->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        m_medTable->setItem(i, 0, textItem(I18n::tr(rows[i].key)));
        m_medTable->setItem(i, 1, numItem(QString::number(rows[i].c)));
        m_medTable->setItem(i, 2, numItem(QString::number(rows[i].rho)));
        m_medTable->setItem(i, 3, numItem(QString::fromUtf8(rows[i].alpha)));
        m_medTable->setItem(i, 4, numItem(QString::fromUtf8(rows[i].ba)));
    }
}

// ── 医療イメージング / medical ──────────────────────────────────────────────
QWidget *UltrasoundTab::buildMedicalTrans()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_arrayType = makeSeg({ I18n::tr("us_arr_linear"), I18n::tr("us_arr_convex"),
                            I18n::tr("us_arr_phased"),
                            I18n::tr("us_arr_matrix") }, 0, page);
    fl->addRow(I18n::tr("us_array_type"), m_arrayType);
    m_elements = numEdit("128", page);
    fl->addRow(I18n::tr("us_elements"), m_elements);
    m_centerFreq = numEdit("5.0", page);
    fl->addRow(I18n::tr("us_center_freq"), unitRow(m_centerFreq, "MHz", page));
    m_bandwidth = numEdit("70", page);
    fl->addRow(I18n::tr("us_bandwidth"), unitRow(m_bandwidth, "%", page));
    m_focusDepth = numEdit("40", page);
    fl->addRow(I18n::tr("us_focus_depth"), unitRow(m_focusDepth, "mm", page));
    return page;
}

// ── HIFU治療 / hifu ─────────────────────────────────────────────────────────
QWidget *UltrasoundTab::buildHifuTrans()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_hifuType = makeSeg({ I18n::tr("us_hifu_bowl"), I18n::tr("us_hifu_array") },
                         0, page);
    fl->addRow(I18n::tr("us_hifu_type"), m_hifuType);
    m_hifuFreq = numEdit("1.0", page);
    fl->addRow(I18n::tr("us_freq"), unitRow(m_hifuFreq, "MHz", page));
    m_hifuPower = numEdit("150", page);
    fl->addRow(I18n::tr("us_power"), unitRow(m_hifuPower, "W", page));

    auto *h = new QHBoxLayout();
    h->addWidget(makeMono("~8 MPa", page));
    h->addWidget(makeBadge(I18n::tr("us_nonlinear_zone"), kWarn, page));
    h->addStretch(1);
    fl->addRow(I18n::tr("us_focal_p"), h);
    // 焦点音圧・非線形域バッジはモック固定値 (計算していない)
    fl->addRow(tabhelp::sampleNote(page));
    return page;
}

// ── NDT非破壊検査 / ndt ─────────────────────────────────────────────────────
QWidget *UltrasoundTab::buildNdtTrans()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_probeType = makeSeg({ I18n::tr("us_probe_normal"),
                            I18n::tr("us_probe_angle"),
                            I18n::tr("us_probe_paut"), "TOFD" }, 1, page);
    fl->addRow(I18n::tr("us_probe"), m_probeType);
    m_ndtFreq = numEdit("5.0", page);
    fl->addRow(I18n::tr("us_freq"), unitRow(m_ndtFreq, "MHz", page));
    m_ndtTarget = makeSeg({ I18n::tr("us_tgt_weld"), I18n::tr("us_tgt_cfrp"),
                            I18n::tr("us_tgt_concrete"),
                            I18n::tr("us_tgt_pipe") }, 0, page);
    fl->addRow(I18n::tr("us_ndt_target"), m_ndtTarget);
    return page;
}

// ── パラメトリックアレイ / sonar ────────────────────────────────────────────
QWidget *UltrasoundTab::buildSonarTrans()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_primFreq = numEdit("40", page);
    fl->addRow(I18n::tr("us_prim_freq"), unitRow(m_primFreq, "kHz", page));
    m_diffFreq = numEdit("2", page);
    fl->addRow(I18n::tr("us_diff_freq"),
               unitRow(m_diffFreq, I18n::tr("us_diff_u"), page));
    return page;
}

// ── 出力 (用途別) ───────────────────────────────────────────────────────────
QWidget *UltrasoundTab::buildMedicalOut()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_oBeam  = makeCheck(I18n::tr("us_o_beam"),  true,  page);
    m_oPsf   = makeCheck(I18n::tr("us_o_psf"),   true,  page);
    m_oBmode = makeCheck(I18n::tr("us_o_bmode"), false, page);
    m_oMiTi  = makeCheck(I18n::tr("us_o_miti"),  true,  page);
    v->addLayout(checkRow({ m_oBeam, m_oPsf }));
    v->addLayout(checkRow({ m_oBmode, m_oMiTi }));
    return page;
}

QWidget *UltrasoundTab::buildHifuOut()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_oIspta      = makeCheck(I18n::tr("us_o_ispta"),      true,  page);
    m_oCem43      = makeCheck(I18n::tr("us_o_cem43"),      true,  page);
    m_oCavitation = makeCheck(I18n::tr("us_o_cavitation"), false, page);
    v->addLayout(checkRow({ m_oIspta, m_oCem43 }));
    v->addLayout(checkRow({ m_oCavitation }));
    return page;
}

QWidget *UltrasoundTab::buildNdtOut()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_oScan = makeCheck(I18n::tr("us_o_scan"), true,  page);
    m_oDac  = makeCheck(I18n::tr("us_o_dac"),  true,  page);
    m_oSaft = makeCheck(I18n::tr("us_o_saft"), false, page);
    v->addLayout(checkRow({ m_oScan, m_oDac }));
    v->addLayout(checkRow({ m_oSaft }));
    return page;
}

QWidget *UltrasoundTab::buildSonarOut()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_oDiffEff     = makeCheck(I18n::tr("us_o_diffeff"), true, page);
    m_oBeamPattern = makeCheck(I18n::tr("us_o_pattern"), true, page);
    v->addLayout(checkRow({ m_oDiffEff, m_oBeamPattern }));
    return page;
}
