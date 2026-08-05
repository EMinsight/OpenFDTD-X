// CabinAcousticsTab.cpp
#include "CabinAcousticsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"
#include "../acoustics/core/RoomModes.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
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
#include <algorithm>
#include <vector>

using namespace ofd;
namespace rm = ofd::acoustics::roommodes;

// ── タブ固有語彙 (cab_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_cabin", "🚗 車内音響", "🚗 Cabin NVH");
    // 概要
    I18n::reg("cab_main_section", "車内・機内音響 (NVH)", "Cabin NVH");
    I18n::reg("cab_main_hint",
              "自動車・航空機・鉄道の車内騒音を、構造振動+音響のFEM/FDTD/SEA連成で"
              "予測する構想 (連成解析は未実装 — この画面は設計モック)。",
              "Concept for predicting interior noise of cars, aircraft and "
              "trains with coupled structural-acoustic FEM / FDTD / SEA "
              "(coupled analysis not implemented — this page is a design mock).");
    I18n::reg("cab_target", "対象", "Target");
    I18n::reg("cab_veh_car", "乗用車", "Passenger car");
    I18n::reg("cab_veh_ev", "EV", "EV");
    I18n::reg("cab_veh_train", "鉄道", "Train");
    I18n::reg("cab_veh_aircraft", "航空機", "Aircraft");
    // 騒音源
    I18n::reg("cab_src_section", "騒音源", "Noise sources");
    I18n::reg("cab_car_engine", "エンジン透過音 (100-500Hz)",
              "Engine airborne noise (100-500 Hz)");
    I18n::reg("cab_car_road", "ロードノイズ (30-300Hz)",
              "Road noise (30-300 Hz)");
    I18n::reg("cab_car_wind", "風切り音 (>1kHz)", "Wind noise (>1 kHz)");
    I18n::reg("cab_car_boom", "こもり音 (ブーミング 40-80Hz)",
              "Booming (40-80 Hz)");
    I18n::reg("cab_ev_motor", "モーター高周波音 (2-8kHz)",
              "Motor high-frequency tone (2-8 kHz)");
    I18n::reg("cab_ev_inverter", "インバータスイッチング音",
              "Inverter switching noise");
    I18n::reg("cab_ev_road", "ロードノイズ (相対的に顕在化)",
              "Road noise (relatively dominant)");
    I18n::reg("cab_ev_gear", "ギヤワイン", "Gear whine");
    I18n::reg("cab_ev_note",
              "▸ EVはエンジンマスキングが無いため高周波トーンの対策が主戦場",
              "▸ Without engine masking, EVs fight high-frequency tones");
    I18n::reg("cab_tr_rolling", "転動音", "Rolling noise");
    I18n::reg("cab_tr_tunnel", "トンネル内圧力変動",
              "Tunnel pressure fluctuation");
    I18n::reg("cab_tr_hvac", "空調", "HVAC");
    I18n::reg("cab_ac_tbl", "境界層騒音 (TBL)",
              "Turbulent boundary layer (TBL) noise");
    I18n::reg("cab_ac_engine", "エンジン (ファン/ジェット)",
              "Engine (fan / jet)");
    I18n::reg("cab_ac_press", "与圧系統", "Pressurization system");
    // 解析手法
    I18n::reg("cab_method_section", "解析手法 (帯域別)", "Method by frequency");
    I18n::reg("cab_h_band", "帯域", "Band");
    I18n::reg("cab_h_method", "手法", "Method");
    I18n::reg("cab_h_use", "用途", "Use");
    I18n::reg("cab_band_low", "〜200 Hz", "up to 200 Hz");
    I18n::reg("cab_band_mid", "200-1000 Hz", "200-1000 Hz");
    I18n::reg("cab_band_high", "1 kHz〜", "1 kHz and above");
    I18n::reg("cab_m_fem", "FEM/FDTD", "FEM / FDTD");
    I18n::reg("cab_m_fesea", "ハイブリッド FE-SEA", "Hybrid FE-SEA");
    I18n::reg("cab_m_sea", "SEA (統計的)", "SEA (statistical)");
    I18n::reg("cab_u_low", "こもり音・パネル共振・音響モード",
              "Booming, panel resonance, acoustic modes");
    I18n::reg("cab_u_mid", "中間周波数帯", "Mid-frequency range");
    I18n::reg("cab_u_high", "風切り音・高周波透過",
              "Wind noise, high-frequency transmission");
    // 車室モデル
    I18n::reg("cab_model_section", "車室モデル", "Cabin model");
    I18n::reg("cab_cad", "3Dモデル", "3D model");
    I18n::reg("cab_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("cab_modal_row", "音響モード解析", "Acoustic modal analysis");
    I18n::reg("cab_modal", "固有モードを計算する",
              "Compute the eigenmodes");
    I18n::reg("cab_absorb", "吸音内装", "Interior absorption");
    I18n::reg("cab_abs_roof", "ルーフライナー", "Roof liner");
    I18n::reg("cab_abs_carpet", "カーペット", "Carpet");
    I18n::reg("cab_abs_door", "ドアトリム", "Door trim");
    I18n::reg("cab_abs_seat", "シート", "Seats");
    // 車室音響モード (直方体近似の厳密解)
    I18n::reg("cab_mode_section", "車室音響モード (直方体近似)",
              "Cabin acoustic modes (rectangular approximation)");
    I18n::reg("cab_mode_hint",
              "剛壁直方体室の固有周波数 f(nx,ny,nz) = (c/2)·√((nx/L)²+(ny/W)²+(nz/H)²) "
              "を下の寸法から計算します (Rayleigh, The Theory of Sound Vol.II §267, 1896)。"
              "ブーミング対策で狙う周波数の目安になります。"
              "初期値は乗用車を想定した入力例です — 対象車両の実寸法に"
              "置き換えてください。",
              "Eigenfrequencies of a rigid-walled rectangular room, "
              "f(nx,ny,nz) = (c/2)·√((nx/L)²+(ny/W)²+(nz/H)²), computed from the "
              "dimensions below (Rayleigh, The Theory of Sound Vol. II §267, 1896). "
              "They indicate the frequencies to target against booming. The "
              "initial values are an example for a passenger car — replace them "
              "with the real dimensions of your vehicle.");
    I18n::reg("cab_dim_l", "長さ L (前後)", "Length L (fore-aft)");
    I18n::reg("cab_dim_w", "幅 W (左右)", "Width W (lateral)");
    I18n::reg("cab_dim_h", "高さ H (上下)", "Height H (vertical)");
    I18n::reg("cab_temp", "室温", "Air temperature");
    I18n::reg("cab_fmax", "計算上限周波数", "Upper frequency");
    I18n::reg("cab_h_order", "次数 (nx,ny,nz)", "Order (nx,ny,nz)");
    I18n::reg("cab_h_freq", "周波数 [Hz]", "Frequency [Hz]");
    I18n::reg("cab_h_kind", "種別", "Type");
    I18n::reg("cab_kind_axial", "軸 (axial)", "Axial");
    I18n::reg("cab_kind_tang", "接線 (tangential)", "Tangential");
    I18n::reg("cab_kind_obl", "斜め (oblique)", "Oblique");
    I18n::reg("cab_mode_summary",
              "音速 c = %1 m/s (%2 ℃) / 容積 V = %3 m³ / "
              "%4 Hz 以下のモード数 %5 (表示 %6) / 最低次モード %7 Hz",
              "c = %1 m/s (%2 °C) / V = %3 m³ / %5 modes below %4 Hz "
              "(%6 listed) / lowest mode %7 Hz");
    I18n::reg("cab_mode_bad",
              "▸ 未計算 — 寸法・室温・上限周波数を正の数で入力してください。",
              "▸ Not computed — enter positive values for the dimensions, "
              "temperature and upper frequency.");
    I18n::reg("cab_mode_off",
              "▸ 未計算 — 「音響モード解析」のチェックを入れると計算します。",
              "▸ Not computed — tick “Acoustic modal analysis” to compute.");
    I18n::reg("cab_mode_src",
              "▸ 剛壁・直方体・無損失の理想化に基づく解析解です。実際の車室 "
              "(曲面・座席・吸音内装・窓) の共鳴周波数とは差が出ます。音速は "
              "c = 331.3·√(1+t/273.15) (ISO 9613-1:1993)。",
              "▸ Closed-form solution for an idealised rigid, lossless, "
              "rectangular room. Real cabins (curved surfaces, seats, trim, "
              "glazing) resonate at somewhat different frequencies. Speed of "
              "sound c = 331.3·√(1+t/273.15) (ISO 9613-1:1993).");
    // 評価
    I18n::reg("cab_metrics_section", "評価", "Metrics");
    I18n::reg("cab_m_earspl", "運転席耳位置 SPL (dB(A))",
              "Driver ear-position SPL (dB(A))");
    I18n::reg("cab_m_ai", "AI (会話明瞭度指数)",
              "AI (articulation index)");
    I18n::reg("cab_m_loud", "Loudness (ISO 532-1)", "Loudness (ISO 532-1)");
    I18n::reg("cab_m_sharp", "Sharpness / Roughness (音質評価)",
              "Sharpness / Roughness (sound quality)");
    I18n::reg("cab_h_metric", "指標", "Metric");
    I18n::reg("cab_h_value", "値", "Value");
    I18n::reg("cab_h_need", "算出に必要なもの", "Required to compute it");
    I18n::reg("cab_dash", "—", "—");
    I18n::reg("cab_need_spl",
              "校正済みの車室内音場 (連成解析または実測)。未校正の絶対 SPL は表示しない",
              "A calibrated cabin sound field (coupled analysis or measurement); "
              "uncalibrated absolute SPL is never shown");
    I18n::reg("cab_need_ai",
              "オクターブ帯域の信号・騒音レベル (ANSI S3.5)",
              "Octave-band speech and noise levels (ANSI S3.5)");
    I18n::reg("cab_need_loud",
              "1/3 オクターブ帯域スペクトル (ISO 532-1 の実装は未着手)",
              "Third-octave spectrum (ISO 532-1 not implemented)");
    I18n::reg("cab_need_sharp",
              "同スペクトル (Sharpness / Roughness の実装は未着手)",
              "The same spectrum (sharpness / roughness not implemented)");
    I18n::reg("cab_metrics_note",
              "▸ いずれも未計算です — 車室の構造振動+音響連成解析が未実装のため、"
              "値を出せる入力がありません。校正の無い絶対 SPL や根拠の無い比較値は "
              "表示しません。連成解析またはキャリブレーション済み実測の結果が"
              "入力された時点で、この表の「値」欄が埋まります。",
              "▸ Nothing here is computed yet: the coupled structural-acoustic "
              "analysis of the cabin is not implemented, so there is no input "
              "from which to derive these values. Uncalibrated absolute SPL and "
              "unsupported comparisons are never displayed. The Value column "
              "fills in once a coupled analysis or a calibrated measurement "
              "provides results.");
    I18n::reg("cab_aural_btn", "🎧 車内音の可聴化", "🎧 Auralize cabin sound");
    I18n::reg("cab_tpa_btn", "📊 寄与度分析 (TPA)",
              "📊 Contribution analysis (TPA)");
    // 対策検討
    I18n::reg("cab_meas_section", "対策検討", "Countermeasures");
    I18n::reg("cab_h_meas", "対策", "Countermeasure");
    I18n::reg("cab_h_effect", "効果 [dB] (入力)", "Effect [dB] (entered)");
    I18n::reg("cab_h_weight", "重量 [kg] (入力)", "Weight [kg] (entered)");
    I18n::reg("cab_h_cost", "コスト (入力)", "Cost (entered)");
    I18n::reg("cab_meas_dash", "ダッシュインシュレータ増厚",
              "Thicker dash insulator");
    I18n::reg("cab_meas_damp", "制振材 (フロア)", "Damping material (floor)");
    I18n::reg("cab_meas_glass", "遮音ガラス (合わせ)",
              "Acoustic laminated glass");
    I18n::reg("cab_meas_anc", "ANC (アクティブ制御)", "ANC (active control)");
    I18n::reg("cab_meas_note",
              "▸ 効果・重量・コストの各欄は空欄です — 対策の効果は解析または実測が"
              "必要で、本画面には算出根拠がありません (未計算)。ダブルクリックで"
              "自分の解析値・見積り値を入力できます (入力値であって計算結果では"
              "ありません)。合計するのは重量だけです — 効果 [dB] は伝搬経路が"
              "異なるため単純加算できません。",
              "▸ The effect / weight / cost cells are empty: the benefit of a "
              "countermeasure has to come from analysis or measurement, and this "
              "screen has no basis to derive it (not computed). Double-click a "
              "cell to enter your own analysed or estimated figures (entered "
              "values, not computed results). Only weight is summed — effects in "
              "dB act on different paths and cannot simply be added.");
    I18n::reg("cab_meas_total",
              "選択中 %1 件 / 重量入力済み %2 件の合計 %3 kg",
              "%1 selected / %3 kg total over the %2 rows with a weight entered");
    I18n::reg("cab_meas_total_none",
              "選択中 %1 件 / 重量の入力がないため合計は未計算",
              "%1 selected / no weight entered, so no total");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-table 相当) ─────────────────────
const char kAcc[] = "#0078D4";      // badge acc

// モード表に並べる最大行数 (総数は要約行に出す)
const int kShownModes = 40;

QLabel *makeHint(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
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

// 「値 + 単位」の 1 行 (mock の <Row><Num/><span class=muted/></Row> 相当)
QHBoxLayout *unitRow(QWidget *w, const QString &unit, QWidget *parent)
{
    auto *h = new QHBoxLayout();
    h->addWidget(w);
    h->addWidget(new QLabel(unit, parent));
    h->addStretch(1);
    return h;
}

// <Seg> 相当 (少数選択肢の排他セグメント) — QComboBox で再現
QComboBox *makeSeg(const QStringList &items, int current, QWidget *parent)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
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

QTableWidgetItem *badgeItem(const QString &s, const char *color)
{
    auto *it = new QTableWidgetItem(s);
    if (color) it->setForeground(QColor(color));
    QFont f = it->font();
    f.setBold(true);
    it->setFont(f);
    return it;
}

QTableWidgetItem *checkItem(bool on)
{
    auto *it = new QTableWidgetItem;
    it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
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
} // namespace

// ── CabinAcousticsTab ───────────────────────────────────────────────────────
CabinAcousticsTab::CabinAcousticsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 車内・機内音響 (概要 + 対象) ────────────────────────────────────────
    auto *sm = new SectionBox(I18n::tr("cab_main_section"), body);
    sm->vbox()->addWidget(makeHint(I18n::tr("cab_main_hint"), sm));
    m_vehicle = makeSeg({ I18n::tr("cab_veh_car"), I18n::tr("cab_veh_ev"),
                          I18n::tr("cab_veh_train"),
                          I18n::tr("cab_veh_aircraft") }, 0, sm);
    sm->form()->addRow(I18n::tr("cab_target"), m_vehicle);
    v->addWidget(sm);

    // ── 騒音源 (対象ごとに切替) ─────────────────────────────────────────────
    auto *ss = new SectionBox(I18n::tr("cab_src_section"), body);
    m_srcStack = new QStackedWidget(ss);
    m_srcStack->addWidget(buildCarSources());       // 0 car
    m_srcStack->addWidget(buildEvSources());        // 1 ev
    m_srcStack->addWidget(buildTrainSources());     // 2 train
    m_srcStack->addWidget(buildAircraftSources());  // 3 aircraft
    ss->vbox()->addWidget(m_srcStack);
    // 騒音源チェックはローカル状態のみ (どこにも読まれない)
    ss->vbox()->addWidget(tabhelp::unwiredNote(ss));
    v->addWidget(ss);

    // ── 解析手法 (帯域別) ───────────────────────────────────────────────────
    auto *sf = new SectionBox(I18n::tr("cab_method_section"), body);
    m_methodTable = makeTable({ I18n::tr("cab_h_band"), I18n::tr("cab_h_method"),
                                I18n::tr("cab_h_use") }, 3, sf, 120);
    struct Meth { const char *band; const char *method; const char *use;
                  const char *color; };
    static const Meth kMeth[3] = {
        { "cab_band_low",  "cab_m_fem",   "cab_u_low",  kAcc    },
        { "cab_band_mid",  "cab_m_fesea", "cab_u_mid",  nullptr },
        { "cab_band_high", "cab_m_sea",   "cab_u_high", nullptr },
    };
    for (int i = 0; i < 3; ++i) {
        m_methodTable->setItem(i, 0, textItem(I18n::tr(kMeth[i].band)));
        m_methodTable->setItem(i, 1, badgeItem(I18n::tr(kMeth[i].method),
                                               kMeth[i].color));
        m_methodTable->setItem(i, 2, textItem(I18n::tr(kMeth[i].use)));
    }
    sf->vbox()->addWidget(m_methodTable);
    v->addWidget(sf);

    // ── 車室モデル ──────────────────────────────────────────────────────────
    auto *sc = new SectionBox(I18n::tr("cab_model_section"), body);
    auto *hCad = new QHBoxLayout();
    m_cadFile = new QLineEdit("cabin_interior.step", sc);
    hCad->addWidget(m_cadFile, 1);
    auto *cadBrowse = new QPushButton(I18n::tr("cab_browse"), sc);
    hCad->addWidget(cadBrowse);
    sc->form()->addRow(I18n::tr("cab_cad"), hCad);
    m_absRoof   = makeCheck(I18n::tr("cab_abs_roof"),   true, sc);
    m_absCarpet = makeCheck(I18n::tr("cab_abs_carpet"), true, sc);
    m_absDoor   = makeCheck(I18n::tr("cab_abs_door"),   true, sc);
    m_absSeat   = makeCheck(I18n::tr("cab_abs_seat"),   true, sc);
    sc->form()->addRow(I18n::tr("cab_absorb"),
                       checkRow({ m_absRoof, m_absCarpet, m_absDoor,
                                  m_absSeat }));
    // CAD ファイル・吸音内装のチェックはまだどこにも読まれない
    // (下の音響モード計算は寸法入力のみを使う)
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc));
    v->addWidget(sc);

    // ── 車室音響モード (直方体近似の厳密解) ────────────────────────────────
    // 固定サンプルだった「1次 42Hz / 2次 68Hz / 3次 87Hz」を、入力寸法からの
    // 実計算 (acoustics/core/RoomModes) へ置き換えたもの。
    auto *smd = new SectionBox(I18n::tr("cab_mode_section"), body);
    smd->vbox()->addWidget(makeHint(I18n::tr("cab_mode_hint"), smd));
    m_modal = makeCheck(I18n::tr("cab_modal"), true, smd);
    smd->form()->addRow(I18n::tr("cab_modal_row"), m_modal);
    m_dimL = numEdit("2.40", smd);
    m_dimW = numEdit("1.45", smd);
    m_dimH = numEdit("1.15", smd);
    smd->form()->addRow(I18n::tr("cab_dim_l"), unitRow(m_dimL, "m", smd));
    smd->form()->addRow(I18n::tr("cab_dim_w"), unitRow(m_dimW, "m", smd));
    smd->form()->addRow(I18n::tr("cab_dim_h"), unitRow(m_dimH, "m", smd));
    m_temp = numEdit("20", smd);
    smd->form()->addRow(I18n::tr("cab_temp"), unitRow(m_temp, "℃", smd));
    m_fmax = numEdit("200", smd);
    smd->form()->addRow(I18n::tr("cab_fmax"), unitRow(m_fmax, "Hz", smd));
    m_modeTable = makeTable({ I18n::tr("cab_h_order"), I18n::tr("cab_h_freq"),
                              I18n::tr("cab_h_kind") }, 0, smd, 220);
    smd->vbox()->addWidget(m_modeTable);
    m_modeSummary = makeHint(QString(), smd);
    smd->vbox()->addWidget(m_modeSummary);
    smd->vbox()->addWidget(makeHint(I18n::tr("cab_mode_src"), smd));
    v->addWidget(smd);

    // ── 評価 ────────────────────────────────────────────────────────────────
    auto *se = new SectionBox(I18n::tr("cab_metrics_section"), body);
    m_mSpl       = makeCheck(I18n::tr("cab_m_earspl"), true,  se);
    m_mAi        = makeCheck(I18n::tr("cab_m_ai"),     true,  se);
    m_mLoudness  = makeCheck(I18n::tr("cab_m_loud"),   false, se);
    m_mSharpness = makeCheck(I18n::tr("cab_m_sharp"),  false, se);
    se->vbox()->addLayout(checkRow({ m_mSpl, m_mAi }));
    se->vbox()->addLayout(checkRow({ m_mLoudness, m_mSharpness }));
    // 固定サンプルだった「62.4 dB(A)」「セグメント平均 -1.8dB」のバッジを、
    // 「未計算 (—)」+ 何があれば埋まるかの表へ置き換えたもの。
    // 校正の無い絶対 SPL は表示しない (絶対規則 6)。
    m_metricTable = makeTable({ I18n::tr("cab_h_metric"), I18n::tr("cab_h_value"),
                                I18n::tr("cab_h_need") }, 0, se, 120);
    se->vbox()->addWidget(m_metricTable);
    se->vbox()->addWidget(makeHint(I18n::tr("cab_metrics_note"), se));
    auto *hBtn = new QHBoxLayout();
    auto *auralBtn = new QPushButton(I18n::tr("cab_aural_btn"), se);
    auto *tpaBtn   = new QPushButton(I18n::tr("cab_tpa_btn"), se);
    tabhelp::markNotImplemented(auralBtn);
    tabhelp::markNotImplemented(tpaBtn);
    hBtn->addWidget(auralBtn);
    hBtn->addWidget(tpaBtn);
    hBtn->addStretch(1);
    se->vbox()->addLayout(hBtn);
    v->addWidget(se);

    // ── 対策検討 ────────────────────────────────────────────────────────────
    auto *sw = new SectionBox(I18n::tr("cab_meas_section"), body);
    m_measureTable = makeTable({ "", I18n::tr("cab_h_meas"),
                                 I18n::tr("cab_h_effect"),
                                 I18n::tr("cab_h_weight"),
                                 I18n::tr("cab_h_cost") }, 4, sw, 150);
    // 効果 / 重量 / コストは固定サンプルを廃し、既定は空欄 (未計算)。
    // 利用者が自分の解析値・見積り値を入力できるセルにする。
    static const char *const kMeasName[4] = {
        "cab_meas_dash", "cab_meas_damp", "cab_meas_glass", "cab_meas_anc"
    };
    m_measureTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                    | QAbstractItemView::SelectedClicked
                                    | QAbstractItemView::EditKeyPressed);
    for (int i = 0; i < 4; ++i) {
        m_measureTable->setItem(i, 0, checkItem(false));
        m_measureTable->setItem(i, 1, textItem(I18n::tr(kMeasName[i])));
        for (int c = 2; c <= 4; ++c) {
            auto *it = (c == 4) ? textItem(QString()) : numItem(QString());
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         | Qt::ItemIsEditable);
            m_measureTable->setItem(i, c, it);
        }
    }
    sw->vbox()->addWidget(m_measureTable);
    m_measureSummary = makeHint(QString(), sw);
    sw->vbox()->addWidget(m_measureSummary);
    sw->vbox()->addWidget(makeHint(I18n::tr("cab_meas_note"), sw));
    v->addWidget(sw);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // 3D モデルの参照ボタンのみ実配線 (隣の QLineEdit にパスを反映)
    connect(cadBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("cab_cad"), QString(),
            "3D model (*.step *.stp *.iges *.igs *.stl *.obj);;All files (*)");
        if (!path.isEmpty()) m_cadFile->setText(path);
    });
    connect(m_vehicle, &QComboBox::currentIndexChanged, this, [this](int i) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_vehicleIdx = i;
        m_srcStack->setCurrentIndex(i);
    });

    // 音響モード: 寸法・室温・上限周波数のいずれかが変わるたびに再計算する
    // (直方体の閉形式解なので同期実行でよい — 数万モードでも数 ms)
    for (QLineEdit *e : { m_dimL, m_dimW, m_dimH, m_temp, m_fmax })
        connect(e, &QLineEdit::textChanged, this,
                &CabinAcousticsTab::updateModes);
    connect(m_modal, &QCheckBox::toggled, this, &CabinAcousticsTab::updateModes);

    // 評価: どの指標を対象にするかで表の行が変わる (値は常に未計算)
    for (QCheckBox *c : { m_mSpl, m_mAi, m_mLoudness, m_mSharpness })
        connect(c, &QCheckBox::toggled, this,
                &CabinAcousticsTab::updateMetrics);

    connect(m_measureTable, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *) { updateMeasures(); });

    connect(project, &Project::loaded, this, &CabinAcousticsTab::refresh);
    refresh();
}

void CabinAcousticsTab::refresh()
{
    m_updating = true;
    m_vehicle->setCurrentIndex(m_vehicleIdx);
    m_srcStack->setCurrentIndex(m_vehicleIdx);
    m_updating = false;
    updateModes();
    updateMetrics();
    updateMeasures();
}

// ── 車室音響モード ──────────────────────────────────────────────────────────
// 剛壁直方体の解析解 (acoustics/core/RoomModes) をそのまま表に出す。
void CabinAcousticsTab::updateModes()
{
    m_modeTable->setRowCount(0);

    if (!m_modal->isChecked()) {
        m_modeTable->setEnabled(false);
        m_modeSummary->setText(I18n::tr("cab_mode_off"));
        return;
    }
    m_modeTable->setEnabled(true);

    bool okL = false, okW = false, okH = false, okT = false, okF = false;
    const double L = m_dimL->text().toDouble(&okL);
    const double W = m_dimW->text().toDouble(&okW);
    const double H = m_dimH->text().toDouble(&okH);
    const double t = m_temp->text().toDouble(&okT);
    const double fmax = m_fmax->text().toDouble(&okF);
    const double c = okT ? rm::soundSpeed(t) : 0.0;
    if (!okL || !okW || !okH || !okT || !okF
        || L <= 0 || W <= 0 || H <= 0 || fmax <= 0 || c <= 0) {
        m_modeSummary->setText(I18n::tr("cab_mode_bad"));
        return;
    }

    // 表に出すのは低次から kShownModes 個 (以下のモード総数は要約に出す)
    const std::vector<rm::Mode> all =
        rm::rectangularModes(L, W, H, c, fmax, 0);
    const int total = int(all.size());
    const int shown = std::min(total, kShownModes);

    m_modeTable->setRowCount(shown);
    for (int i = 0; i < shown; ++i) {
        const rm::Mode &m = all[size_t(i)];
        m_modeTable->setItem(i, 0, textItem(QString("(%1, %2, %3)")
                                                .arg(m.nx).arg(m.ny).arg(m.nz)));
        m_modeTable->setItem(i, 1, numItem(QString::number(m.freqHz, 'f', 1)));
        const char *kindKey = (m.kind == rm::ModeAxial)      ? "cab_kind_axial"
                            : (m.kind == rm::ModeTangential) ? "cab_kind_tang"
                                                             : "cab_kind_obl";
        m_modeTable->setItem(i, 2,
            badgeItem(I18n::tr(kindKey),
                      m.kind == rm::ModeAxial ? kAcc : nullptr));
    }

    m_modeSummary->setText(
        I18n::tr("cab_mode_summary")
            .arg(QString::number(c, 'f', 1))
            .arg(QString::number(t, 'f', 1))
            .arg(QString::number(L * W * H, 'f', 2))
            .arg(QString::number(fmax, 'f', 0))
            .arg(total)
            .arg(shown)
            .arg(total > 0 ? QString::number(all[0].freqHz, 'f', 1)
                           : I18n::tr("cab_dash")));
}

// ── 評価 (未計算) ───────────────────────────────────────────────────────────
// 連成解析が未実装で算出根拠が無いため、値は常に「—」。何があれば埋まるかを
// 併記する (絶対規則 5・6)。実行結果が入るようになったら value を差し替える。
void CabinAcousticsTab::updateMetrics()
{
    struct Row { QCheckBox *cb; const char *name; const char *need; };
    const Row rows[4] = {
        { m_mSpl,       "cab_m_earspl", "cab_need_spl"   },
        { m_mAi,        "cab_m_ai",     "cab_need_ai"    },
        { m_mLoudness,  "cab_m_loud",   "cab_need_loud"  },
        { m_mSharpness, "cab_m_sharp",  "cab_need_sharp" },
    };
    m_metricTable->setRowCount(0);
    int r = 0;
    for (const Row &row : rows) {
        if (!row.cb->isChecked()) continue;
        m_metricTable->insertRow(r);
        m_metricTable->setItem(r, 0, textItem(I18n::tr(row.name)));
        // 値は未計算 — 偽の数値を出さない
        m_metricTable->setItem(r, 1, numItem(I18n::tr("cab_dash")));
        m_metricTable->setItem(r, 2, textItem(I18n::tr(row.need)));
        ++r;
    }
}

// ── 対策検討 (利用者入力の集計) ─────────────────────────────────────────────
// 効果 [dB] は経路が異なるため合算しない。加算が成り立つ重量のみ合計する。
void CabinAcousticsTab::updateMeasures()
{
    int selected = 0, weighed = 0;
    double totalKg = 0.0;
    for (int i = 0; i < m_measureTable->rowCount(); ++i) {
        QTableWidgetItem *chk = m_measureTable->item(i, 0);
        if (!chk || chk->checkState() != Qt::Checked) continue;
        ++selected;
        QTableWidgetItem *w = m_measureTable->item(i, 3);
        if (!w) continue;
        bool ok = false;
        const double kg = w->text().trimmed().toDouble(&ok);
        if (!ok) continue;
        ++weighed;
        totalKg += kg;
    }
    m_measureSummary->setText(
        weighed > 0 ? I18n::tr("cab_meas_total")
                          .arg(selected).arg(weighed)
                          .arg(QString::number(totalKg, 'f', 2))
                    : I18n::tr("cab_meas_total_none").arg(selected));
}

// ── 乗用車 / car ────────────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildCarSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_carEngine = makeCheck(I18n::tr("cab_car_engine"), true, page);
    m_carRoad   = makeCheck(I18n::tr("cab_car_road"),   true, page);
    m_carWind   = makeCheck(I18n::tr("cab_car_wind"),   true, page);
    m_carBoom   = makeCheck(I18n::tr("cab_car_boom"),   true, page);
    v->addLayout(checkRow({ m_carEngine, m_carRoad }));
    v->addLayout(checkRow({ m_carWind, m_carBoom }));
    return page;
}

// ── EV ──────────────────────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildEvSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_evMotor    = makeCheck(I18n::tr("cab_ev_motor"),    true, page);
    m_evInverter = makeCheck(I18n::tr("cab_ev_inverter"), true, page);
    m_evRoad     = makeCheck(I18n::tr("cab_ev_road"),     true, page);
    m_evGear     = makeCheck(I18n::tr("cab_ev_gear"),     true, page);
    v->addLayout(checkRow({ m_evMotor, m_evInverter }));
    v->addLayout(checkRow({ m_evRoad, m_evGear }));
    v->addWidget(makeHint(I18n::tr("cab_ev_note"), page));
    return page;
}

// ── 鉄道 / train ────────────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildTrainSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_trRolling = makeCheck(I18n::tr("cab_tr_rolling"), true, page);
    m_trTunnel  = makeCheck(I18n::tr("cab_tr_tunnel"),  true, page);
    m_trHvac    = makeCheck(I18n::tr("cab_tr_hvac"),    true, page);
    v->addLayout(checkRow({ m_trRolling, m_trTunnel, m_trHvac }));
    return page;
}

// ── 航空機 / aircraft ───────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildAircraftSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_acTbl      = makeCheck(I18n::tr("cab_ac_tbl"),    true,  page);
    m_acEngine   = makeCheck(I18n::tr("cab_ac_engine"), true,  page);
    m_acPressure = makeCheck(I18n::tr("cab_ac_press"),  false, page);
    v->addLayout(checkRow({ m_acTbl, m_acEngine, m_acPressure }));
    return page;
}
