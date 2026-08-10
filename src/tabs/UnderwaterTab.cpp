// UnderwaterTab.cpp
#include "UnderwaterTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../io/ShdReader.h"
#include "../io/ArrReader.h"
#include "../acoustics/core/AudioBuffer.h"
#include "../acoustics/io/WavWriter.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
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
    I18n::reg("uwx_rays_auto", "自動", "auto");
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
    // TL 断面 (.shd の可視化)
    I18n::reg("uwx_tl_view", "TL 断面 (計算結果)", "TL section (run result)");
    I18n::reg("uwx_tl_none", "未計算 — 「計算」を実行すると表示されます",
              "Not computed yet — run the solver to see it here");
    I18n::reg("uwx_tl_ok",
              "%1 × %2 点、TL %3〜%4 dB (TL = -20 log10|p|、小さいほど"
              "よく届く)。出所: %5",
              "%1 x %2 points, TL %3-%4 dB (TL = -20 log10|p|; smaller means "
              "better reach). Source: %5");
    I18n::reg("uwx_tl_err", "TL 断面を読めませんでした: %1",
              "Could not read the TL section: %1");
    // 受信インパルス応答 (.arr → IR → WAV)
    I18n::reg("uwx_ir_sec", "受信波形 (インパルス応答)",
              "Received waveform (impulse response)");
    I18n::reg("uwx_ir_depth", "受波器 深度", "Receiver depth");
    I18n::reg("uwx_ir_range", "受波器 距離", "Receiver range");
    I18n::reg("uwx_ir_fs", "書き出す fs", "Export sample rate");
    I18n::reg("uwx_ir_export", "💾 受信波形を WAV で書き出す",
              "\U0001f4be Export the received waveform as WAV");
    I18n::reg("uwx_ir_hint",
              "「計算モード = 到達時間」で実行すると、到達ごとの振幅・位相・"
              "遅延から受信インパルス応答を合成できます。書き出した WAV は"
              "「音響編集・解析」の畳み込み (リバーブ) にそのまま使えます。",
              "Running with \u201cRun mode = Arrivals\u201d lets the received impulse "
              "response be synthesised from each arrival's amplitude, phase "
              "and delay. The exported WAV can be used directly by the "
              "convolution (reverb) in Audio editing / analysis.");
    I18n::reg("uwx_ir_narrow",
              "▸ 到達の振幅は %1 Hz 1 波数で計算された値です。合成した波形が"
              "正しいのは「その周波数の近傍だけ」で、広帯域の音を作るには"
              "周波数ごとに実行して合成する必要があります (未実装)。",
              "\u25b8 Arrival amplitudes are computed at the single frequency "
              "%1 Hz, so the synthesised waveform is only valid near that "
              "frequency. Broadband audio needs a run per frequency and a "
              "combination step (not implemented).");
    I18n::reg("uwx_ir_none",
              "到達ファイル (.arr) がありません — 「計算モード」を"
              "「到達時間」にして計算すると作れます。",
              "There is no arrival file (.arr) \u2014 set Run mode to Arrivals and "
              "run to produce one.");
    I18n::reg("uwx_ir_ready", "%1 (%2 Hz、受波器 %3×%4 点)",
              "%1 (%2 Hz, %3 x %4 receivers)");
    I18n::reg("uwx_ir_ok",
              "書き出しました: %1 — 到達 %2 本、%3 サンプル (%4 s @ %5 Hz)、"
              "直接波 %6 s、最後の到達 %7 s、ピーク %8",
              "Exported %1 \u2014 %2 arrivals, %3 samples (%4 s @ %5 Hz), direct "
              "arrival %6 s, last arrival %7 s, peak %8");
    I18n::reg("uwx_ir_empty",
              "この受波器には到達がありません (音が届いていない)。"
              "別の深度・距離を選んでください。",
              "No arrivals reach this receiver. Pick another depth or range.");
    I18n::reg("uwx_ir_err", "受信波形を作れませんでした: %1",
              "Could not build the received waveform: %1");
    I18n::reg("uwx_ir_norm",
              " ※ WAV はピークで正規化しています (絶対音圧ではありません)",
              " Note: the WAV is peak-normalised (it is not an absolute "
              "pressure level)");
    I18n::reg("uwx_uw_solver", "ソルバーの選択と BELLHOP / PE のパラメータ",
              "the solver selection and the BELLHOP / PE parameters");
    I18n::reg("uwx_uw_surface", "海面の設定",
              "the sea-surface settings");
    I18n::reg("uwx_uw_beam", "直上の指向性とビーム幅",
              "the directivity and beam width just above");
    I18n::reg("uwx_uw_beam_ok", "周波数・音源レベル・距離",
              "the frequency, source level and range");
    I18n::reg("uwx_uw_loss", "損失項の選択と距離の下限",
              "the loss-term selection and the lower range bound");
    I18n::reg("uwx_uw_loss_ok", "距離の上限 (「最大距離」と同期します)",
              "the upper range bound (kept in sync with the maximum range)");
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

// ── UwTlView — TL 断面 (.shd) のヒートマップ ────────────────────────────────
UwTlView::UwTlView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 190);
}

void UwTlView::clear()
{
    m_tl.clear();
    m_nz = m_nr = 0;
    m_caption.clear();
    update();
}

void UwTlView::setField(const ShdField &f, double rangeMax_km,
                        double depthMax_m, const QString &caption)
{
    m_tl = f.tl_dB;
    m_nz = f.nrz;
    m_nr = f.nrr;
    m_lo = f.minTL;
    m_hi = f.maxTL;
    m_rangeMax = rangeMax_km;
    m_depthMax = depthMax_m;
    m_caption = caption;
    update();
}

void UwTlView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);
    if (m_nz < 2 || m_nr < 2) {
        p.setPen(palette().mid().color());
        p.drawText(rect(), Qt::AlignCenter, I18n::tr("uwx_tl_none"));
        return;
    }
    const QRectF plot(40, 8, width() - 52, height() - 34);
    // 表示レンジは有効値の下端から 60 dB (慣用の TL カラースケール幅)
    const double lo = m_lo, hi = qMin(m_hi, m_lo + 60.0);
    for (int y = 0; y < int(plot.height()); ++y) {
        const int iz = qBound(0, int(double(y) / plot.height() * m_nz), m_nz - 1);
        for (int x = 0; x < int(plot.width()); ++x) {
            const int ir = qBound(0, int(double(x) / plot.width() * m_nr), m_nr - 1);
            const float tl = m_tl[qsizetype(iz) * m_nr + ir];
            QColor c;
            if (tl >= ShdField::kNoField) {
                c = palette().base().color();      // 到達なし
            } else {
                // TL が小さい (よく届く) ほど暖色。jet 風の 4 区間
                const double t = qBound(0.0, (hi - tl) / qMax(1.0, hi - lo), 1.0);
                c = QColor::fromHsvF((1.0 - t) * 0.66, 0.85, 0.35 + 0.6 * t);
            }
            p.setPen(c);
            p.drawPoint(int(plot.left()) + x, int(plot.top()) + y);
        }
    }
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(plot);
    p.setPen(palette().text().color());
    p.drawText(QPointF(4, plot.top() + 8), QStringLiteral("0 m"));
    p.drawText(QPointF(4, plot.bottom()),
               QStringLiteral("%1 m").arg(m_depthMax, 0, 'f', 0));
    p.drawText(QPointF(plot.left(), height() - 4), QStringLiteral("0 km"));
    p.drawText(QRectF(plot.left(), height() - 14, plot.width(), 12),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 km").arg(m_rangeMax, 0, 'f', 1));
    p.setPen(palette().mid().color());
    p.drawText(QRectF(plot.left(), 0, plot.width(), 10),
               Qt::AlignRight | Qt::AlignVCenter, m_caption);
}

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
    // 0 = NBEAMS 自動 (カーネルが決める) — 従来の .env と同じ既定
    m_numRays->setRange(0, 10000000);
    m_numRays->setSpecialValueText(I18n::tr("uwx_rays_auto"));
    m_numRays->setValue(0);
    m_numRays->setMaximumWidth(120);
    bhForm->addRow(I18n::tr("uwx_num_rays"), m_numRays);
    m_beamType = makeSeg(m_bellhopPanel, { I18n::tr("uwx_beam_geom"),
                                           I18n::tr("uwx_beam_gauss"),
                                           I18n::tr("uwx_beam_hat") }, 0);
    bhForm->addRow(I18n::tr("uwx_beam_type"), m_beamType);
    m_angMin = makeSpin(m_bellhopPanel, -90, 90, -45, 0);
    m_angMax = makeSpin(m_bellhopPanel, -90, 90, 45, 0);
    auto *angRow = new QHBoxLayout();
    angRow->addWidget(m_angMin);
    angRow->addWidget(new QLabel(QStringLiteral("~"), m_bellhopPanel));
    angRow->addWidget(m_angMax);
    angRow->addStretch(1);
    bhForm->addRow(I18n::tr("uwx_angle_range"), angRow);
    m_calcMode = makeSeg(m_bellhopPanel, { I18n::tr("uwx_mode_eigen"),
                                           I18n::tr("uwx_mode_coher"),
                                           I18n::tr("uwx_mode_incoh"),
                                           I18n::tr("uwx_mode_arr") }, 1);
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

    // TL 断面 (計算結果) — .shd を読めたときだけ中身が入る
    auto *tlBox = new SectionBox(I18n::tr("uwx_tl_view"), sv);
    m_tlView = new UwTlView(tlBox);
    tlBox->vbox()->addWidget(m_tlView);
    m_tlNote = mutedLabel(I18n::tr("uwx_tl_none"), tlBox);
    tlBox->vbox()->addWidget(m_tlNote);
    sv->vbox()->addWidget(tlBox);

    // 受信波形 (到達 → インパルス応答 → WAV)。到達ファイルが無い間は
    // 「未計算」ではなく **作り方** を出す (何をすれば出るかが分かるように)
    {
        auto *irBox = new SectionBox(I18n::tr("uwx_ir_sec"), sv);
        m_irBox = irBox;
        auto *hint = mutedLabel(I18n::tr("uwx_ir_hint"), irBox);
        irBox->vbox()->addWidget(hint);
        auto *f = new QFormLayout();
        f->setContentsMargins(0, 0, 0, 0);
        f->setHorizontalSpacing(8);
        f->setVerticalSpacing(4);
        m_irDepth = new QComboBox(irBox);
        m_irRange = new QComboBox(irBox);
        m_irFs = new QComboBox(irBox);
        for (const int fs : { 48000, 44100, 96000, 24000, 8000 })
            m_irFs->addItem(QStringLiteral("%1 Hz").arg(fs), fs);
        f->addRow(I18n::tr("uwx_ir_depth"), m_irDepth);
        f->addRow(I18n::tr("uwx_ir_range"), m_irRange);
        f->addRow(I18n::tr("uwx_ir_fs"), m_irFs);
        irBox->vbox()->addLayout(f);
        m_irExport = new QPushButton(I18n::tr("uwx_ir_export"), irBox);
        m_irExport->setEnabled(false);
        irBox->vbox()->addWidget(m_irExport);
        m_irNote = mutedLabel(I18n::tr("uwx_ir_none"), irBox);
        irBox->vbox()->addWidget(m_irNote);
        connect(m_irExport, &QPushButton::clicked,
                this, &UnderwaterTab::exportReceivedIr);
        sv->vbox()->addWidget(irBox);
    }

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
    sv->vbox()->addWidget(tabhelp::unwiredNote(sv, I18n::tr("uwx_uw_solver")));
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
    ssf->vbox()->addWidget(tabhelp::unwiredNote(ssf, I18n::tr("uwx_uw_surface")));
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
    ss->form()->addRow(tabhelp::unwiredNote(ss, I18n::tr("uwx_uw_beam"), I18n::tr("uwx_uw_beam_ok")));
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
    st->vbox()->addWidget(tabhelp::unwiredNote(st, I18n::tr("uwx_uw_loss"), I18n::tr("uwx_uw_loss_ok")));
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

    // Bellhop 実行設定は .ofdx へ永続化し .env へ渡る (旧: UI だけで
    // どこにも繋がっておらず、変更しても計算内容が変わらなかった)
    for (auto *c : { m_beamType, m_calcMode })
        connect(c, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_numRays, &QSpinBox::valueChanged, this, applyCb);
    for (auto *s : { m_angMin, m_angMax })
        connect(s, &QDoubleSpinBox::valueChanged, this, applyCb);

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
    // Bellhop 実行設定 (.env の RunType / NBEAMS / ALPHA へ渡る)。
    // 既定値は従来 BellhopIO がハードコードしていた挙動と同じ:
    // RunType 'CG' (コヒーレント TL + 幾何 hat ビーム)、NBEAMS 0、±45°。
    static const char *kModes[4] = { "eigenray", "coherent", "incoherent",
                                     "arrivals" };
    static const char *kBeams[3] = { "geometric", "gaussian", "hat" };
    u.runMode = QString::fromLatin1(kModes[qBound(0, m_calcMode->currentIndex(), 3)]);
    u.beamType = QString::fromLatin1(kBeams[qBound(0, m_beamType->currentIndex(), 2)]);
    u.numRays = m_numRays->value();
    u.angleMin_deg = m_angMin->value();
    u.angleMax_deg = m_angMax->value();
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

// 実行完了時 (MainWindow から) — <ケース名>.shd を読んで TL 断面を出す
void UnderwaterTab::showTlResult(const QString &workingDir,
                                 const QString &caseName)
{
    if (!m_tlView || !m_tlNote) return;
    const QString path = workingDir + QLatin1Char('/') + caseName + ".shd";
    ShdField field;
    QString err;
    if (!ShdReader::read(path, field, &err)) {
        m_tlView->clear();
        m_tlNote->setText(I18n::tr("uwx_tl_err").arg(err));
        return;
    }
    const UnderwaterOpts &u = m_p->underwater();
    double depthMax = 0.0;
    for (const SSPPoint &s : u.ssp) depthMax = qMax(depthMax, s.depth_m);
    for (const BathyPoint &b : u.bathymetry) depthMax = qMax(depthMax, b.depth_m);
    const QString src = u.bathymetry.isEmpty()
                            ? QStringLiteral("flat bottom")
                            : u.bathySource;
    m_tlView->setField(field, u.rangeMax_km, depthMax, field.plotType);
    m_tlNote->setText(I18n::tr("uwx_tl_ok")
                          .arg(field.nrz).arg(field.nrr)
                          .arg(field.minTL, 0, 'f', 1)
                          .arg(field.maxTL, 0, 'f', 1)
                          .arg(src));
}

// 実行完了時 — <ケース名>.arr があれば受波器の一覧を作る
void UnderwaterTab::showArrivalResult(const QString &workingDir,
                                      const QString &caseName)
{
    if (!m_irNote || !m_irDepth || !m_irRange || !m_irExport) return;
    const QString path = workingDir + QLatin1Char('/') + caseName + ".arr";
    m_arrPath.clear();
    m_irDepth->clear();
    m_irRange->clear();
    m_irExport->setEnabled(false);
    if (!QFileInfo::exists(path)) {
        m_irNote->setText(I18n::tr("uwx_ir_none"));
        return;
    }
    ArrHeader h;
    QString err;
    if (!ArrReader::readHeader(path, h, &err)) {
        m_irNote->setText(I18n::tr("uwx_ir_err").arg(err));
        return;
    }
    m_arrPath = path;
    for (int i = 0; i < h.rz.size(); ++i)
        m_irDepth->addItem(QStringLiteral("%1 m").arg(h.rz[i], 0, 'f', 1), i);
    for (int i = 0; i < h.rr.size(); ++i)
        m_irRange->addItem(QStringLiteral("%1 km").arg(h.rr[i] / 1000.0, 0, 'f', 3), i);
    // 既定は最遠・音源に近い深度 (いちばん「聴きたい」点)
    if (m_irRange->count() > 0) m_irRange->setCurrentIndex(m_irRange->count() - 1);
    if (!h.sz.isEmpty() && !h.rz.isEmpty()) {
        int best = 0;
        for (int i = 1; i < h.rz.size(); ++i)
            if (std::fabs(h.rz[i] - h.sz[0]) < std::fabs(h.rz[best] - h.sz[0])) best = i;
        m_irDepth->setCurrentIndex(best);
    }
    m_irExport->setEnabled(true);
    m_irNote->setText(I18n::tr("uwx_ir_ready")
                          .arg(QFileInfo(path).fileName())
                          .arg(h.freqHz, 0, 'f', 1)
                          .arg(h.rz.size()).arg(h.rr.size())
                      + QStringLiteral("\n")
                      + I18n::tr("uwx_ir_narrow").arg(h.freqHz, 0, 'f', 1));
}

// 選んだ受波器の到達列 → インパルス応答 → WAV
void UnderwaterTab::exportReceivedIr()
{
    if (m_arrPath.isEmpty()) return;
    const int iz = m_irDepth->currentData().toInt();
    const int ir = m_irRange->currentData().toInt();
    const double fs = m_irFs->currentData().toDouble();

    ArrHeader h;
    QVector<ArrArrival> arrivals;
    QString err;
    if (!ArrReader::readArrivals(m_arrPath, iz, ir, h, arrivals, &err)) {
        m_irNote->setText(I18n::tr("uwx_ir_err").arg(err));
        return;
    }
    if (arrivals.isEmpty()) {
        m_irNote->setText(I18n::tr("uwx_ir_empty"));
        return;
    }
    IrSynthInfo info;
    const QVector<double> ir1 = synthesizeIr(arrivals, fs, 0.05, &info);
    if (ir1.isEmpty()) {
        m_irNote->setText(I18n::tr("uwx_ir_empty"));
        return;
    }
    // WAV はピーク正規化する (絶対音圧ではない — 注記を必ず出す)
    acoustics::AudioBuffer buf;
    buf.sampleRateHz = fs;
    buf.channels.resize(1);
    buf.channels[0].assign(ir1.begin(), ir1.end());
    if (info.peak > 0.0)
        for (double &v : buf.channels[0]) v /= info.peak;

    const QFileInfo fi(m_arrPath);
    const QString out = fi.path() + QLatin1Char('/') + fi.completeBaseName()
                        + QStringLiteral("_rx_z%1_r%2.wav").arg(iz).arg(ir);
    const auto res = acoustics::writeWavFile(out.toStdString(), buf,
                                             acoustics::WavSampleFormat::Float32);
    if (!res.success()) {
        m_irNote->setText(I18n::tr("uwx_ir_err")
                              .arg(QString::fromStdString(res.message())));
        return;
    }
    m_irNote->setText(I18n::tr("uwx_ir_ok")
                          .arg(QFileInfo(out).fileName())
                          .arg(info.arrivals)
                          .arg(info.length)
                          .arg(info.length / fs, 0, 'f', 3)
                          .arg(fs, 0, 'f', 0)
                          .arg(info.firstDelayS, 0, 'f', 4)
                          .arg(info.lastDelayS, 0, 'f', 4)
                          .arg(info.peak, 0, 'g', 4)
                      + I18n::tr("uwx_ir_norm")
                      + QStringLiteral("\n")
                      + I18n::tr("uwx_ir_narrow").arg(h.freqHz, 0, 'f', 1));
    emit receivedIrExported(out);
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
    {
        static const char *kModes[4] = { "eigenray", "coherent", "incoherent",
                                         "arrivals" };
        static const char *kBeams[3] = { "geometric", "gaussian", "hat" };
        for (int i = 0; i < 4; ++i)
            if (u.runMode == QLatin1String(kModes[i])) m_calcMode->setCurrentIndex(i);
        for (int i = 0; i < 3; ++i)
            if (u.beamType == QLatin1String(kBeams[i])) m_beamType->setCurrentIndex(i);
        m_numRays->setValue(u.numRays);
        m_angMin->setValue(u.angleMin_deg);
        m_angMax->setValue(u.angleMax_deg);
    }

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
