// UltrasoundTab.cpp
#include "UltrasoundTab.h"
#include "TabHelpers.h"
#include "../acoustics/core/FocusedField.h"
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
              "HIFU の焦点音場 (焦点音圧・強度・MI・非線形の卓越度) は線形集束理論"
              "による実計算。時間領域の非線形伝搬 (Westervelt / KZK) と "
              "power-law 吸収を含む波動計算は未実装 — その他の設定は設計モック。",
              "MHz-band ultrasound propagation: medical imaging, HIFU therapy and "
              "NDT.\nThe HIFU focal field (pressure, intensity, MI, nonlinearity) "
              "is really computed from linear focusing theory. Time-domain "
              "nonlinear propagation (Westervelt / KZK) with power-law absorption "
              "is not implemented — the remaining settings are a design mock.");
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
    // 媒質表 — 文献値データベースであることと、材料への反映
    I18n::reg("us_h_y", "y", "y");
    I18n::reg("us_h_z", "Z [MRayl]", "Z [MRayl]");
    I18n::reg("us_med_source",
              "▸ 文献値データベース (Duck 1990 / IT'IS Tissue Properties V4.1 / "
              "Krautkrämer 1990、B/A は Hamilton & Blackstock 1998)。\n"
              "吸収は α(f) = α₀·f^y [dB/cm]、Z = ρc は表の値からの計算値。"
              "プロジェクトの材料とは独立です — 下のボタンで材料へ反映できます。",
              "▸ Literature database (Duck 1990 / IT'IS Tissue Properties V4.1 / "
              "Krautkrämer 1990; B/A from Hamilton & Blackstock 1998).\n"
              "Absorption follows α(f) = α₀·f^y [dB/cm]; Z = ρc is computed from "
              "the table. This is independent of the project materials — use the "
              "button below to apply a row to a material.");
    I18n::reg("us_apply_target", "反映先の材料", "Target material");
    I18n::reg("us_apply_btn", "選択した媒質を材料へ適用 (ρ, c)",
              "Apply the selected medium to the material (ρ, c)");
    I18n::reg("us_apply_none",
              "プロジェクトに材料がありません (材料タブで追加してください)",
              "The project has no materials (add one in the Materials tab)");
    I18n::reg("us_apply_done",
              "材料 %1 に ρ = %2 kg/m³, c = %3 m/s を設定しました",
              "Set ρ = %2 kg/m³, c = %3 m/s on material %1");
    // HIFU の焦点音場 (実計算)
    I18n::reg("us_focus_section", "焦点音場 (線形集束理論による実計算)",
              "Focal field (computed from linear focusing theory)");
    I18n::reg("us_aperture", "開口径 (直径 2a)", "Aperture diameter (2a)");
    I18n::reg("us_focal_len", "曲率半径 R (幾何焦点距離)",
              "Radius of curvature R (geometric focal length)");
    I18n::reg("us_med_used", "評価に使う媒質", "Medium used");
    I18n::reg("us_focal_i", "焦点強度 I", "Focal intensity I");
    I18n::reg("us_focal_gain", "焦点音圧利得 kh", "Focal pressure gain kh");
    I18n::reg("us_atten", "音源→焦点 減衰", "Source→focus attenuation");
    I18n::reg("us_beamw", "焦点 −6 dB 幅 (強度)",
              "Focal −6 dB width (intensity)");
    I18n::reg("us_mi", "機械指標 MI", "Mechanical index MI");
    I18n::reg("us_shock", "衝撃形成距離 x_sh", "Shock formation distance x_sh");
    I18n::reg("us_goldberg", "Gol'dberg 数 Γ", "Gol'dberg number Γ");
    I18n::reg("us_regime_lin", "準線形 (Γ ≤ 0.1)", "Quasi-linear (Γ ≤ 0.1)");
    I18n::reg("us_regime_trans", "遷移域 (0.1 < Γ < 10)",
              "Transitional (0.1 < Γ < 10)");
    I18n::reg("us_regime_shock", "非線形域 (Γ ≥ 10 — 衝撃形成)",
              "Nonlinear (Γ ≥ 10 — shock formation)");
    I18n::reg("us_regime_unknown", "非線形域 判定不可 (B/A 不明)",
              "Nonlinear regime unknown (B/A not available)");
    I18n::reg("us_hifu_note",
              "▸ 線形集束理論の実計算: 球面集束開口の Rayleigh 積分の軸上閉形式 "
              "(O'Neil 1949) に開口径・曲率半径・周波数・音響出力・媒質を代入。\n"
              "放射インピーダンスは ρc、吸収は片道の単純減衰と仮定。"
              "非線形飽和は含まないため、Γ ≫ 1 では実際の焦点音圧はこれより低い。\n"
              "MI は IEC 62359 の定義 (0.3 dB/cm/MHz デレーティング、正弦波を仮定)。"
              "アレイの素子分割・グレーティングローブは未考慮。",
              "▸ Real calculation from linear focusing theory: the on-axis "
              "closed form of the Rayleigh integral for a spherical bowl "
              "(O'Neil 1949), evaluated with the aperture, radius of curvature, "
              "frequency, acoustic power and medium.\n"
              "Radiation impedance is assumed to be ρc and absorption a simple "
              "one-way decay. Nonlinear saturation is not included, so for "
              "Γ ≫ 1 the true focal pressure is lower than shown.\n"
              "MI follows the IEC 62359 definition (0.3 dB/cm/MHz derating, "
              "sinusoidal waveform assumed). Array element discretisation and "
              "grating lobes are not modelled.");
    I18n::reg("us_hifu_bad",
              "⚠ 入力が不正です (0 < 開口半径 < 曲率半径、周波数 > 0、出力 ≥ 0)",
              "⚠ Invalid input (0 < aperture radius < radius of curvature, "
              "frequency > 0, power ≥ 0)");
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
    I18n::reg("us_uw_xdcr", "トランスデューサの設定",
              "the transducer settings");
    I18n::reg("us_uw_chk", "チェック状態",
              "the check boxes");
    I18n::reg("us_uw_bf", "ビームフォーミングの設定",
              "the beamforming settings");
    I18n::reg("us_uw_out", "出力のチェック",
              "the output check boxes");
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

// 媒質表は文献値データベース (src/acoustics/core/FocusedField) を表示する。
// GUI 側は id → 表示名キーの対応だけを持つ (数値は core 側が出典付きで保持)。
const char *mediumLabelKey(const char *id, bool ndt)
{
    const QLatin1String s(id);
    if (s == QLatin1String("water"))       return ndt ? "us_mat_water_c"
                                                      : "us_mat_water";
    if (s == QLatin1String("soft_tissue")) return "us_mat_soft";
    if (s == QLatin1String("fat"))         return "us_mat_fat";
    if (s == QLatin1String("liver"))       return "us_mat_liver";
    if (s == QLatin1String("bone"))        return "us_mat_bone";
    if (s == QLatin1String("steel_long"))  return "us_mat_steel_l";
    if (s == QLatin1String("steel_shear")) return "us_mat_steel_s";
    if (s == QLatin1String("cfrp"))        return "us_mat_cfrp";
    return id;
}
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
    st->vbox()->addWidget(tabhelp::unwiredNote(st, I18n::tr("us_uw_xdcr")));
    v->addWidget(st);

    // ── 焦点音場 (HIFU のみ / 実計算) ───────────────────────────────────────
    m_focusSection = buildFocusSection(body);
    v->addWidget(m_focusSection);

    // ── 媒質 (文献値データベース + プロジェクト材料への反映) ────────────────
    auto *sd = new SectionBox(I18n::tr("us_med_section"), body);
    m_medTable = makeTable({ I18n::tr("us_h_tissue"), "c [m/s]",
                             QString::fromUtf8("ρ [kg/m³]"),
                             QString::fromUtf8("α₀ [dB/cm/MHz^y]"),
                             I18n::tr("us_h_y"), "B/A", I18n::tr("us_h_z") },
                           5, sd, 170);
    m_medTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_medTable->setSelectionMode(QAbstractItemView::SingleSelection);
    sd->vbox()->addWidget(m_medTable);
    // 「サンプル」ではなく出典付きの文献値データベースであることを明示する
    auto *medNote = makeHint(I18n::tr("us_med_source"), sd);
    medNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sd->vbox()->addWidget(medNote);
    // 選択した媒質を Project の材料 (ρ, c) へ反映する導線
    m_matTarget = new QComboBox(sd);
    m_applyMedium = new QPushButton(I18n::tr("us_apply_btn"), sd);
    auto *applyRow = new QHBoxLayout();
    applyRow->addWidget(m_matTarget);
    applyRow->addWidget(m_applyMedium);
    applyRow->addStretch(1);
    sd->form()->addRow(I18n::tr("us_apply_target"), applyRow);
    m_applyStatus = makeHint(QString(), sd);
    sd->vbox()->addWidget(m_applyStatus);
    m_powerLaw  = makeCheck(I18n::tr("us_powerlaw"),  true,  sd);
    m_nonlinear = makeCheck(I18n::tr("us_nonlinear"), false, sd);
    sd->vbox()->addLayout(checkRow({ m_powerLaw, m_nonlinear }));
    // チェック状態はどこにも読まれない
    sd->vbox()->addWidget(tabhelp::unwiredNote(sd, I18n::tr("us_uw_chk")));
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
    sb->form()->addRow(tabhelp::unwiredNote(sb, I18n::tr("us_uw_bf")));
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
    so->vbox()->addWidget(tabhelp::unwiredNote(so, I18n::tr("us_uw_out")));
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
    // 媒質の選択は焦点音場の実計算に効く
    connect(m_medTable, &QTableWidget::itemSelectionChanged, this,
            &UltrasoundTab::updateHifu);
    connect(m_applyMedium, &QPushButton::clicked, this,
            &UltrasoundTab::applyMediumToMaterial);
    connect(project, &Project::loaded, this, &UltrasoundTab::refresh);
    connect(project, &Project::materialsEdited, this,
            &UltrasoundTab::refreshMaterials);
    refresh();
}

void UltrasoundTab::refresh()
{
    m_updating = true;
    m_app->setCurrentIndex(m_appIdx);
    m_updating = false;
    refreshMaterials();
    onAppChanged();
}

void UltrasoundTab::onAppChanged()
{
    const bool ndt  = (m_appIdx == 2);
    const bool hifu = (m_appIdx == 1);
    m_transStack->setCurrentIndex(m_appIdx);
    m_focusSection->setVisible(hifu);   // 焦点音場の実計算は HIFU のみ
    m_outStack->setCurrentIndex(m_appIdx);
    fillMediumTable(ndt);
    // mock: <Check label="非線形 (B/A)" checked={app==="hifu"} />
    m_nonlinear->setChecked(hifu);
    updateHifu();      // 媒質が変わったので焦点音場を再計算
}

// 文献値データベース (src/acoustics/core/FocusedField) を表示する。
// Z = ρc は表の値からの計算値。B/A が不明な材料は「—」。
void UltrasoundTab::fillMediumTable(bool ndt)
{
    using namespace ofd::acoustics::ultrasound;
    const int n = ndt ? ndtMediumCount() : bioMediumCount();
    m_medTable->clearContents();
    m_medTable->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const MediumEntry &e = ndt ? ndtMedium(i) : bioMedium(i);
        const Medium &m = e.medium;
        m_medTable->setItem(i, 0, textItem(I18n::tr(mediumLabelKey(e.id, ndt))));
        m_medTable->setItem(i, 1, numItem(QString::number(m.c, 'g', 5)));
        m_medTable->setItem(i, 2, numItem(QString::number(m.rho, 'g', 5)));
        m_medTable->setItem(i, 3,
                            numItem(QString::number(m.alpha0_dBcmMHz, 'g', 3)));
        m_medTable->setItem(i, 4,
                            numItem(QString::number(m.alphaExponent, 'g', 3)));
        m_medTable->setItem(i, 5,
                            numItem(m.bOverA >= 0.0
                                        ? QString::number(m.bOverA, 'g', 3)
                                        : QString::fromUtf8("—")));
        m_medTable->setItem(i, 6,
                            numItem(QString::number(
                                acousticImpedance(m) * 1.0e-6, 'f', 2)));
    }
    if (m_medTable->currentRow() < 0 || m_medTable->currentRow() >= n)
        m_medTable->selectRow(0);
}

// ── 材料一覧の再構築 (Project::materials) ──────────────────────────────────
void UltrasoundTab::refreshMaterials()
{
    const QVector<Material> &mats = m_p->materials();
    const int keep = m_matTarget->currentIndex();
    m_matTarget->clear();
    for (int i = 0; i < mats.size(); ++i) {
        // 表示 ID は 0=真空 / 1=PEC の次から (MaterialTab と同じ流儀)
        const QString name = mats[i].name.isEmpty()
                                 ? QString::number(i + 2)
                                 : QString("%1: %2").arg(i + 2).arg(mats[i].name);
        m_matTarget->addItem(name);
    }
    const bool has = !mats.isEmpty();
    m_matTarget->setEnabled(has);
    m_applyMedium->setEnabled(has);
    if (has && keep >= 0 && keep < mats.size())
        m_matTarget->setCurrentIndex(keep);
    if (!has) m_applyStatus->setText(I18n::tr("us_apply_none"));
}

// ── 選択した媒質 (文献値) を Project の材料 (ρ, c) へ反映 ──────────────────
void UltrasoundTab::applyMediumToMaterial()
{
    QVector<Material> &mats = m_p->materials();
    const int mi = m_matTarget->currentIndex();
    if (mi < 0 || mi >= mats.size()) {
        m_applyStatus->setText(I18n::tr("us_apply_none"));
        return;
    }
    const ofd::acoustics::ultrasound::Medium med = currentMedium();
    mats[mi].rho = med.rho;
    mats[mi].soundSpeed = med.c;
    m_p->touch();
    emit m_p->materialsEdited();
    m_applyStatus->setText(I18n::tr("us_apply_done")
                               .arg(m_matTarget->currentText())
                               .arg(QString::number(med.rho, 'g', 5))
                               .arg(QString::number(med.c, 'g', 5)));
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
    // 焦点音場の計算に必要な開口寸法 (既定は代表的な単一素子 HIFU トランスデューサ)
    m_hifuAperture = numEdit("64", page);
    fl->addRow(I18n::tr("us_aperture"), unitRow(m_hifuAperture, "mm", page));
    m_hifuFocal = numEdit("62.6", page);
    fl->addRow(I18n::tr("us_focal_len"), unitRow(m_hifuFocal, "mm", page));

    for (QLineEdit *e : { m_hifuFreq, m_hifuPower, m_hifuAperture, m_hifuFocal })
        connect(e, &QLineEdit::textChanged, this, &UltrasoundTab::updateHifu);
    return page;
}

// ── 焦点音場 (実計算) — HIFU のときだけ表示する独立セクション ──────────────
// 計算実体は src/acoustics/core/FocusedField (O'Neil 1949 の閉形式)。
QWidget *UltrasoundTab::buildFocusSection(QWidget *parent)
{
    auto *s = new SectionBox(I18n::tr("us_focus_section"), parent);

    m_hifuMedium = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_med_used"), m_hifuMedium);

    m_hifuPressure = makeMono(QString::fromUtf8("—"), s);
    auto *h = new QHBoxLayout();
    h->addWidget(m_hifuPressure);
    m_hifuRegime = makeBadge(I18n::tr("us_regime_unknown"), kWarn, s);
    h->addWidget(m_hifuRegime);
    h->addStretch(1);
    s->form()->addRow(I18n::tr("us_focal_p"), h);

    m_hifuIntensity = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_focal_i"), m_hifuIntensity);
    m_hifuGain = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_focal_gain"), m_hifuGain);
    m_hifuAtten = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_atten"), m_hifuAtten);
    m_hifuBeamWidth = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_beamw"), m_hifuBeamWidth);
    m_hifuMi = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_mi"), m_hifuMi);
    m_hifuShock = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_shock"), m_hifuShock);
    m_hifuGoldberg = makeMono(QString::fromUtf8("—"), s);
    s->form()->addRow(I18n::tr("us_goldberg"), m_hifuGoldberg);

    m_hifuNote = makeHint(I18n::tr("us_hifu_note"), s);
    m_hifuNote->setStyleSheet("font-size:11px; color:palette(mid);");
    s->vbox()->addWidget(m_hifuNote);
    return s;
}

// 媒質表で選択されている行の物性 (文献値データベース) を返す。
ofd::acoustics::ultrasound::Medium UltrasoundTab::currentMedium() const
{
    using namespace ofd::acoustics::ultrasound;
    const bool ndt = (m_appIdx == 2);
    const int n = ndt ? ndtMediumCount() : bioMediumCount();
    int row = 0;
    if (m_medTable && m_medTable->currentRow() >= 0)
        row = m_medTable->currentRow();
    if (row >= n) row = 0;
    return ndt ? ndtMedium(row).medium : bioMedium(row).medium;
}

// ── HIFU: 焦点音場の実計算 (入力 → O'Neil の閉形式 + MI / Gol'dberg 数) ─────
void UltrasoundTab::updateHifu()
{
    using namespace ofd::acoustics::ultrasound;
    if (!m_hifuPressure) return;

    bool okF = false, okW = false, okA = false, okR = false;
    const double fMHz  = m_hifuFreq->text().trimmed().toDouble(&okF);
    const double powerW = m_hifuPower->text().trimmed().toDouble(&okW);
    const double apMm  = m_hifuAperture->text().trimmed().toDouble(&okA);
    const double rMm   = m_hifuFocal->text().trimmed().toDouble(&okR);

    const Medium med = currentMedium();
    const bool ndt = (m_appIdx == 2);
    const int row = (m_medTable && m_medTable->currentRow() >= 0)
                        ? m_medTable->currentRow() : 0;
    const char *id = ndt ? ndtMedium(row).id : bioMedium(row).id;
    m_hifuMedium->setText(I18n::tr(mediumLabelKey(id, ndt)));

    FocusedSource src;
    src.frequency_Hz = fMHz * 1.0e6;
    src.power_W = powerW;
    src.apertureRadius_m = 0.5 * apMm * 1.0e-3;
    src.focalLength_m = rMm * 1.0e-3;

    const QString dash = QString::fromUtf8("—");
    FocusedFieldResult r;
    if (okF && okW && okA && okR)
        r = evaluateFocus(src, med);
    if (!r.valid) {
        for (QLabel *l : { m_hifuPressure, m_hifuIntensity, m_hifuGain,
                           m_hifuAtten, m_hifuBeamWidth, m_hifuMi,
                           m_hifuShock, m_hifuGoldberg })
            l->setText(dash);
        m_hifuPressure->setText(I18n::tr("us_hifu_bad"));
        m_hifuRegime->setText(I18n::tr("us_regime_unknown"));
        return;
    }

    m_hifuPressure->setText(QString::fromUtf8("%1 MPa")
        .arg(QString::number(r.focalPressure_Pa * 1.0e-6, 'f', 2)));
    m_hifuIntensity->setText(QString::fromUtf8("%1 W/cm²  (%2 MW/m²)")
        .arg(QString::number(r.focalIntensity_Wm2 * 1.0e-4, 'f', 0))
        .arg(QString::number(r.focalIntensity_Wm2 * 1.0e-6, 'f', 2)));
    m_hifuGain->setText(QString::number(r.pressureGain, 'f', 1));
    m_hifuAtten->setText(QString::fromUtf8("%1 dB")
        .arg(QString::number(r.attenuation_dB, 'f', 2)));
    m_hifuBeamWidth->setText(QString::fromUtf8("%1 mm  (F# = %2)")
        .arg(QString::number(r.beamWidth6dB_m * 1.0e3, 'f', 2))
        .arg(QString::number(r.fNumber, 'f', 2)));
    m_hifuMi->setText(QString::number(r.mechanicalIndex, 'f', 2));

    if (r.nonlinearValid) {
        m_hifuShock->setText(QString::fromUtf8("%1 mm")
            .arg(QString::number(r.shockDistance_m * 1.0e3, 'f', 2)));
        m_hifuGoldberg->setText(r.goldberg < 0.0
            ? QString::fromUtf8("∞")
            : QString::number(r.goldberg, 'g', 3));
        const char *key = (r.regime == RegimeShock)        ? "us_regime_shock"
                          : (r.regime == RegimeTransitional) ? "us_regime_trans"
                                                             : "us_regime_lin";
        m_hifuRegime->setText(I18n::tr(key));
    } else {
        m_hifuShock->setText(dash);
        m_hifuGoldberg->setText(dash);
        m_hifuRegime->setText(I18n::tr("us_regime_unknown"));
    }
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
