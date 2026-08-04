// EmcTab.cpp
#include "EmcTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../em/EmcStandards.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <cmath>

using namespace ofd;

// ── タブ固有語彙 (emc_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_emc", "📻 EMC/EMI", "📻 EMC/EMI");

    // 概要
    I18n::reg("emc_title", "EMC / EMI 解析 / Compliance analysis",
              "EMC / EMI compliance analysis");
    I18n::reg("emc_hint",
              "放射エミッション・イミュニティを規格の試験配置そのままで再現。"
              "試作前に規格逸脱を予測。",
              "Reproduces radiated emission and immunity tests in the standard's "
              "own setup, predicting non-compliance before the first prototype.");
    I18n::reg("emc_mode_emission", "放射エミッション", "Radiated emission");
    I18n::reg("emc_mode_conducted", "伝導エミッション", "Conducted emission");
    I18n::reg("emc_mode_immunity", "イミュニティ (RS/ESD)", "Immunity (RS/ESD)");
    I18n::reg("emc_standard", "適用規格", "Applicable standard");
    I18n::reg("emc_std_cispr32", "CISPR 32 / EN 55032 (マルチメディア機器)",
              "CISPR 32 / EN 55032 (multimedia equipment)");
    I18n::reg("emc_std_cispr25", "CISPR 25 (車載)", "CISPR 25 (automotive)");
    I18n::reg("emc_std_fcc15", "FCC Part 15 (§15.109 放射妨害波)",
              "FCC Part 15 (§15.109 radiated emission)");
    I18n::reg("emc_std_iec4_3", "IEC 61000-4-3 (放射イミュニティ)",
              "IEC 61000-4-3 (radiated immunity)");
    I18n::reg("emc_std_iec4_2", "IEC 61000-4-2 (ESD)", "IEC 61000-4-2 (ESD)");
    I18n::reg("emc_std_do160", "RTCA DO-160 (航空)", "RTCA DO-160 (avionics)");
    I18n::reg("emc_std_mil461", "MIL-STD-461G (軍用)", "MIL-STD-461G (military)");

    // 試験配置
    I18n::reg("emc_setup_section", "試験配置 / Test setup", "Test setup");
    I18n::reg("emc_site", "サイト", "Site");
    I18n::reg("emc_site_oats", "OATS (オープンサイト)",
              "OATS (open area test site)");
    I18n::reg("emc_site_semi", "セミアネコイックチャンバ",
              "Semi-anechoic chamber");
    I18n::reg("emc_site_full", "フルアネコイック", "Fully anechoic");
    I18n::reg("emc_site_rev", "リバブレーションチャンバ",
              "Reverberation chamber");
    I18n::reg("emc_distance", "測定距離", "Measurement distance");
    I18n::reg("emc_distance_unit", "m (10m換算も併記)",
              "m (10 m equivalent also reported)");
    I18n::reg("emc_ant_h", "アンテナ高", "Antenna height");
    I18n::reg("emc_ant_h_unit", "〜4.0 m 走査", "scanned up to 4.0 m");
    I18n::reg("emc_eut", "EUT配置", "EUT placement");
    I18n::reg("emc_eut_turn", "ターンテーブル0〜360° (15°刻み)",
              "Turntable 0–360° (15° steps)");
    I18n::reg("emc_eut_pol", "水平/垂直偏波両方",
              "Both horizontal and vertical polarization");
    I18n::reg("emc_gnd", "グランドプレーン", "Ground plane");
    I18n::reg("emc_gnd_pec", "金属床 (PEC) を模擬",
              "Model the metal floor (PEC)");
    I18n::reg("emc_gnd_cable", "ケーブル配線を含む", "Include the cable routing");

    // 放射源
    I18n::reg("emc_src_section", "放射源 / Emission sources", "Emission sources");
    I18n::reg("emc_src_switching", "基板のスイッチングノイズ (PEEC 連携は未実装)",
              "Board switching noise (PEEC hand-off not implemented)");
    I18n::reg("emc_src_cm", "ケーブル・コモンモード電流",
              "Cable common-mode current");
    I18n::reg("emc_src_slit", "筐体スリット・開口",
              "Enclosure slits and apertures");
    I18n::reg("emc_clock", "クロック", "Clock");
    I18n::reg("emc_clock_unit", "MHz (高調波 40次まで)",
              "MHz (harmonics up to the 40th)");

    // 判定結果
    I18n::reg("emc_check_section", "判定結果 / Compliance check",
              "Compliance check");
    I18n::reg("emc_class", "クラス", "Class");
    I18n::reg("emc_class_a", "Class A (工業環境)", "Class A (industrial)");
    I18n::reg("emc_class_b", "Class B (住宅環境)", "Class B (residential)");
    I18n::reg("emc_col_freq", "周波数", "Frequency");
    I18n::reg("emc_col_meas", "実測相当値", "Measured equivalent");
    I18n::reg("emc_col_limit", "規格限度", "Limit");
    I18n::reg("emc_col_margin", "マージン", "Margin");
    I18n::reg("emc_col_verdict", "判定", "Verdict");
    // 限度値は実データ / 被測定値は未取得 — この 2 つを混同させない注記
    I18n::reg("emc_limit_note",
              "▸ 「規格限度」列と下の曲線は規格の公表値です "
              "(CISPR 32:2015 Table A.3/A.4 = 準尖頭値 10 m、"
              "FCC 47 CFR §15.109)。"
              "「実測相当値」「マージン」「判定」は、実測またはFDTD解析で得た"
              "エミッションレベルがまだ無いため「—」です — "
              "放射エミッションの解析・測定値の取り込みは未実装です。"
              "値が得られれば同じ表・曲線に重ねて判定します。",
              "▸ The “Limit” column and the curve below are the published values of "
              "the standard (CISPR 32:2015 Tables A.3/A.4, quasi-peak at 10 m; "
              "FCC 47 CFR §15.109). “Measured equivalent”, “Margin” and “Verdict” "
              "are “—” because no emission level from measurement or FDTD exists "
              "yet — computing/importing radiated emission is not implemented. "
              "Once a level is available it is overlaid on this same table and "
              "curve and the verdict is evaluated.");
    I18n::reg("emc_limit_none",
              "▸ 選択した規格の放射妨害波限度値表はこのビルドに収載していません。"
              "推定値を表示することはしません (収載: CISPR 32 / FCC Part 15)。",
              "▸ The radiated-disturbance limit table of the selected standard is "
              "not bundled with this build, and no estimate is shown in its place "
              "(bundled: CISPR 32 / FCC Part 15).");
    I18n::reg("emc_limit_dist",
              "▸ 限度値は規定測定距離 %1 m の値を逆距離則 20log10(d_ref/d) で "
              "%2 m に換算しています (CISPR 16-2-3 / ANSI C63.4 の外挿。"
              "グランド反射・近傍界の影響があるため目安)。",
              "▸ Limits are extrapolated from the standard's %1 m measurement "
              "distance to %2 m by the inverse-distance rule 20log10(d_ref/d) "
              "(the extrapolation allowed by CISPR 16-2-3 / ANSI C63.4; treat it "
              "as an estimate — ground reflection and near-field effects apply).");
    I18n::reg("emc_limit_dist_same",
              "▸ 限度値は規格の規定測定距離 %1 m での値です。",
              "▸ Limits are given at the standard's %1 m measurement distance.");
    I18n::reg("emc_proj_freq",
              "▸ プロジェクトの解析周波数: %1 / 限度値表の範囲: 30 MHz – 1 GHz",
              "▸ Project analysis frequency: %1 / limit table range: 30 MHz – 1 GHz");
    I18n::reg("emc_btn_locate", "🔍 放射源を特定 (寄与度)",
              "🔍 Locate the dominant source (contributions)");
    I18n::reg("emc_btn_report", "📄 EMC事前評価レポート",
              "📄 EMC pre-compliance report");

    // 対策検討
    I18n::reg("emc_mit_section", "対策検討 / Mitigation", "Mitigation");
    I18n::reg("emc_mit_note",
              "▸ 改善量は下の古典式で計算した値です "
              "(挿入損失 IL = 20log10(1+Z/Zc) : CISPR 17 の挿入損失の定義、"
              "開口 SE ∝ 20log10(1/L) : H. W. Ott, EMC Engineering (2009) 式 6-33)。"
              "理想素子・独立経路を仮定した見積りで、実測値ではありません。",
              "▸ Improvements are computed with the classical expressions below "
              "(insertion loss IL = 20log10(1+Z/Zc), the definition used in "
              "CISPR 17; aperture SE ∝ 20log10(1/L), H. W. Ott, EMC Engineering "
              "(2009) eq. 6-33). They are estimates for ideal components and "
              "independent paths — not measured values.");
    I18n::reg("emc_mit_freq", "評価周波数", "Evaluation frequency");
    I18n::reg("emc_mit_freq_unit", "MHz", "MHz");
    I18n::reg("emc_mit_zc", "回路インピーダンス Zc",
              "Circuit impedance Zc");
    I18n::reg("emc_mit_zc_unit",
              "Ω (コモンモードの基準値 150 Ω — CISPR 16-1-2)",
              "Ω (150 Ω common-mode reference — CISPR 16-1-2)");
    I18n::reg("emc_col_mit", "対策", "Countermeasure");
    I18n::reg("emc_col_param", "パラメータ", "Parameter");
    I18n::reg("emc_col_gain", "改善量 [dB]", "Improvement [dB]");
    I18n::reg("emc_col_cost", "コスト", "Cost");
    I18n::reg("emc_mit_ferrite", "ケーブルにフェライトコア (コア Z [Ω])",
              "Ferrite core on the cable (core Z [Ω])");
    I18n::reg("emc_mit_slit", "筐体スリットを縮める (幅比 L_after/L_before)",
              "Shrink the enclosure slit (width ratio L_after/L_before)");
    I18n::reg("emc_mit_via", "GNDビア追加 (スティッチング) — 定量モデル無し",
              "Add GND vias (stitching) — no quantitative model");
    I18n::reg("emc_mit_choke", "コモンモードチョーク (L_cm [μH])",
              "Common-mode choke (L_cm [μH])");
    I18n::reg("emc_cost_low", "低", "Low");
    I18n::reg("emc_cost_mid", "中", "Medium");
    I18n::reg("emc_mit_total", "選択した対策の合計改善量: %1 dB",
              "Total improvement of the selected measures: %1 dB");
    I18n::reg("emc_mit_abs",
              "▸ 対策前のエミッションレベルが無いため、対策後の絶対レベル "
              "[dBμV/m] は算出しません (合計は独立な経路と仮定した単純和)。"
              "コモンモードチョークは理想インダクタとして計算しており、"
              "実素子の自己共振より上では過大評価になります。",
              "▸ Without a pre-mitigation emission level the absolute level after "
              "mitigation [dBμV/m] is not computed (the total is a plain sum "
              "assuming independent paths). The common-mode choke is treated as an "
              "ideal inductor, so the figure is optimistic above its "
              "self-resonance.");

    // 筐体シールドの遮蔽効果 (対策検討の中)
    I18n::reg("emc_shield_title",
              "筐体シールドの遮蔽効果 SE = A + R + B (平面波・無限平板)",
              "Enclosure shielding effectiveness SE = A + R + B "
              "(plane wave, infinite sheet)");
    I18n::reg("emc_shield_mat", "シールド材", "Shield material");
    I18n::reg("emc_mat_cu", "銅", "Copper");
    I18n::reg("emc_mat_al", "アルミニウム", "Aluminum");
    I18n::reg("emc_mat_steel", "鋼 (低炭素)", "Steel (low carbon)");
    I18n::reg("emc_mat_sus", "ステンレス SUS304", "Stainless steel 304");
    I18n::reg("emc_mat_mu", "パーマロイ (ミューメタル)", "Permalloy (mu-metal)");
    I18n::reg("emc_shield_t", "板厚", "Thickness");
    I18n::reg("emc_shield_t_unit", "mm", "mm");
    I18n::reg("emc_ap_len", "開口の最長寸法", "Longest aperture dimension");
    I18n::reg("emc_ap_len_unit", "mm (0 = 開口なし)", "mm (0 = no aperture)");
    I18n::reg("emc_ap_n", "開口数", "Number of apertures");
    I18n::reg("emc_shield_out",
              "δ = %1 μm ／ A = %2 dB ／ R = %3 dB ／ B = %4 dB → SE = %5 dB",
              "δ = %1 μm / A = %2 dB / R = %3 dB / B = %4 dB → SE = %5 dB");
    I18n::reg("emc_shield_ap_out",
              "開口の SE = %1 dB → 正味 SE = min(板, 開口) = %2 dB",
              "Aperture SE = %1 dB → net SE = min(sheet, aperture) = %2 dB");
    I18n::reg("emc_shield_ap_none",
              "開口なし → 正味 SE = %1 dB (板のみ)",
              "No aperture → net SE = %1 dB (sheet only)");
    I18n::reg("emc_shield_src",
              "出典: S. A. Schelkunoff, Electromagnetic Waves (1943) / "
              "H. W. Ott, Electromagnetic Compatibility Engineering (2009) "
              "§6.4 (式 6-9, 6-11, 6-12)・§6.7 (式 6-33)、材料定数は同書 表 6-1。"
              "μr は低周波の代表値で、高周波では低下します。",
              "Sources: S. A. Schelkunoff, Electromagnetic Waves (1943); "
              "H. W. Ott, Electromagnetic Compatibility Engineering (2009) "
              "§6.4 (eqs. 6-9, 6-11, 6-12) and §6.7 (eq. 6-33); material constants "
              "from Table 6-1 of the same book. μr is a low-frequency value and "
              "falls off at high frequency.");
    I18n::reg("emc_shield_invalid",
              "板厚・周波数を正の値にすると SE を計算します。",
              "Enter a positive thickness and frequency to compute SE.");

    // 伝導エミッション
    I18n::reg("emc_cond_section", "伝導エミッション / Conducted emission",
              "Conducted emission");
    I18n::reg("emc_cond_setup", "測定系", "Measurement setup");
    I18n::reg("emc_cond_lisn", "LISN (AMN) 50Ω/50μH", "LISN (AMN) 50 Ω / 50 μH");
    I18n::reg("emc_cond_probe", "電流プローブ", "Current probe");
    I18n::reg("emc_cond_cdn", "CDN", "CDN");
    I18n::reg("emc_cond_range", "周波数範囲", "Frequency range");
    I18n::reg("emc_cond_range_unit", "〜30 MHz", "to 30 MHz");
    I18n::reg("emc_det_qp", "準尖頭値 (QP) 検波", "Quasi-peak (QP) detection");
    I18n::reg("emc_det_av", "平均値 (AV) 検波", "Average (AV) detection");
    I18n::reg("emc_cond_hint",
              "▸ PEEC抽出した基板寄生+電源フィルタ回路のSPICE共シミュレーションに"
              "よる算出は未実装です (PEEC 連携は未実装)。",
              "▸ Computation by SPICE co-simulation of the PEEC-extracted board "
              "parasitics plus the power-line filter is not implemented yet.");

    // イミュニティ
    I18n::reg("emc_imm_section", "イミュニティ / Immunity", "Immunity");
    I18n::reg("emc_imm_test", "試験", "Test");
    I18n::reg("emc_imm_rs", "放射イミュニティ (RS)",
              "Radiated susceptibility (RS)");
    I18n::reg("emc_imm_esd", "ESD", "ESD");
    I18n::reg("emc_imm_eft", "ファストトランジェント",
              "Electrical fast transient");
    I18n::reg("emc_imm_surge", "サージ", "Surge");
    I18n::reg("emc_imm_level", "試験レベル", "Test level");
    I18n::reg("emc_imm_level_unit", "V/m (80MHz〜6GHz, 80%AM)",
              "V/m (80 MHz–6 GHz, 80% AM)");
    I18n::reg("emc_esd_v", "ESD電圧", "ESD voltage");
    I18n::reg("emc_esd_v_unit", "kV (接触) / 15kV (気中)",
              "kV (contact) / 15 kV (air discharge)");
    I18n::reg("emc_imm_field", "筐体内部の電界分布を可視化",
              "Visualize the E-field distribution inside the enclosure");
    I18n::reg("emc_imm_induced", "基板上の誘導電圧を算出",
              "Compute the voltage induced on the board");
    // 試験レベルから規格の定義どおりに決まる量 (実計算)
    I18n::reg("emc_imm_derived", "試験レベルから決まる量 (規格の定義値)",
              "Quantities fixed by the test level (definitions of the standard)");
    I18n::reg("emc_imm_rs_out",
              "電界強度 %1 V/m → 電力密度 S = E²/Z0 = %2 W/m² (%3 mW/cm²)、"
              "80% AM (1 kHz) 変調時の尖頭包絡線 = 1.8×E = %4 V/m "
              "(IEC 61000-4-3 §6.2)",
              "Field strength %1 V/m → power density S = E²/Z0 = %2 W/m² "
              "(%3 mW/cm²); peak envelope with 80% AM (1 kHz) = 1.8×E = %4 V/m "
              "(IEC 61000-4-3 §6.2)");
    I18n::reg("emc_imm_esd_out",
              "接触放電 %1 kV → 第1ピーク %2 A ／ 30 ns で %3 A ／ 60 ns で %4 A "
              "(IEC 61000-4-2:2008 Table 2、波形 1 ns/60 ns)",
              "Contact discharge %1 kV → first peak %2 A / %3 A at 30 ns / "
              "%4 A at 60 ns (IEC 61000-4-2:2008 Table 2, 1 ns/60 ns waveform)");
    I18n::reg("emc_imm_eft_out",
              "ファストトランジェント: パルス 5/50 ns、バースト長 15 ms・"
              "周期 300 ms、繰返し 5 kHz または 100 kHz (IEC 61000-4-4:2012)。"
              "試験電圧は上の「試験レベル」欄の値。",
              "Electrical fast transient: 5/50 ns pulse, 15 ms burst every 300 ms, "
              "5 kHz or 100 kHz repetition (IEC 61000-4-4:2012). The test voltage "
              "is the value in the “Test level” field above.");
    I18n::reg("emc_imm_surge_out",
              "サージ: 開放電圧 1.2/50 μs、短絡電流 8/20 μs、"
              "発生器インピーダンス 2 Ω (線間) / 12 Ω (線-大地) "
              "(IEC 61000-4-5:2014)。",
              "Surge: 1.2/50 μs open-circuit voltage, 8/20 μs short-circuit "
              "current, 2 Ω generator impedance (line-to-line) / 12 Ω "
              "(line-to-ground) (IEC 61000-4-5:2014).");
    I18n::reg("emc_imm_verdict",
              "筐体内部の電界分布・基板上の誘導電圧・クリティカル判定: —",
              "Internal field distribution, induced board voltage, criticality: —");
    I18n::reg("emc_imm_verdict_note",
              "▸ 誘導電圧の算出には、この試験配置で筐体内部の電界を解いた "
              "FDTD の実行結果と、基板配線への結合モデルが必要です。"
              "カーネル実行と結果の取り込みが未実装のため、判定は出しません "
              "(推定値も表示しません)。",
              "▸ Computing the induced voltage requires an FDTD run of this test "
              "setup for the field inside the enclosure plus a coupling model to "
              "the board traces. Running the kernel and importing its results is "
              "not implemented, so no verdict — and no estimate — is shown.");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-table / Seg 相当) ───────────────
const char kAcc[]  = "#0078D4";     // badge acc / var(--acc)
const char kWarn[] = "#B45309";     // badge warn
const char kErr[]  = "#B91C1C";     // badge err

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

// <Seg> 相当: 排他 checkable QPushButton 行を 1 ウィジェットに畳む
QWidget *segRow(QWidget *parent, QButtonGroup **out, const QStringList &labels,
                int current)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(1);
    auto *grp = new QButtonGroup(w);
    grp->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels[i], w);
        b->setCheckable(true);
        b->setStyleSheet("padding:2px 10px;");
        grp->addButton(b, i);
        h->addWidget(b);
    }
    if (auto *b = grp->button(current)) b->setChecked(true);
    h->addStretch(1);
    if (out) *out = grp;
    return w;
}

QTableWidgetItem *textItem(const QString &s) { return new QTableWidgetItem(s); }

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QTableWidgetItem *monoItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    QFont f = it->font();
    // 実在するファミリのみ指定 (Theme が環境ごとに解決済み)
    if (const QString mf = Theme::monoFontFamily(); !mf.isEmpty())
        f.setFamily(mf);
    f.setStyleHint(QFont::Monospace);
    it->setFont(f);
    return it;
}

// 先頭列のチェックボックスセル (mock の <input type="checkbox">)
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
    t->verticalHeader()->setDefaultSectionSize(24);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
    return t;
}

// 適用規格コンボ (index) → 限度値表を持つ規格。持たないものは None。
//   0=CISPR 32  1=CISPR 25  2=FCC Part 15  3=IEC 61000-4-3 (イミュニティ)
//   4=IEC 61000-4-2 (ESD)  5=DO-160  6=MIL-STD-461
em::emc::Standard standardFromIndex(int idx)
{
    switch (idx) {
    case 0: return em::emc::Standard::Cispr32;
    case 2: return em::emc::Standard::Fcc15;
    default: return em::emc::Standard::None;
    }
}

// 対策 4 行の定義 (パラメータの意味と既定値はコード側で固定、値は編集可能)
struct MitDef {
    const char *nameKey;
    const char *costKey;
    const char *defParam;   // 空 = パラメータ無し (定量モデルが無い行)
};
const MitDef kMit[4] = {
    { "emc_mit_ferrite", "emc_cost_low", "300"  },  // コア Z [Ω]
    { "emc_mit_slit",    "emc_cost_mid", "0.5"  },  // 幅比 L_after/L_before
    { "emc_mit_via",     "emc_cost_low", ""     },  // モデル無し
    { "emc_mit_choke",   "emc_cost_mid", "1.0"  },  // L_cm [μH]
};

// シールド材コンボ (順序は em::emc::shieldMaterial() の添字と一致させる)
const char *kShieldMatKeys[em::emc::kShieldMaterialCount] = {
    "emc_mat_cu", "emc_mat_al", "emc_mat_steel", "emc_mat_sus", "emc_mat_mu"
};

// 「—」セル (未計算であることを示す。灰色にして数値と区別する)
QTableWidgetItem *dashItem()
{
    auto *it = new QTableWidgetItem(QStringLiteral("—"));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setForeground(QColor("#7A7A7A"));
    return it;
}
} // namespace

// ── EmcTab ──────────────────────────────────────────────────────────────────
EmcTab::EmcTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── EMC / EMI 解析 (概要 + モード + 適用規格) ───────────────────────────
    auto *st = new SectionBox(I18n::tr("emc_title"), body);
    st->vbox()->addWidget(makeHint(I18n::tr("emc_hint"), st));
    st->vbox()->addWidget(segRow(st, &m_mode,
                                 { I18n::tr("emc_mode_emission"),
                                   I18n::tr("emc_mode_conducted"),
                                   I18n::tr("emc_mode_immunity") }, 0));
    m_standard = new QComboBox(st);
    m_standard->addItems({ I18n::tr("emc_std_cispr32"),
                           I18n::tr("emc_std_cispr25"),
                           I18n::tr("emc_std_fcc15"),
                           I18n::tr("emc_std_iec4_3"),
                           I18n::tr("emc_std_iec4_2"),
                           I18n::tr("emc_std_do160"),
                           I18n::tr("emc_std_mil461") });
    m_standard->setCurrentIndex(0);          // mock: defaultValue="cispr32"
    st->form()->addRow(I18n::tr("emc_standard"), m_standard);
    v->addWidget(st);

    // ── モード別セクション群 (show/hide で切替) ─────────────────────────────
    m_emissionPage  = buildEmissionPage();
    m_conductedPage = buildConductedPage();
    m_immunityPage  = buildImmunityPage();
    v->addWidget(m_emissionPage);
    v->addWidget(m_conductedPage);
    v->addWidget(m_immunityPage);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_mode, &QButtonGroup::idClicked, this, [this](int id) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_modeIdx = id;
        onModeChanged();
    });
    // 適用規格を変えると限度値表・曲線が変わる
    connect(m_standard, &QComboBox::currentIndexChanged,
            this, &EmcTab::updateCompliance);
    connect(project, &Project::loaded, this, &EmcTab::refresh);
    refresh();
}

void EmcTab::refresh()
{
    m_updating = true;
    if (auto *b = m_mode->button(m_modeIdx)) b->setChecked(true);
    m_updating = false;
    onModeChanged();
    // 限度値・対策・イミュニティの表示はプロジェクト (解析周波数) にも依存する
    updateCompliance();
    updateMitigation();
    updateImmunity();
}

void EmcTab::onModeChanged()
{
    m_emissionPage->setVisible(m_modeIdx == 0);
    m_conductedPage->setVisible(m_modeIdx == 1);
    m_immunityPage->setVisible(m_modeIdx == 2);
}

// ── 放射エミッション: 試験配置 / 放射源 / 判定結果 / 対策検討 ───────────────
QWidget *EmcTab::buildEmissionPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // 試験配置 / Test setup
    auto *ss = new SectionBox(I18n::tr("emc_setup_section"), page);
    ss->form()->addRow(I18n::tr("emc_site"),
                       segRow(ss, &m_site, { I18n::tr("emc_site_oats"),
                                             I18n::tr("emc_site_semi"),
                                             I18n::tr("emc_site_full"),
                                             I18n::tr("emc_site_rev") }, 0));
    m_distance = numEdit("3", ss);
    ss->form()->addRow(I18n::tr("emc_distance"),
                       unitRow(m_distance, I18n::tr("emc_distance_unit"), ss));
    m_antHeight = numEdit("1.0", ss);
    ss->form()->addRow(I18n::tr("emc_ant_h"),
                       unitRow(m_antHeight, I18n::tr("emc_ant_h_unit"), ss));
    m_turnTable = makeCheck(I18n::tr("emc_eut_turn"), true, ss);
    m_bothPol   = makeCheck(I18n::tr("emc_eut_pol"), true, ss);
    ss->form()->addRow(I18n::tr("emc_eut"),
                       checkRow({ m_turnTable, m_bothPol }));
    m_gndPec   = makeCheck(I18n::tr("emc_gnd_pec"), true, ss);
    m_gndCable = makeCheck(I18n::tr("emc_gnd_cable"), true, ss);
    ss->form()->addRow(I18n::tr("emc_gnd"),
                       checkRow({ m_gndPec, m_gndCable }));
    // 試験配置フォームはどこにも読まれていない (未実装)
    ss->vbox()->addWidget(ofd::tabhelp::unwiredNote(ss));
    v->addWidget(ss);

    // 放射源 / Emission sources
    auto *se = new SectionBox(I18n::tr("emc_src_section"), page);
    m_srcSwitching  = makeCheck(I18n::tr("emc_src_switching"), true, se);
    m_srcCommonMode = makeCheck(I18n::tr("emc_src_cm"), true, se);
    m_srcSlit       = makeCheck(I18n::tr("emc_src_slit"), true, se);
    se->vbox()->addLayout(checkRow({ m_srcSwitching }));
    se->vbox()->addLayout(checkRow({ m_srcCommonMode, m_srcSlit }));
    m_clock = numEdit("100", se);
    se->form()->addRow(I18n::tr("emc_clock"),
                       unitRow(m_clock, I18n::tr("emc_clock_unit"), se));
    // 放射源フォームはどこにも読まれていない (未実装)
    se->vbox()->addWidget(ofd::tabhelp::unwiredNote(se));
    v->addWidget(se);

    // 判定結果 / Compliance check
    //   限度値 = 規格の公表値 (実データ)、被測定値 = 未取得 → 「—」。
    //   両者を混同させないよう注記で分けて説明する (絶対規則 5)。
    auto *sc = new SectionBox(I18n::tr("emc_check_section"), page);
    m_class = new QComboBox(sc);
    m_class->addItem(I18n::tr("emc_class_a"));
    m_class->addItem(I18n::tr("emc_class_b"));
    m_class->setCurrentIndex(1);                 // 既定は厳しい方 (Class B)
    sc->form()->addRow(I18n::tr("emc_class"), m_class);

    m_compNote = makeHint(QString(), sc);
    sc->vbox()->addWidget(m_compNote);
    m_compTable = makeTable({ I18n::tr("emc_col_freq"), I18n::tr("emc_col_meas"),
                              I18n::tr("emc_col_limit"),
                              I18n::tr("emc_col_margin"),
                              I18n::tr("emc_col_verdict") }, 0, sc, 120);
    sc->vbox()->addWidget(m_compTable);
    m_distNote = makeHint(QString(), sc);
    sc->vbox()->addWidget(m_distNote);
    m_projFreqNote = makeHint(QString(), sc);
    sc->vbox()->addWidget(m_projFreqNote);

    m_spectrum = new MiniPlot(sc);
    m_spectrum->setMinimumSize(360, 130);        // mock: width=360 height=130
    sc->vbox()->addWidget(m_spectrum);

    connect(m_class, &QComboBox::currentIndexChanged,
            this, &EmcTab::updateCompliance);
    connect(m_distance, &QLineEdit::textChanged,
            this, &EmcTab::updateCompliance);

    // 放射源特定 / レポートのボタンは未配線 (絶対規則 5)
    auto *cb = new QHBoxLayout();
    auto *locateBtn = new QPushButton(I18n::tr("emc_btn_locate"), sc);
    auto *reportBtn = new QPushButton(I18n::tr("emc_btn_report"), sc);
    ofd::tabhelp::markNotImplemented(locateBtn);
    ofd::tabhelp::markNotImplemented(reportBtn);
    cb->addWidget(locateBtn);
    cb->addWidget(reportBtn);
    cb->addStretch(1);
    sc->vbox()->addLayout(cb);
    v->addWidget(sc);

    // 対策検討 / Mitigation — 改善量は入力値から古典式で実計算する
    auto *sm = new SectionBox(I18n::tr("emc_mit_section"), page);
    sm->vbox()->addWidget(makeHint(I18n::tr("emc_mit_note"), sm));
    // SectionBox::form() は最初の呼び出し位置に 1 個だけ置かれるので、
    // 対策表とシールド節を挟むために各ブロック専用のフォームを自前で作る
    auto *mitForm = new QFormLayout();
    mitForm->setContentsMargins(0, 0, 0, 0);
    m_mitFreq = numEdit("500", sm);
    mitForm->addRow(I18n::tr("emc_mit_freq"),
                    unitRow(m_mitFreq, I18n::tr("emc_mit_freq_unit"), sm));
    m_mitZc = numEdit("150", sm);
    mitForm->addRow(I18n::tr("emc_mit_zc"),
                    unitRow(m_mitZc, I18n::tr("emc_mit_zc_unit"), sm));
    sm->vbox()->addLayout(mitForm);

    m_mitTable = makeTable({ QString(), I18n::tr("emc_col_mit"),
                             I18n::tr("emc_col_param"),
                             I18n::tr("emc_col_gain"),
                             I18n::tr("emc_col_cost") }, 4, sm, 140);
    // パラメータ列だけ編集可能にする (改善量は計算結果なので編集させない)
    m_mitTable->setEditTriggers(QAbstractItemView::DoubleClicked |
                                QAbstractItemView::SelectedClicked |
                                QAbstractItemView::EditKeyPressed);
    sm->vbox()->addWidget(m_mitTable);
    buildMitigationRows();
    m_mitBadge = makeBadge(QString(), kAcc, sm);
    auto *mb = new QHBoxLayout();
    mb->addWidget(m_mitBadge);
    mb->addStretch(1);
    sm->vbox()->addLayout(mb);
    sm->vbox()->addWidget(makeHint(I18n::tr("emc_mit_abs"), sm));

    // 筐体シールドの遮蔽効果 (SE = A + R + B) — 同じ評価周波数を使う
    auto *shTitle = new QLabel(I18n::tr("emc_shield_title"), sm);
    shTitle->setWordWrap(true);
    QFont shFont = shTitle->font();
    shFont.setBold(true);
    shTitle->setFont(shFont);
    sm->vbox()->addWidget(shTitle);

    auto *shForm = new QFormLayout();
    shForm->setContentsMargins(0, 0, 0, 0);
    m_shieldMat = new QComboBox(sm);
    for (int i = 0; i < em::emc::kShieldMaterialCount; ++i)
        m_shieldMat->addItem(I18n::tr(kShieldMatKeys[i]));
    shForm->addRow(I18n::tr("emc_shield_mat"), m_shieldMat);
    m_shieldThick = numEdit("1.0", sm);
    shForm->addRow(I18n::tr("emc_shield_t"),
                   unitRow(m_shieldThick, I18n::tr("emc_shield_t_unit"), sm));
    m_apLen = numEdit("50", sm);
    shForm->addRow(I18n::tr("emc_ap_len"),
                   unitRow(m_apLen, I18n::tr("emc_ap_len_unit"), sm));
    m_apCount = numEdit("1", sm);
    shForm->addRow(I18n::tr("emc_ap_n"), unitRow(m_apCount, QString(), sm));
    sm->vbox()->addLayout(shForm);
    m_shieldOut = makeHint(QString(), sm);
    sm->vbox()->addWidget(m_shieldOut);
    m_shieldNet = makeHint(QString(), sm);
    sm->vbox()->addWidget(m_shieldNet);
    sm->vbox()->addWidget(makeHint(I18n::tr("emc_shield_src"), sm));
    v->addWidget(sm);

    for (QLineEdit *e : { m_mitFreq, m_mitZc, m_shieldThick, m_apLen, m_apCount })
        connect(e, &QLineEdit::textChanged, this, &EmcTab::updateMitigation);
    connect(m_shieldMat, &QComboBox::currentIndexChanged,
            this, &EmcTab::updateMitigation);
    connect(m_mitTable, &QTableWidget::itemChanged,
            this, [this](QTableWidgetItem *) { updateMitigation(); });

    return page;
}

// ── 判定結果: 限度値 (実データ) を表と曲線に、被測定値は「—」 ───────────────
void EmcTab::updateCompliance()
{
    using namespace ofd::em;

    const emc::Standard std_ = standardFromIndex(m_standard->currentIndex());
    const emc::EmClass cls = (m_class->currentIndex() == 0) ? emc::EmClass::A
                                                            : emc::EmClass::B;
    emc::LimitSegment seg[emc::kMaxLimitSegments];
    const int n = emc::radiatedLimits(std_, cls, seg, emc::kMaxLimitSegments);

    // 測定距離 (試験配置の入力欄)。不正入力は規定距離のまま扱う。
    bool okDist = false;
    const double dist = m_distance->text().toDouble(&okDist);
    const double d = (okDist && dist > 0) ? dist : 0.0;

    m_compTable->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const double lim = emc::limitAtDistance(seg[i], d);
        m_compTable->setItem(i, 0, monoItem(
            QStringLiteral("%1–%2 MHz").arg(seg[i].f1_MHz, 0, 'g', 4)
                                       .arg(seg[i].f2_MHz, 0, 'g', 4)));
        m_compTable->setItem(i, 1, dashItem());          // 実測相当値: 未取得
        m_compTable->setItem(i, 2, numItem(
            QStringLiteral("%1 dBμV/m").arg(lim, 0, 'f', 1)));
        m_compTable->setItem(i, 3, dashItem());          // マージン: 算出不可
        m_compTable->setItem(i, 4, dashItem());          // 判定: 出さない
    }

    if (n == 0) {
        m_compNote->setText(I18n::tr("emc_limit_none"));
        m_distNote->setText(QString());
        m_spectrum->setSeries({});
        m_spectrum->setLabels("f [MHz]", "dBμV/m");
        return;
    }

    m_compNote->setText(I18n::tr("emc_limit_note"));
    const double dref = seg[0].refDist_m;
    if (d > 0 && std::fabs(d - dref) > 1e-9)
        m_distNote->setText(I18n::tr("emc_limit_dist")
                                .arg(dref, 0, 'g', 3).arg(d, 0, 'g', 3));
    else
        m_distNote->setText(I18n::tr("emc_limit_dist_same").arg(dref, 0, 'g', 3));

    // 限度値カーブ (階段状): 各区間の両端に点を打つ
    MiniSeries lim;
    lim.color = QColor(kErr);
    lim.label = I18n::tr("emc_col_limit");
    for (int i = 0; i < n; ++i) {
        const double y = emc::limitAtDistance(seg[i], d);
        lim.pts.push_back({ seg[i].f1_MHz, y });
        lim.pts.push_back({ seg[i].f2_MHz, y });
    }
    m_spectrum->setSeries({ lim });
    m_spectrum->setLabels("f [MHz]",
                          (d > 0) ? QStringLiteral("dBμV/m @%1m")
                                        .arg(d, 0, 'g', 3)
                                  : QStringLiteral("dBμV/m"));

    // プロジェクトの解析周波数と限度値表の範囲の関係を示す
    const GeneralOpts &g = m_p->general();
    const QString band = (g.f1min > 0 && g.f1max > 0)
        ? QStringLiteral("%1 – %2 MHz").arg(g.f1min * 1e-6, 0, 'g', 4)
                                       .arg(g.f1max * 1e-6, 0, 'g', 4)
        : QStringLiteral("—");
    m_projFreqNote->setText(I18n::tr("emc_proj_freq").arg(band));
}

// ── 対策検討: 行 (チェック / 名称 / パラメータ / 改善量 / コスト) を作る ─────
void EmcTab::buildMitigationRows()
{
    m_updating = true;
    for (int r = 0; r < 4; ++r) {
        const MitDef &row = kMit[r];
        const bool hasParam = row.defParam[0] != '\0';
        m_mitTable->setItem(r, 0, checkItem(hasParam));   // モデル無しは既定オフ
        auto *name = textItem(I18n::tr(row.nameKey));
        name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_mitTable->setItem(r, 1, name);
        if (hasParam) {
            auto *p = numItem(QString::fromUtf8(row.defParam));
            p->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                        Qt::ItemIsEditable);
            m_mitTable->setItem(r, 2, p);
        } else {
            m_mitTable->setItem(r, 2, dashItem());
        }
        m_mitTable->setItem(r, 3, dashItem());
        auto *cost = textItem(I18n::tr(row.costKey));
        cost->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_mitTable->setItem(r, 4, cost);
    }
    m_updating = false;
}

// ── 対策検討: 改善量とシールド SE を計算して表示する ────────────────────────
void EmcTab::updateMitigation()
{
    using namespace ofd::em;
    if (m_updating) return;

    const double f_MHz = m_mitFreq->text().toDouble();
    const double f_Hz = f_MHz * 1e6;
    const double zc = m_mitZc->text().toDouble();

    double total = 0;
    m_updating = true;                       // setText の itemChanged を無視
    for (int r = 0; r < 4; ++r) {
        const bool hasParam = kMit[r].defParam[0] != '\0';
        double gain = 0;
        bool known = false;
        if (hasParam && f_Hz > 0) {
            const double v = m_mitTable->item(r, 2)
                                 ? m_mitTable->item(r, 2)->text().toDouble()
                                 : 0.0;
            if (r == 0) {                    // フェライトコア: 直列 Z の挿入損失
                gain = emc::insertionLoss_dB(v, zc);
                known = (v > 0 && zc > 0);
            } else if (r == 1) {             // スリット幅を縮める
                gain = emc::apertureShrinkGain_dB(v);
                known = (v > 0 && v < 1.0);
            } else if (r == 3) {             // コモンモードチョーク (理想 L)
                const double z = emc::inductiveReactance(f_Hz, v * 1e-6);
                gain = emc::insertionLoss_dB(z, zc);
                known = (v > 0 && zc > 0);
            }
        }
        if (known) {
            auto *it = numItem(QStringLiteral("%1").arg(gain, 0, 'f', 1));
            m_mitTable->setItem(r, 3, it);
            if (m_mitTable->item(r, 0) &&
                m_mitTable->item(r, 0)->checkState() == Qt::Checked)
                total += gain;
        } else {
            m_mitTable->setItem(r, 3, dashItem());
        }
    }
    m_updating = false;
    m_mitBadge->setText(I18n::tr("emc_mit_total")
                            .arg(QString::number(total, 'f', 1)));

    // 筐体シールド SE = A + R + B と開口による制限
    const double t_m = m_shieldThick->text().toDouble() * 1e-3;
    const emc::ShieldMaterial &mat =
        emc::shieldMaterial(m_shieldMat->currentIndex());
    const emc::ShieldSE se =
        emc::shieldEffectiveness(f_Hz, t_m, mat.sigmaRel, mat.muRel);
    if (!se.valid) {
        m_shieldOut->setText(I18n::tr("emc_shield_invalid"));
        m_shieldNet->setText(QString());
        return;
    }
    m_shieldOut->setText(I18n::tr("emc_shield_out")
        .arg(se.skinDepth_m * 1e6, 0, 'f', 2)
        .arg(se.absorption_dB, 0, 'f', 1)
        .arg(se.reflection_dB, 0, 'f', 1)
        .arg(se.multiRefl_dB, 0, 'f', 1)
        .arg(se.total_dB, 0, 'f', 1));

    const double apLen_m = m_apLen->text().toDouble() * 1e-3;
    const int apN = std::max(1, m_apCount->text().toInt());
    if (apLen_m > 0) {
        const double apSE = emc::apertureSE_dB(f_Hz, apLen_m, apN);
        m_shieldNet->setText(I18n::tr("emc_shield_ap_out")
            .arg(apSE, 0, 'f', 1)
            .arg(std::min(se.total_dB, apSE), 0, 'f', 1));
    } else {
        m_shieldNet->setText(I18n::tr("emc_shield_ap_none")
                                 .arg(se.total_dB, 0, 'f', 1));
    }
}

// ── 伝導エミッション ────────────────────────────────────────────────────────
QWidget *EmcTab::buildConductedPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("emc_cond_section"), page);
    s->form()->addRow(I18n::tr("emc_cond_setup"),
                      segRow(s, &m_condSetup, { I18n::tr("emc_cond_lisn"),
                                                I18n::tr("emc_cond_probe"),
                                                I18n::tr("emc_cond_cdn") }, 0));
    m_condFreq = numEdit("0.15", s);
    s->form()->addRow(I18n::tr("emc_cond_range"),
                      unitRow(m_condFreq, I18n::tr("emc_cond_range_unit"), s));
    m_detQp = makeCheck(I18n::tr("emc_det_qp"), true, s);
    m_detAv = makeCheck(I18n::tr("emc_det_av"), true, s);
    s->vbox()->addLayout(checkRow({ m_detQp, m_detAv }));
    s->vbox()->addWidget(makeHint(I18n::tr("emc_cond_hint"), s));
    // 伝導エミッションのフォームはどこにも読まれていない (未実装)
    s->vbox()->addWidget(ofd::tabhelp::unwiredNote(s));
    v->addWidget(s);

    return page;
}

// ── イミュニティ ────────────────────────────────────────────────────────────
QWidget *EmcTab::buildImmunityPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("emc_imm_section"), page);
    s->form()->addRow(I18n::tr("emc_imm_test"),
                      segRow(s, &m_immTest, { I18n::tr("emc_imm_rs"),
                                              I18n::tr("emc_imm_esd"),
                                              I18n::tr("emc_imm_eft"),
                                              I18n::tr("emc_imm_surge") }, 0));
    m_immLevel = numEdit("10", s);
    s->form()->addRow(I18n::tr("emc_imm_level"),
                      unitRow(m_immLevel, I18n::tr("emc_imm_level_unit"), s));
    m_esdVolt = numEdit("8", s);
    s->form()->addRow(I18n::tr("emc_esd_v"),
                      unitRow(m_esdVolt, I18n::tr("emc_esd_v_unit"), s));
    m_immField   = makeCheck(I18n::tr("emc_imm_field"), true, s);
    m_immInduced = makeCheck(I18n::tr("emc_imm_induced"), true, s);
    s->vbox()->addLayout(checkRow({ m_immField, m_immInduced }));
    // 電界分布の可視化・誘導電圧の算出は未実装 (チェックはどこにも読まれない)
    s->vbox()->addWidget(ofd::tabhelp::unwiredNote(s));

    // 試験レベルから規格の定義どおりに決まる量 (実計算)
    auto *dTitle = new QLabel(I18n::tr("emc_imm_derived"), s);
    dTitle->setWordWrap(true);
    QFont dFont = dTitle->font();
    dFont.setBold(true);
    dTitle->setFont(dFont);
    s->vbox()->addWidget(dTitle);
    m_immDerived = makeHint(QString(), s);
    s->vbox()->addWidget(m_immDerived);

    // 判定は出さない (筐体内部の電界も基板の誘導電圧も未算出 — 絶対規則 5)
    m_immBadge = makeBadge(I18n::tr("emc_imm_verdict"), kWarn, s);
    auto *bb = new QHBoxLayout();
    bb->addWidget(m_immBadge);
    bb->addStretch(1);
    s->vbox()->addLayout(bb);
    s->vbox()->addWidget(makeHint(I18n::tr("emc_imm_verdict_note"), s));
    v->addWidget(s);

    connect(m_immTest, &QButtonGroup::idClicked,
            this, [this](int) { updateImmunity(); });
    for (QLineEdit *e : { m_immLevel, m_esdVolt })
        connect(e, &QLineEdit::textChanged, this, &EmcTab::updateImmunity);

    return page;
}

// ── イミュニティ: 試験レベルから決まる量 (規格の定義値) を表示する ──────────
void EmcTab::updateImmunity()
{
    using namespace ofd::em;
    const int test = m_immTest ? m_immTest->checkedId() : 0;
    QString text;
    switch (test) {
    case 0: {                                   // 放射イミュニティ (RS)
        const double e = m_immLevel->text().toDouble();
        const double s = emc::powerDensity_Wm2(e);
        text = I18n::tr("emc_imm_rs_out")
                   .arg(e, 0, 'g', 4)
                   .arg(s, 0, 'f', 3)
                   .arg(s * 0.1, 0, 'f', 3)     // W/m² → mW/cm² (×0.1)
                   .arg(emc::amModulatedPeakField(e), 0, 'f', 1);
        break;
    }
    case 1: {                                   // ESD (接触放電)
        const double kv = m_esdVolt->text().toDouble();
        const emc::EsdContactCurrent c = emc::esdContactCurrent(kv);
        text = I18n::tr("emc_imm_esd_out")
                   .arg(kv, 0, 'g', 4)
                   .arg(c.firstPeak_A, 0, 'f', 1)
                   .arg(c.at30ns_A, 0, 'f', 1)
                   .arg(c.at60ns_A, 0, 'f', 1);
        break;
    }
    case 2: text = I18n::tr("emc_imm_eft_out"); break;
    default: text = I18n::tr("emc_imm_surge_out"); break;
    }
    m_immDerived->setText(text);
}
