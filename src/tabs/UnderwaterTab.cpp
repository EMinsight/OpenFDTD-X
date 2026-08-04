// UnderwaterTab.cpp
#include "UnderwaterTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;

// ── タブ固有の翻訳キー (uwx_) — file-local 登録 (既存 uw_ は I18n.cpp) ───────
namespace {
const bool s_i18nUnderwater = [] {
    using ofd::I18n;
    // ソルバー / Solver
    I18n::reg("uwx_solver_section", "ソルバー", "Solver");
    I18n::reg("uwx_sv_fdtd", "FDTD (波動)", "FDTD (wave)");
    I18n::reg("uwx_sv_bellhop", "Bellhop", "Bellhop");
    I18n::reg("uwx_sv_pe", "PE (放物方程式)", "PE (parabolic equation)");
    I18n::reg("uwx_sv_hybrid", "ハイブリッド", "Hybrid");
    I18n::reg("uwx_sv_desc_fdtd", "波動FDTD — 近距離・複雑地形の精密解析",
              "Wave FDTD — precise analysis at short range and over complex terrain");
    I18n::reg("uwx_sv_desc_bellhop",
              "ガウシアンビームレイトレース — 長距離(>10km)伝搬の標準手法",
              "Gaussian-beam ray tracing — the standard for long-range (>10 km) "
              "propagation");
    I18n::reg("uwx_sv_desc_pe",
              "Parabolic Equation — 中距離・低周波の高精度解析 (RAM相当)",
              "Parabolic equation — accurate at mid range / low frequency "
              "(RAM equivalent)");
    I18n::reg("uwx_sv_desc_hybrid", "近距離FDTD + 遠方Bellhop の段階的解析",
              "Staged analysis: near-field FDTD + far-field Bellhop");
    // Bellhop パラメータ
    I18n::reg("uwx_num_rays", "レイ数", "# rays");
    I18n::reg("uwx_beam_type", "ビーム種別", "Beam type");
    I18n::reg("uwx_beam_geom", "幾何", "Geometric");
    I18n::reg("uwx_beam_gauss", "ガウシアン", "Gaussian");
    I18n::reg("uwx_beam_hat", "Hat関数", "Hat function");
    I18n::reg("uwx_angle_range", "角度範囲 [°]", "Angle range [°]");
    I18n::reg("uwx_calc_mode", "計算モード", "Run mode");
    I18n::reg("uwx_mode_eigen", "固有線", "Eigenrays");
    I18n::reg("uwx_mode_coher", "コヒーレント", "Coherent");
    I18n::reg("uwx_mode_incoh", "インコヒーレント", "Incoherent");
    I18n::reg("uwx_mode_arr", "到達時間", "Arrivals");
    I18n::reg("uwx_visualize", "可視化", "Visualization");
    I18n::reg("uwx_vis_ray", "レイ図", "Ray diagram");
    I18n::reg("uwx_vis_tl", "TL等高線", "TL contours");
    I18n::reg("uwx_vis_echo", "エコー応答", "Echo response");
    // PE パラメータ
    I18n::reg("uwx_pe_algo", "アルゴリズム", "Algorithm");
    I18n::reg("uwx_pe_ssf", "Split-Step Fourier", "Split-step Fourier");
    I18n::reg("uwx_pe_crank", "Crank-Nicolson", "Crank-Nicolson");
    I18n::reg("uwx_pe_angular", "アンギュラブランチ [°]", "Angular branch [°]");
    // 海洋環境 / SSP
    I18n::reg("uwx_c0", "基準音速 c₀", "Reference speed c₀");
    I18n::reg("uwx_calc_value", "(計算値)", "(computed)");
    I18n::reg("uwx_sofar_depth", "→ SOFARチャネル深度 ~%1m",
              "→ SOFAR channel axis ≈ %1 m");
    // 海洋環境 — 水温はモック (i18n.js uw_temp) が「水温 [℃] / Water temp [℃]」。
    // I18n.cpp の共通 uw_temp は旧表記なので、表示だけモックに合わせる。
    I18n::reg("uwx_temp", "水温 [℃]", "Water temp [℃]");
    // 海底特性
    I18n::reg("uwx_bottom_alpha", "吸収係数 α [dB/λ]", "Absorption α [dB/λ]");
    // 底質の種類 — モック (tabs.jsx の 底質 セレクタ) の 4 種 + 既存 .ofdx 値。
    I18n::reg("uwx_bottom_sand", "砂", "Sand");
    I18n::reg("uwx_bottom_silt", "シルト", "Silt");
    I18n::reg("uwx_bottom_clay", "粘土", "Clay");
    I18n::reg("uwx_bottom_rock", "岩盤", "Rock");
    I18n::reg("uwx_bottom_mud", "泥", "Mud");
    I18n::reg("uwx_bottom_gravel", "礫", "Gravel");
    // 海面
    I18n::reg("uwx_surface_section", "海面", "Sea surface");
    I18n::reg("uwx_wave_height", "波高 [m]", "Wave height [m]");
    I18n::reg("uwx_specular", "鏡面反射", "Specular reflection");
    I18n::reg("uwx_bragg", "散乱モデル (Bragg)", "Scattering model (Bragg)");
    // ソナー
    I18n::reg("uwx_sonar_dir", "ソナー指向性", "Sonar directivity");
    I18n::reg("uwx_dir_omni", "全方位", "Omni");
    I18n::reg("uwx_dir_dir", "指向性", "Directional");
    I18n::reg("uwx_dir_array", "アレイ", "Array");
    I18n::reg("uwx_beam_width", "ビーム幅 [°]", "Beamwidth [°]");
    // 伝搬損失 (TL)
    I18n::reg("uwx_tl_section", "伝搬損失 (TL)", "Transmission loss (TL)");
    I18n::reg("uwx_tl_spread", "距離依存損失", "Spreading loss");
    I18n::reg("uwx_tl_absorb", "吸収損失 (Thorp)", "Absorption loss (Thorp)");
    I18n::reg("uwx_tl_scatter", "散乱損失", "Scattering loss");
    I18n::reg("uwx_tl_surface", "海面ロス", "Sea-surface loss");
    I18n::reg("uwx_tl_range", "距離 [km]", "Range [km]");
    return true;
}();

// mock の CSS クラス相当 (最小限のスタイル)
;
const char kMuted[] = "color:#888888;";
const char kAccUw[] = "#1E6FBF";     // styles.css --acc-underwater

QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(kMuted);
    l->setWordWrap(true);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// <Seg> 相当: 少数選択肢の排他選択 → QComboBox
QComboBox *makeSeg(QWidget *parent, const QStringList &items, int current)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
}

QDoubleSpinBox *makeSpin(QWidget *parent, double lo, double hi, double value,
                         int decimals)
{
    auto *w = new QDoubleSpinBox(parent);
    w->setRange(lo, hi);
    w->setDecimals(decimals);
    w->setValue(value);
    w->setMaximumWidth(120);
    return w;
}

// 海水の音速 (Mackenzie 1981, z = 0 の基準値)。
//   c = 1448.96 + 4.591T − 5.304e-2 T² + 2.374e-4 T³ + 1.340(S−35)
//     − 1.025e-2 T(S−35)
// mock は静的な表示値 (1500.3 m/s) だが、ここでは水温・塩分から実際に計算する。
double refSoundSpeed(double T, double S)
{
    return 1448.96 + 4.591 * T - 5.304e-2 * T * T + 2.374e-4 * T * T * T
         + 1.340 * (S - 35.0) - 1.025e-2 * T * (S - 35.0);
}

} // namespace

// ── UwSspPlot ───────────────────────────────────────────────────────────────
UwSspPlot::UwSspPlot(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(310, 130);
}

void UwSspPlot::setProfile(const QVector<QPointF> &pts)
{
    m_pts = pts;
    update();
}

void UwSspPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());

    const QRectF plot(46, 10, width() - 58, height() - 40);
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(plot);

    // mock の xRange=[1480,1540] / yRange=[5000,0] を既定とし、外れる点は含める
    double cLo = 1480, cHi = 1540, dMax = 5000;
    for (const QPointF &pt : m_pts) {
        cLo = std::min(cLo, pt.x() - 5.0);
        cHi = std::max(cHi, pt.x() + 5.0);
        dMax = std::max(dMax, pt.y());
    }
    if (cHi <= cLo) cHi = cLo + 1.0;
    if (dMax <= 0) dMax = 1.0;

    auto X = [&](double c) {
        return plot.left() + (c - cLo) / (cHi - cLo) * plot.width();
    };
    auto Y = [&](double d) { return plot.top() + d / dMax * plot.height(); };

    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);
    for (int i = 0; i <= 4; ++i) {
        const double gc = cLo + (cHi - cLo) * i / 4.0;
        const double gd = dMax * i / 4.0;
        p.setPen(QPen(palette().midlight().color(), 1, Qt::DotLine));
        p.drawLine(QPointF(X(gc), plot.top()), QPointF(X(gc), plot.bottom()));
        p.drawLine(QPointF(plot.left(), Y(gd)), QPointF(plot.right(), Y(gd)));
        p.setPen(palette().text().color());
        p.drawText(QRectF(X(gc) - 30, plot.bottom() + 2, 60, 12), Qt::AlignCenter,
                   QString::number(gc, 'g', 5));
        p.drawText(QRectF(0, Y(gd) - 6, plot.left() - 4, 12),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(gd, 'g', 4));
    }

    // プロファイル (深度は下向き)
    p.setPen(QPen(QColor(kAccUw), 1.8));
    QPainterPath path;
    for (int i = 0; i < m_pts.size(); ++i) {
        const QPointF sp(X(m_pts[i].x()), Y(m_pts[i].y()));
        if (i == 0) path.moveTo(sp); else path.lineTo(sp);
    }
    p.drawPath(path);
    p.setBrush(QColor(kAccUw));
    for (const QPointF &pt : m_pts)
        p.drawEllipse(QPointF(X(pt.x()), Y(pt.y())), 2.2, 2.2);
    p.setBrush(Qt::NoBrush);

    // 軸ラベル
    p.setPen(palette().text().color());
    p.drawText(QRectF(plot.left(), height() - 15, plot.width(), 14),
               Qt::AlignCenter, QStringLiteral("c [m/s]"));
    p.save();
    p.translate(10, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-60, -6, 120, 12), Qt::AlignCenter,
               QStringLiteral("depth [m]"));
    p.restore();
}

// ── UnderwaterTab ───────────────────────────────────────────────────────────
UnderwaterTab::UnderwaterTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ソルバー / Solver (mock の先頭セクション) — 選択に応じた説明文 +
    // 条件付きパラメータ。.ofdx に対応フィールドが無いのでローカル状態。
    auto *sv = new SectionBox(I18n::tr("uwx_solver_section"), body);
    m_solver = makeSeg(sv, { I18n::tr("uwx_sv_fdtd"), I18n::tr("uwx_sv_bellhop"),
                             I18n::tr("uwx_sv_pe"), I18n::tr("uwx_sv_hybrid") }, 0);
    sv->vbox()->addWidget(m_solver);
    m_solverDesc = mutedLabel(QString(), sv);
    sv->vbox()->addWidget(m_solverDesc);

    m_bellhopPanel = new QWidget(sv);
    auto *bhForm = new QFormLayout(m_bellhopPanel);
    bhForm->setContentsMargins(0, 0, 0, 0);
    bhForm->setHorizontalSpacing(8);
    bhForm->setVerticalSpacing(4);
    m_numRays = new QSpinBox(m_bellhopPanel);
    m_numRays->setRange(1, 10000000);
    m_numRays->setValue(5000);
    m_numRays->setMaximumWidth(120);
    bhForm->addRow(I18n::tr("uwx_num_rays"), m_numRays);
    m_beamType = makeSeg(m_bellhopPanel, { I18n::tr("uwx_beam_geom"),
                                           I18n::tr("uwx_beam_gauss"),
                                           I18n::tr("uwx_beam_hat") }, 1);
    bhForm->addRow(I18n::tr("uwx_beam_type"), m_beamType);
    m_angMin = makeSpin(m_bellhopPanel, -90, 90, -30, 0);
    m_angMax = makeSpin(m_bellhopPanel, -90, 90, 30, 0);
    auto *angRow = new QHBoxLayout();
    angRow->addWidget(m_angMin);
    angRow->addWidget(new QLabel(QStringLiteral("~"), m_bellhopPanel));
    angRow->addWidget(m_angMax);
    angRow->addStretch(1);
    bhForm->addRow(I18n::tr("uwx_angle_range"), angRow);
    m_calcMode = makeSeg(m_bellhopPanel, { I18n::tr("uwx_mode_eigen"),
                                           I18n::tr("uwx_mode_coher"),
                                           I18n::tr("uwx_mode_incoh"),
                                           I18n::tr("uwx_mode_arr") }, 0);
    bhForm->addRow(I18n::tr("uwx_calc_mode"), m_calcMode);
    m_visRay  = makeCheck(I18n::tr("uwx_vis_ray"), true, m_bellhopPanel);
    m_visTL   = makeCheck(I18n::tr("uwx_vis_tl"), true, m_bellhopPanel);
    m_visEcho = makeCheck(I18n::tr("uwx_vis_echo"), false, m_bellhopPanel);
    auto *visRow = new QHBoxLayout();
    visRow->addWidget(m_visRay);
    visRow->addWidget(m_visTL);
    visRow->addWidget(m_visEcho);
    visRow->addStretch(1);
    bhForm->addRow(I18n::tr("uwx_visualize"), visRow);
    sv->vbox()->addWidget(m_bellhopPanel);

    m_pePanel = new QWidget(sv);
    auto *peForm = new QFormLayout(m_pePanel);
    peForm->setContentsMargins(0, 0, 0, 0);
    peForm->setHorizontalSpacing(8);
    peForm->setVerticalSpacing(4);
    m_peAlgo = makeSeg(m_pePanel, { I18n::tr("uwx_pe_ssf"),
                                    I18n::tr("uwx_pe_crank") }, 0);
    peForm->addRow(I18n::tr("uwx_pe_algo"), m_peAlgo);
    m_peAngular = makeSpin(m_pePanel, 1, 90, 30, 0);
    peForm->addRow(I18n::tr("uwx_pe_angular"), m_peAngular);
    sv->vbox()->addWidget(m_pePanel);
    // ソルバー選択と Bellhop/PE パラメータはローカル状態のみで、
    // 計算にも .ofdx にも反映されない (apply() 非対象)
    sv->vbox()->addWidget(tabhelp::unwiredNote(sv));
    v->addWidget(sv);

    // environment
    auto *se = new SectionBox(I18n::tr("uw_env"), body);
    m_temp = new QDoubleSpinBox(se);
    m_temp->setRange(-2, 40);
    m_salinity = new QDoubleSpinBox(se);
    m_salinity->setRange(0, 45);
    m_sofar = new QCheckBox(se);
    se->form()->addRow(I18n::tr("uwx_temp"), m_temp);
    se->form()->addRow(I18n::tr("uw_salinity"), m_salinity);
    // 基準音速 c₀ — 水温・塩分からの計算値 (mock の「基準音速 c₀ … (計算値)」)
    m_c0 = new QLabel(se);
    m_c0->setStyleSheet(Theme::monoQss());
    auto *c0Row = new QHBoxLayout();
    c0Row->addWidget(m_c0);
    c0Row->addWidget(mutedLabel(I18n::tr("uwx_calc_value"), se));
    c0Row->addStretch(1);
    se->form()->addRow(I18n::tr("uwx_c0"), c0Row);
    se->form()->addRow(I18n::tr("uw_sofar"), m_sofar);
    v->addWidget(se);

    // SSP table
    auto *sp = new SectionBox(I18n::tr("uw_ssp"), body);
    m_ssp = new QTableWidget(0, 2, sp);
    m_ssp->setHorizontalHeaderLabels({ I18n::tr("uw_depth"), I18n::tr("uw_speed") });
    m_ssp->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ssp->verticalHeader()->setDefaultSectionSize(22);
    m_ssp->setMinimumHeight(140);
    sp->vbox()->addWidget(m_ssp);
    auto *row = new QHBoxLayout();
    auto *add = new QPushButton(I18n::tr("p2_add"), sp);
    auto *del = new QPushButton(I18n::tr("p2_del"), sp);
    row->addWidget(add); row->addWidget(del); row->addStretch(1);
    sp->vbox()->addLayout(row);
    // SOFAR チャネル軸 (音速最小の深度) — mock の「→ SOFARチャネル深度 ~1200m」
    m_sofarHint = mutedLabel(QString(), sp);
    sp->vbox()->addWidget(m_sofarHint);
    // 音速プロファイル図 (mock の MiniPlot, 深度下向き)
    m_sspPlot = new UwSspPlot(sp);
    sp->vbox()->addWidget(m_sspPlot);
    v->addWidget(sp);

    // seabed
    auto *sb = new SectionBox(I18n::tr("uw_bottom"), body);
    m_bottomType = new QComboBox(sb);
    // 表示ラベルはモックの語彙 (砂/シルト/粘土/岩盤)、保存値は .ofdx の
    // bottom_type と互換の英小文字コード (OceanEnvironmentTab も "sand"/"mud" を
    // 書く) を itemData に持たせる。既存コード mud/gravel はモック外だが温存。
    struct BottomKind { const char *code; const char *key; };
    static const BottomKind kBottoms[] = {
        { "sand",   "uwx_bottom_sand"   },   // ← モックの 4 種 (この順)
        { "silt",   "uwx_bottom_silt"   },
        { "clay",   "uwx_bottom_clay"   },
        { "rock",   "uwx_bottom_rock"   },
        { "mud",    "uwx_bottom_mud"    },   // ← 既存 .ofdx 値 (モック外)
        { "gravel", "uwx_bottom_gravel" }
    };
    for (const BottomKind &b : kBottoms)
        m_bottomType->addItem(I18n::tr(b.key), QString::fromLatin1(b.code));
    m_bottomC = new QDoubleSpinBox(sb);
    m_bottomC->setRange(1000, 6000);
    m_bottomRho = new QDoubleSpinBox(sb);
    m_bottomRho->setRange(1000, 4000);
    sb->form()->addRow(I18n::tr("uw_bottom_type"), m_bottomType);
    sb->form()->addRow(I18n::tr("uw_bottom_c"), m_bottomC);
    sb->form()->addRow(I18n::tr("uw_bottom_rho"), m_bottomRho);
    // 底質の吸収係数 α [dB/λ] — .ofdx "bottom_alpha_db_lambda" へ永続化し、
    // BellhopIO が底質ハーフスペース行の減衰として .env へ出力する (配線済み)。
    m_bottomAlpha = makeSpin(sb, 0.0, 20.0, 0.5, 2);
    sb->form()->addRow(I18n::tr("uwx_bottom_alpha"), m_bottomAlpha);
    v->addWidget(sb);

    // 海面 / Sea surface (mock 追加分) — すべてローカル状態
    auto *ssf = new SectionBox(I18n::tr("uwx_surface_section"), body);
    m_waveHeight = makeSpin(ssf, 0.0, 30.0, 1.5, 1);
    ssf->form()->addRow(I18n::tr("uwx_wave_height"), m_waveHeight);
    m_surfSpecular = makeCheck(I18n::tr("uwx_specular"), false, ssf);
    m_surfBragg    = makeCheck(I18n::tr("uwx_bragg"), true, ssf);
    auto *surfRow = new QHBoxLayout();
    surfRow->addWidget(m_surfSpecular);
    surfRow->addWidget(m_surfBragg);
    surfRow->addStretch(1);
    ssf->vbox()->addLayout(surfRow);
    // 海面の設定はすべてローカル状態 (apply() 非対象)
    ssf->vbox()->addWidget(tabhelp::unwiredNote(ssf));
    v->addWidget(ssf);

    // sonar
    auto *ss = new SectionBox(I18n::tr("uw_sonar"), body);
    m_sonarFreq = new QDoubleSpinBox(ss);
    m_sonarFreq->setRange(0.01, 1000);
    m_sonarSL = new QDoubleSpinBox(ss);
    m_sonarSL->setRange(0, 300);
    m_rangeMax = new QDoubleSpinBox(ss);
    m_rangeMax->setRange(0.1, 10000);
    ss->form()->addRow(I18n::tr("uw_freq"), m_sonarFreq);
    // 指向性 / ビーム幅 (mock 追加分) — ローカル状態
    m_sonarDir = makeSeg(ss, { I18n::tr("uwx_dir_omni"), I18n::tr("uwx_dir_dir"),
                               I18n::tr("uwx_dir_array") }, 1);
    ss->form()->addRow(I18n::tr("uwx_sonar_dir"), m_sonarDir);
    m_beamWidth = makeSpin(ss, 1.0, 180.0, 15.0, 0);
    ss->form()->addRow(I18n::tr("uwx_beam_width"), m_beamWidth);
    // 直上の指向性・ビーム幅のみローカル状態 (周波数・SL・距離は apply() 済み)
    ss->form()->addRow(tabhelp::unwiredNote(ss));
    ss->form()->addRow(I18n::tr("uw_sl"), m_sonarSL);
    ss->form()->addRow(I18n::tr("uw_range"), m_rangeMax);
    v->addWidget(ss);

    // 伝搬損失 (TL) (mock 追加分) — 損失項はローカル状態、
    // 距離の上限のみ既存の rangeMax_km と同期する。
    auto *st = new SectionBox(I18n::tr("uwx_tl_section"), body);
    m_tlSpread  = makeCheck(I18n::tr("uwx_tl_spread"), true, st);
    m_tlAbsorb  = makeCheck(I18n::tr("uwx_tl_absorb"), true, st);
    m_tlScatter = makeCheck(I18n::tr("uwx_tl_scatter"), false, st);
    m_tlSurface = makeCheck(I18n::tr("uwx_tl_surface"), false, st);
    auto *loss1 = new QHBoxLayout();
    loss1->addWidget(m_tlSpread);
    loss1->addWidget(m_tlAbsorb);
    loss1->addStretch(1);
    st->vbox()->addLayout(loss1);
    auto *loss2 = new QHBoxLayout();
    loss2->addWidget(m_tlScatter);
    loss2->addWidget(m_tlSurface);
    loss2->addStretch(1);
    st->vbox()->addLayout(loss2);
    // 損失項の選択と距離下限はローカル状態 (距離上限のみ「最大距離」と同期)
    st->vbox()->addWidget(tabhelp::unwiredNote(st));
    // 上限は既存の m_rangeMax と同じ刻み (decimals) にして値ズレを防ぐ
    m_tlRangeMin = makeSpin(st, 0.0, 10000.0, 0.0, 2);
    m_tlRangeMax = makeSpin(st, 0.1, 10000.0, 50.0, 2);
    auto *rangeRow = new QHBoxLayout();
    rangeRow->addWidget(m_tlRangeMin);
    rangeRow->addWidget(new QLabel(QStringLiteral("~"), st));
    rangeRow->addWidget(m_tlRangeMax);
    rangeRow->addStretch(1);
    st->form()->addRow(I18n::tr("uwx_tl_range"), rangeRow);
    v->addWidget(st);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    auto applyCb = [this] { apply(); };
    for (auto *s : { m_temp, m_salinity, m_bottomC, m_bottomRho, m_bottomAlpha,
                     m_sonarFreq, m_sonarSL, m_rangeMax })
        connect(s, &QDoubleSpinBox::valueChanged, this, applyCb);
    connect(m_sofar, &QCheckBox::toggled, this, applyCb);
    connect(m_bottomType, &QComboBox::currentIndexChanged, this, applyCb);

    // ソルバー選択はローカル状態 (Project 非永続) → apply() は呼ばない
    connect(m_solver, &QComboBox::currentIndexChanged,
            this, &UnderwaterTab::updateSolverView);
    updateSolverView();

    // TL セクションの距離上限は既存の「最大距離」と同じ値 (二重編集を避ける)
    connect(m_tlRangeMax, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        if (m_updating) return;
        QSignalBlocker block(m_rangeMax);
        m_rangeMax->setValue(val);
        apply();
    });

    connect(add, &QPushButton::clicked, this, [this] {
        auto &ssp = m_p->underwater().ssp;
        const double depth = ssp.isEmpty() ? 0 : ssp.last().depth_m + 500;
        ssp.push_back({ depth, 1500 });
        refresh();
        m_p->touch();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        auto &ssp = m_p->underwater().ssp;
        const int r = m_ssp->currentRow();
        if (r >= 0 && r < ssp.size()) {
            ssp.removeAt(r);
            refresh();
            m_p->touch();
        }
    });
    connect(m_ssp, &QTableWidget::cellChanged, this, [this] {
        if (!m_updating) { applySsp(); updateDerived(); m_p->touch(); }
    });

    connect(project, &Project::loaded, this, &UnderwaterTab::refresh);
    refresh();
}

void UnderwaterTab::apply()
{
    if (m_updating) return;
    UnderwaterOpts &u = m_p->underwater();
    u.waterTemp_C = m_temp->value();
    u.salinity_psu = m_salinity->value();
    u.sofar = m_sofar->isChecked();
    u.bottomType = m_bottomType->currentData().toString();   // 表示名ではなくコード
    u.bottomC_mps = m_bottomC->value();
    u.bottomRho_kgm3 = m_bottomRho->value();
    u.bottomAlpha_dBlambda = m_bottomAlpha->value();
    u.sonarFreq_kHz = m_sonarFreq->value();
    u.sonarSL_dB = m_sonarSL->value();
    u.rangeMax_km = m_rangeMax->value();
    if (m_tlRangeMax->value() != u.rangeMax_km) {
        QSignalBlocker block(m_tlRangeMax);
        m_tlRangeMax->setValue(u.rangeMax_km);
    }
    updateDerived();
    m_p->touch();
}

void UnderwaterTab::applySsp()
{
    auto &ssp = m_p->underwater().ssp;
    for (int r = 0; r < m_ssp->rowCount() && r < ssp.size(); ++r) {
        if (auto *it = m_ssp->item(r, 0)) ssp[r].depth_m = it->text().toDouble();
        if (auto *it = m_ssp->item(r, 1)) ssp[r].c_mps = it->text().toDouble();
    }
}

// ソルバー切替 → 説明文と条件付きパラメータの表示 (mock の solver === … 分岐)
void UnderwaterTab::updateSolverView()
{
    static const char *kDesc[4] = { "uwx_sv_desc_fdtd", "uwx_sv_desc_bellhop",
                                    "uwx_sv_desc_pe", "uwx_sv_desc_hybrid" };
    const int i = qBound(0, m_solver->currentIndex(), 3);
    m_solverDesc->setText(I18n::tr(kDesc[i]));
    m_bellhopPanel->setVisible(i == 1);
    m_pePanel->setVisible(i == 2);
}

// 基準音速 c₀ / SOFAR チャネル深度 / SSP プロファイル図の再計算
void UnderwaterTab::updateDerived()
{
    const UnderwaterOpts &u = m_p->underwater();
    m_c0->setText(QStringLiteral("%1 m/s")
        .arg(refSoundSpeed(u.waterTemp_C, u.salinity_psu), 0, 'f', 1));

    QVector<QPointF> pts;
    int cMinIdx = -1;
    for (int i = 0; i < u.ssp.size(); ++i) {
        pts.push_back(QPointF(u.ssp[i].c_mps, u.ssp[i].depth_m));
        if (cMinIdx < 0 || u.ssp[i].c_mps < u.ssp[cMinIdx].c_mps) cMinIdx = i;
    }
    m_sspPlot->setProfile(pts);

    // SOFAR チャネル軸 = 音速最小層の深度
    if (cMinIdx >= 0) {
        m_sofarHint->setText(I18n::tr("uwx_sofar_depth")
            .arg(qRound(u.ssp[cMinIdx].depth_m)));
        m_sofarHint->show();
    } else {
        m_sofarHint->hide();
    }
}

void UnderwaterTab::refresh()
{
    m_updating = true;
    const UnderwaterOpts &u = m_p->underwater();
    m_temp->setValue(u.waterTemp_C);
    m_salinity->setValue(u.salinity_psu);
    m_sofar->setChecked(u.sofar);
    // .ofdx の底質コード → コンボ。未知コードは項目として足し、値を落とさない。
    int bi = m_bottomType->findData(u.bottomType);
    if (bi < 0 && !u.bottomType.isEmpty()) {
        m_bottomType->addItem(u.bottomType, u.bottomType);
        bi = m_bottomType->count() - 1;
    }
    m_bottomType->setCurrentIndex(bi < 0 ? 0 : bi);
    m_bottomC->setValue(u.bottomC_mps);
    m_bottomRho->setValue(u.bottomRho_kgm3);
    m_bottomAlpha->setValue(u.bottomAlpha_dBlambda);
    m_sonarFreq->setValue(u.sonarFreq_kHz);
    m_sonarSL->setValue(u.sonarSL_dB);
    m_rangeMax->setValue(u.rangeMax_km);
    m_tlRangeMax->setValue(u.rangeMax_km);

    m_ssp->setRowCount(u.ssp.size());
    for (int r = 0; r < u.ssp.size(); ++r) {
        m_ssp->setItem(r, 0, new QTableWidgetItem(
            QString::number(u.ssp[r].depth_m, 'g', 8)));
        m_ssp->setItem(r, 1, new QTableWidgetItem(
            QString::number(u.ssp[r].c_mps, 'g', 8)));
    }
    m_updating = false;
    updateDerived();
}
