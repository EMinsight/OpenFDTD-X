// ScatteringTab.cpp
#include "ScatteringTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"
#include "../io/KernelResultReader.h"
#include "../em/MieSphere.h"
#include "../em/RadarCrossSection.h"

#include <QCheckBox>
#include <QComboBox>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 sct_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("sct_title", "散乱特性 (OpenFDTD §2.15)", "Scattering (OpenFDTD §2.15)");
    I18n::reg("sct_hint",
              "平面波入射に対する散乱体の解析。RCS、シールド効果、透過率など。",
              "Analysis of a scatterer under plane-wave incidence: RCS, shielding "
              "effectiveness, transmittance and more.");

    // 入射波 / Incident wave
    I18n::reg("sct_inc", "入射波", "Incident wave");
    I18n::reg("sct_inc_dir", "入射方向 (θ, φ)", "Incidence direction (θ, φ)");
    I18n::reg("sct_pol", "偏波", "Polarization");
    I18n::reg("sct_pol_v", "V (TE)", "V (TE)");
    I18n::reg("sct_pol_h", "H (TM)", "H (TM)");
    I18n::reg("sct_pol_cp", "円偏波", "Circular");
    I18n::reg("sct_sweep", "入射角スイープ (バイスタティック)",
              "Incidence-angle sweep (bistatic)");
    I18n::reg("sct_sweep_range", "スイープ範囲", "Sweep range");
    I18n::reg("sct_points", "点", "points");
    I18n::reg("sct_inc_note",
              "θ/φ/偏波は波源タブの平面波 (planewave) と同じ設定を共有します。"
              "平面波の有効/無効は波源タブで切り替えます。",
              "θ/φ/polarization share the plane-wave (planewave) settings on "
              "the Source tab; enable or disable the plane wave there.");
    I18n::reg("sct_cp_notimpl",
              "円偏波はカーネル未対応 (未実装)",
              "Circular polarization is not supported by the kernel "
              "(not implemented)");
    I18n::reg("sct_sweep_notimpl",
              "▸ 円偏波はカーネル未対応 (未実装)",
              "▸ Circular polarization is not supported by the kernel "
              "(not implemented)");
    // ── 入射角スイープ (kernel/SweepRunner) ──
    I18n::reg("sct_sweep_axis", "振る角度", "Swept angle");
    I18n::reg("sct_sweep_how",
              "カーネルは 1 回の実行につき planewave を 1 組しか受け取らない"
              "ため、スイープは同じ入力の角度違いを「N 回実行」して行います"
              "(作業ディレクトリの sweep/sweep_000, 001, … に 1 点ずつ)。"
              "点数ぶん計算時間がかかります。",
              "The kernel takes only one planewave per run, so the sweep runs "
              "the same input N times, one angle per run (into "
              "sweep/sweep_000, 001, … under the working directory). "
              "It costs one full solve per point.");
    I18n::reg("sct_sweep_run", "スイープ実行", "Run sweep");
    I18n::reg("sct_sweep_stop", "中止", "Stop");
    I18n::reg("sct_sweep_csv", "CSV 保存", "Save CSV");
    I18n::reg("sct_sweep_idle", "未実行", "Not run yet");
    I18n::reg("sct_sweep_running", "実行中 %1 / %2 — %3",
              "Running %1 / %2 — %3");
    I18n::reg("sct_sweep_done", "完了 — %1 点", "Done — %1 points");
    I18n::reg("sct_sweep_failed", "失敗した点があります (%1 / %2 成功)",
              "Some points failed (%1 of %2 succeeded)");
    I18n::reg("sct_sweep_need",
              "スイープには 2 点以上と、始点 ≠ 終点 が要ります。",
              "A sweep needs at least 2 points, and from must differ from to.");
    I18n::reg("sct_sweep_col_angle", "角度 [°]", "Angle [deg]");
    I18n::reg("sct_sweep_col_status", "状態", "Status");
    I18n::reg("sct_sweep_col_peak", "E-abs ピーク [dB]", "Peak E-abs [dB]");
    I18n::reg("sct_sweep_col_dir", "出力先", "Output dir");
    I18n::reg("sct_sweep_ok", "成功", "ok");
    I18n::reg("sct_sweep_ng", "失敗", "failed");
    I18n::reg("sct_sweep_nofar",
              "far1d.log 無し (ポスト(2) で遠方界を有効に)",
              "no far1d.log (enable the far field on Post-Proc (2))");
    I18n::reg("sct_sweep_saveto", "スイープ結果を CSV で保存",
              "Save the sweep results as CSV");

    // RCS
    I18n::reg("sct_rcs", "RCS / レーダ断面積", "RCS / Radar cross-section");
    I18n::reg("sct_rcs_mono", "モノスタティックRCS σ(θ_inc=θ_obs)",
              "Monostatic RCS σ(θ_inc = θ_obs)");
    I18n::reg("sct_rcs_bi", "バイスタティックRCS σ(θ_inc, θ_obs)",
              "Bistatic RCS σ(θ_inc, θ_obs)");
    I18n::reg("sct_unit", "単位", "Unit");
    I18n::reg("sct_rcs_matrix", "偏波散乱行列 [HH, HV, VH, VV]",
              "Polarimetric scattering matrix [HH, HV, VH, VV]");

    // NTFF
    I18n::reg("sct_ntff", "近傍/遠方界変換 (NTFF)", "Near-to-far-field transform (NTFF)");
    I18n::reg("sct_ntff_extract", "散乱波抽出 (入射波除去)",
              "Scattered-field extraction (incident field removed)");
    I18n::reg("sct_ntff_surface", "変換面", "Transform surface");
    I18n::reg("sct_ntff_box", "直方体閉曲面", "Closed box surface");
    I18n::reg("sct_ntff_sphere", "球面", "Spherical surface");
    I18n::reg("sct_ntff_wide", "広帯域RCS (FFT)", "Wideband RCS (FFT)");

    // その他散乱量
    I18n::reg("sct_misc", "その他散乱量", "Other scattering quantities");
    I18n::reg("sct_m_se", "シールド効果 SE [dB]", "Shielding effectiveness SE [dB]");
    I18n::reg("sct_m_fss", "透過率 / 反射率 (FSS用)",
              "Transmittance / reflectance (for FSS)");
    I18n::reg("sct_m_abs", "吸収断面積 σ_abs", "Absorption cross-section σ_abs");
    I18n::reg("sct_m_ext", "消散断面積 σ_ext (光散乱)",
              "Extinction cross-section σ_ext (optical scattering)");
    I18n::reg("sct_m_mie", "効率係数 Q_sca, Q_abs (Mie)",
              "Efficiency factors Q_sca, Q_abs (Mie)");
    I18n::reg("sct_uw_rcs_ok",
              "モノスタティック / バイスタティックの選択・単位 "
              "(m² / dBsm / σ/λ²)・偏波散乱行列のうち入射偏波の列 — "
              "いずれも下の結果表に効きます",
              "the monostatic / bistatic selection, the unit "
              "(m2 / dBsm / sigma over lambda^2) and the column of the "
              "polarimetric matrix for the launched polarisation — they all "
              "apply to the result tables below");
    I18n::reg("sct_res_freq", "周波数", "Frequency");
    I18n::reg("sct_res_back", "後方散乱 (モノスタティック)",
              "Backscatter (monostatic)");
    I18n::reg("sct_res_fwd",  "前方散乱", "Forward scatter");
    I18n::reg("sct_res_ka",   "電気サイズ ka", "Electrical size ka");
    I18n::reg("sct_res_mie_b", "後方 (Mie 厳密解)", "Backscatter (exact Mie)");
    I18n::reg("sct_res_mie_f", "前方 (Mie 厳密解)", "Forward (exact Mie)");
    I18n::reg("sct_mie_note",
              "「その他散乱量」の Mie にチェックが入っているので、完全導体球の"
              "厳密解を並べて表示しています。球を直交格子で階段近似するので"
              "一致はしません — 桁と傾向が合っているかの目安です。"
              "形状が球 (shape 2) のときだけ計算します。",
              "The Mie box under \"other scattering quantities\" is checked, so "
              "the exact perfectly-conducting-sphere solution is shown "
              "alongside. The sphere is staircased on the Cartesian grid so the "
              "two will not agree - this is a sanity check on the order of "
              "magnitude and the trend. It is computed only when the shape is a "
              "sphere (shape 2).");
    I18n::reg("sct_mie_notsphere",
              "Mie 厳密解は球にだけ当てはまるので、この形状では出しません",
              "The exact Mie solution applies to a sphere only, so it is not "
              "shown for this shape");
    I18n::reg("sct_res_none",
              "▸ 実行結果の RCS はまだありません。平面波入射 (planewave) と "
              "frequency2 のあるプロジェクトを計算すると、カーネルが "
              "<kernel>.log へ後方 / 前方散乱断面積を書き、ここに出ます。",
              "▸ No computed RCS yet. Run a project that has a plane wave and "
              "frequency2 — the kernel writes the backward / forward cross "
              "sections to <kernel>.log and they appear here.");
    I18n::reg("sct_res_ok",
              "▸ %1 の「=== cross section ===」から %2 周波数。値はカーネルが"
              "出した m² の実値で、ここでは単位を換算しているだけです。"
              "ka は形状の最大半寸法 %3 m から求めています "
              "(球ならこれが半径そのもの。球以外では代表寸法の目安)。",
              "▸ %2 frequency point(s) from the “=== cross section ===” block "
              "of %1. The values are the kernel's own m² results; only the "
              "unit is converted here. ka uses the largest half-extent "
              "%3 m of the geometry (the radius itself for a sphere; only "
              "indicative for other shapes).");
    I18n::reg("sct_uw_rcs",
              "偏波散乱行列のうち入射偏波と直交する側の 2 要素 "
              "(1 回の実行では入射させた偏波の列しか出ません — 直交偏波で"
              "もう 1 回走らせる必要があります)",
              "the two elements of the polarimetric matrix for the orthogonal "
              "incidence (one run only yields the column for the polarisation "
              "actually launched; the other needs a second run)");
    I18n::reg("sct_bi_sec", "バイスタティック RCS (far1d.log)",
              "Bistatic RCS (far1d.log)");
    I18n::reg("sct_bi_col_plane", "面 / 周波数", "Plane / frequency");
    I18n::reg("sct_bi_col_back", "後方 (θ_inc)", "Backward (theta_inc)");
    I18n::reg("sct_bi_col_max", "最大", "Maximum");
    I18n::reg("sct_bi_col_maxat", "最大の角度", "Angle of maximum");
    I18n::reg("sct_bi_col_min", "最小", "Minimum");
    I18n::reg("sct_bi_none",
              "▸ far1d.log がまだありません (ポスト処理の「遠方界 1 次元」を"
              "有効にして実行すると出ます)。",
              "▸ There is no far1d.log yet (enable the 1-D far field in the "
              "post-processing options and run).");
    I18n::reg("sct_bi_notrcs",
              "▸ この問題には給電点があるため、far1d.log の値は入力電力で"
              "正規化された相対利得 [dB] であって RCS ではありません "
              "(カーネルの単位ラベルも [dBsm] ではなく [dB] になります)。"
              "バイスタティック RCS は平面波入射のみの問題で出ます。",
              "▸ This problem has feed points, so the values in far1d.log are "
              "relative gain [dB] normalised by the input power, not RCS (the "
              "kernel labels them [dB] rather than [dBsm]). Bistatic RCS is "
              "available for plane-wave-only problems.");
    I18n::reg("sct_bi_ok",
              "▸ %1 面 %2 本ぶん。平面波入射のみの問題なので far1d.log の "
              "E-abs 列はそのまま dBsm です (sol/farfield.c の遠方界係数が "
              "RCS 正規化を含むため)。",
              "▸ %2 pattern(s) over %1 plane(s). For a plane-wave-only problem "
              "the E-abs column of far1d.log is already dBsm (the far-field "
              "factor in sol/farfield.c carries the RCS normalisation).");
    I18n::reg("sct_mx_sec", "偏波散乱行列", "Polarimetric scattering matrix");
    I18n::reg("sct_mx_fmt",
              "入射 %1 偏波での後方散乱: σ_θ (co) = %2 / σ_φ (cross) = %3。\n"
              "直交偏波の列 (残る 2 要素) は入射偏波を変えてもう 1 回実行すると"
              "揃います。",
              "Backscatter for %1-polarised incidence: sigma_theta (co) = %2 / "
              "sigma_phi (cross) = %3.\nThe other column needs a second run "
              "with the orthogonal incident polarisation.");
    I18n::reg("sct_mx_nocols",
              "▸ far1d.log に偏波成分の列がありません (E-theta / E-phi)。",
              "▸ far1d.log carries no polarisation columns (E-theta / E-phi).");
    I18n::reg("sct_uw_ntff", "近傍界→遠方界変換の設定 (抽出面・広角オプション)",
              "the near-to-far-field settings (extraction surface, wide-angle option)");
    I18n::reg("sct_uw_misc", "SE・FSS・吸収率・消光のチェック",
              "the SE, FSS, absorptance and extinction check boxes");
    I18n::reg("sct_uw_misc_ok",
              "Mie のチェック — 上の結果表に完全導体球の厳密解を並べて表示します",
              "the Mie check box - it adds the exact perfectly-conducting-sphere "
              "solution alongside the results above");
    return true;
}();

const char *const kMiscKeys[] = { "sct_m_se", "sct_m_fss", "sct_m_abs",
                                  "sct_m_ext", "sct_m_mie" };
const bool kMiscOn[] = { false, false, false, false, false };

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}
} // namespace

ScatteringTab::ScatteringTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 散乱特性 (説明) ────────────────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("sct_title"), body);
    auto *hint = new QLabel(I18n::tr("sct_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);
    v->addWidget(sTop);

    // ── 入射波 / Incident wave ─────────────────────────────────────────────
    auto *sInc = new SectionBox(I18n::tr("sct_inc"), body);
    auto *dirRow = new QHBoxLayout();
    m_theta = numEdit("90", 70, sInc);
    m_phi   = numEdit("0", 70, sInc);
    dirRow->addWidget(m_theta);
    dirRow->addWidget(m_phi);
    dirRow->addWidget(new QLabel("°", sInc));
    dirRow->addStretch(1);
    sInc->form()->addRow(I18n::tr("sct_inc_dir"), dirRow);

    m_pol = new QComboBox(sInc);
    m_pol->addItem(I18n::tr("sct_pol_v"));       // index 0 → pol=1 (SourceTab と同一対応)
    m_pol->addItem(I18n::tr("sct_pol_h"));       // index 1 → pol=2
    m_pol->addItem(I18n::tr("sct_pol_cp"));      // index 2: カーネル未対応 → 選択不可
    if (auto *polModel = qobject_cast<QStandardItemModel *>(m_pol->model())) {
        QStandardItem *cp = polModel->item(2);
        cp->setFlags(cp->flags() & ~Qt::ItemIsEnabled);
        cp->setToolTip(I18n::tr("sct_cp_notimpl"));
    }
    sInc->form()->addRow(I18n::tr("sct_pol"), m_pol);

    // θ/φ/偏波は SourceTab の平面波と同じ Project::planewave() を編集する
    auto *incNote = new QLabel(I18n::tr("sct_inc_note"), sInc);
    incNote->setWordWrap(true);
    incNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sInc->form()->addRow(incNote);

    m_sweep = new QCheckBox(I18n::tr("sct_sweep"), sInc);
    sInc->form()->addRow(m_sweep);

    m_sweepAxis = new QComboBox(sInc);
    m_sweepAxis->addItem(QStringLiteral("θ"));   // index 0
    m_sweepAxis->addItem(QStringLiteral("φ"));   // index 1
    m_sweepAxis->setMaximumWidth(70);
    sInc->form()->addRow(I18n::tr("sct_sweep_axis"), m_sweepAxis);

    auto *swRow = new QHBoxLayout();
    m_sweepFrom = numEdit("0", 70, sInc);
    m_sweepTo   = numEdit("180", 70, sInc);
    m_sweepPts  = numEdit("37", 70, sInc);
    swRow->addWidget(m_sweepFrom);
    swRow->addWidget(new QLabel("〜", sInc));
    swRow->addWidget(m_sweepTo);
    swRow->addWidget(new QLabel("°  (", sInc));
    swRow->addWidget(m_sweepPts);
    swRow->addWidget(new QLabel(I18n::tr("sct_points") + QStringLiteral(")"), sInc));
    swRow->addStretch(1);
    sInc->form()->addRow(I18n::tr("sct_sweep_range"), swRow);

    // スイープが「N 回実行」であることを明示する (点数ぶん時間がかかる)
    auto *howNote = new QLabel(I18n::tr("sct_sweep_how"), sInc);
    howNote->setWordWrap(true);
    howNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sInc->form()->addRow(howNote);

    // 実行 / 進捗 / CSV
    auto *runRow = new QHBoxLayout();
    m_sweepRun = new QPushButton(I18n::tr("sct_sweep_run"), sInc);
    m_sweepCsv = new QPushButton(I18n::tr("sct_sweep_csv"), sInc);
    m_sweepCsv->setEnabled(false);
    m_sweepProgress = new QProgressBar(sInc);
    m_sweepProgress->setVisible(false);
    runRow->addWidget(m_sweepRun);
    runRow->addWidget(m_sweepCsv);
    runRow->addWidget(m_sweepProgress, 1);
    sInc->form()->addRow(runRow);
    m_sweepStatus = new QLabel(I18n::tr("sct_sweep_idle"), sInc);
    m_sweepStatus->setWordWrap(true);
    sInc->form()->addRow(m_sweepStatus);

    m_sweepTable = new QTableWidget(0, 4, sInc);
    m_sweepTable->setHorizontalHeaderLabels({
        I18n::tr("sct_sweep_col_angle"), I18n::tr("sct_sweep_col_status"),
        I18n::tr("sct_sweep_col_peak"), I18n::tr("sct_sweep_col_dir") });
    m_sweepTable->horizontalHeader()->setStretchLastSection(true);
    m_sweepTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sweepTable->setMinimumHeight(140);
    sInc->form()->addRow(m_sweepTable);

    // 円偏波のみ未実装 (θ/φ/偏波とスイープは配線済み — 絶対規則 5)
    auto *sweepNote = new QLabel(I18n::tr("sct_sweep_notimpl"), sInc);
    sweepNote->setWordWrap(true);
    sweepNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sInc->form()->addRow(sweepNote);
    v->addWidget(sInc);

    // ── RCS ────────────────────────────────────────────────────────────────
    auto *sRcs = new SectionBox(I18n::tr("sct_rcs"), body);
    m_rcsMono = new QCheckBox(I18n::tr("sct_rcs_mono"), sRcs);
    m_rcsMono->setChecked(true);
    sRcs->form()->addRow(m_rcsMono);
    m_rcsBi = new QCheckBox(I18n::tr("sct_rcs_bi"), sRcs);
    sRcs->form()->addRow(m_rcsBi);
    m_rcsUnit = new QComboBox(sRcs);
    m_rcsUnit->addItems({ "m²", "dBsm", "σ/λ²" });
    m_rcsUnit->setCurrentIndex(1);                // 既定 "dbsm"
    sRcs->form()->addRow(I18n::tr("sct_unit"), m_rcsUnit);
    m_rcsMatrix = new QCheckBox(I18n::tr("sct_rcs_matrix"), sRcs);
    sRcs->form()->addRow(m_rcsMatrix);
    sRcs->form()->addRow(
        tabhelp::unwiredNote(sRcs, I18n::tr("sct_uw_rcs"),
                             I18n::tr("sct_uw_rcs_ok")));
    // モノ / バイ / 散乱行列は下の結果表の内容を決める (refreshRcsResult)
    for (QCheckBox *c : { m_rcsMono, m_rcsBi, m_rcsMatrix })
        connect(c, &QCheckBox::toggled, this,
                [this](bool) { refreshRcsResult(); });

    // ── 実行結果の RCS (<kernel>.log の "=== cross section ===") ─────────
    // カーネルは平面波入射の問題について後方 / 前方散乱断面積を **m² の実値**
    // で書く (sol/outputChars.c:37 — IPlanewave && NFreq2 のときだけ)。
    // ここはそれを読んで単位を換算するだけで、値そのものはカーネルの出力。
    m_rcsResultNote = new QLabel(sRcs);
    m_rcsResultNote->setWordWrap(true);
    m_rcsResultNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sRcs->vbox()->addWidget(m_rcsResultNote);
    m_rcsTable = new QTableWidget(0, 6, sRcs);
    m_rcsTable->setHorizontalHeaderLabels(
        { I18n::tr("sct_res_freq"), I18n::tr("sct_res_back"),
          I18n::tr("sct_res_mie_b"), I18n::tr("sct_res_fwd"),
          I18n::tr("sct_res_mie_f"), I18n::tr("sct_res_ka") });
    m_rcsTable->horizontalHeader()->setStretchLastSection(true);
    m_rcsTable->verticalHeader()->setVisible(false);
    m_rcsTable->setMinimumHeight(110);
    m_rcsTable->setVisible(false);
    sRcs->vbox()->addWidget(m_rcsTable);
    connect(m_rcsUnit, &QComboBox::currentIndexChanged,
            this, &ScatteringTab::refreshRcsResult);

    // ── バイスタティック RCS / 偏波散乱行列 (far1d.log) ────────────────────
    auto *sBi = new SectionBox(I18n::tr("sct_bi_sec"), sRcs);
    m_biTable = new QTableWidget(0, 5, sBi);
    m_biTable->setHorizontalHeaderLabels(
        { I18n::tr("sct_bi_col_plane"), I18n::tr("sct_bi_col_back"),
          I18n::tr("sct_bi_col_max"), I18n::tr("sct_bi_col_maxat"),
          I18n::tr("sct_bi_col_min") });
    m_biTable->horizontalHeader()->setStretchLastSection(true);
    m_biTable->verticalHeader()->setVisible(false);
    m_biTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_biTable->setMinimumHeight(110);
    m_biTable->setVisible(false);
    sBi->vbox()->addWidget(m_biTable);
    m_biNote = new QLabel(sBi);
    m_biNote->setWordWrap(true);
    m_biNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sBi->vbox()->addWidget(m_biNote);
    m_mxNote = new QLabel(sBi);
    m_mxNote->setWordWrap(true);
    m_mxNote->setVisible(false);
    sBi->vbox()->addWidget(m_mxNote);
    sRcs->vbox()->addWidget(sBi);
    v->addWidget(sRcs);

    // ── 近傍/遠方界変換 / NTFF ─────────────────────────────────────────────
    auto *sNtff = new SectionBox(I18n::tr("sct_ntff"), body);
    m_ntffExtract = new QCheckBox(I18n::tr("sct_ntff_extract"), sNtff);
    m_ntffExtract->setChecked(true);
    sNtff->form()->addRow(m_ntffExtract);
    m_ntffSurface = new QComboBox(sNtff);
    m_ntffSurface->addItem(I18n::tr("sct_ntff_box"));
    m_ntffSurface->addItem(I18n::tr("sct_ntff_sphere"));
    m_ntffSurface->setCurrentIndex(0);            // 既定 "box"
    sNtff->form()->addRow(I18n::tr("sct_ntff_surface"), m_ntffSurface);
    m_ntffWide = new QCheckBox(I18n::tr("sct_ntff_wide"), sNtff);
    sNtff->form()->addRow(m_ntffWide);
    sNtff->form()->addRow(
        tabhelp::unwiredNote(sNtff, I18n::tr("sct_uw_ntff")));
    v->addWidget(sNtff);

    // ── その他散乱量 ──────────────────────────────────────────────────────
    auto *sMisc = checkSection(body, "sct_misc", kMiscKeys, kMiscOn,
                               int(sizeof(kMiscKeys) / sizeof(kMiscKeys[0])),
                               &m_misc);
    sMisc->vbox()->addWidget(tabhelp::unwiredNote(sMisc, I18n::tr("sct_uw_misc"),
                                                 I18n::tr("sct_uw_misc_ok")));
    // Mie のチェックは上の結果表に理論値の列を足す
    if (m_misc.size() > 4 && m_misc[4])
        connect(m_misc[4], &QCheckBox::toggled,
                this, &ScatteringTab::refreshRcsResult);
    v->addWidget(sMisc);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // ── スイープ実行 ─────────────────────────────────────────────────────
    m_sweeper = new SweepRunner(this);
    connect(m_sweeper, &SweepRunner::logLine, this, &ScatteringTab::sweepLog);
    connect(m_sweeper, &SweepRunner::pointStarted, this,
            [this](int i, int n, const QString &label) {
        m_sweepProgress->setRange(0, n);
        m_sweepProgress->setValue(i);
        m_sweepStatus->setText(
            I18n::tr("sct_sweep_running").arg(i + 1).arg(n).arg(label));
    });
    connect(m_sweeper, &SweepRunner::pointFinished, this,
            [this](int, const SweepResult &r) {
        const int row = m_sweepTable->rowCount();
        m_sweepTable->insertRow(row);
        m_sweepTable->setItem(row, 0, tabhelp::roItem(
            QString::number(r.value, 'g', 6)));
        m_sweepTable->setItem(row, 1, tabhelp::roItem(
            r.ok ? I18n::tr("sct_sweep_ok") : I18n::tr("sct_sweep_ng")));
        // ピークが無いのは「遠方界を出していない」ケース — 空欄にせず理由を出す
        m_sweepTable->setItem(row, 2, tabhelp::roItem(
            r.hasPeak ? QString::number(r.peakEAbs_dB, 'f', 3)
                      : I18n::tr("sct_sweep_nofar")));
        m_sweepTable->setItem(row, 3, tabhelp::roItem(
            QFileInfo(r.dir).fileName()));
        m_sweepProgress->setValue(m_sweepTable->rowCount());
    });
    connect(m_sweeper, &SweepRunner::finished, this, [this](bool ok) {
        const QVector<SweepResult> &rs = m_sweeper->results();
        int good = 0;
        for (const SweepResult &r : rs) if (r.ok) ++good;
        m_sweepStatus->setText(
            ok ? I18n::tr("sct_sweep_done").arg(rs.size())
               : I18n::tr("sct_sweep_failed").arg(good).arg(rs.size()));
        m_sweepProgress->setVisible(false);
        m_sweepRun->setText(I18n::tr("sct_sweep_run"));
        m_sweepCsv->setEnabled(!rs.isEmpty());
        updateSweepUi();
    });
    connect(m_sweepRun, &QPushButton::clicked, this, &ScatteringTab::startSweep);
    connect(m_sweepCsv, &QPushButton::clicked, this, &ScatteringTab::exportCsv);

    // スイープ範囲はスイープ ON のときだけ編集可 (モックのラベル順を維持)
    connect(m_sweep, &QCheckBox::toggled, this, [this] {
        apply();
        updateSweepUi();
    });
    updateSweepUi();

    // ── 入射波 (θ/φ/偏波) の配線: Project::planewave() の View ────────────
    connect(m_theta, &QLineEdit::editingFinished, this, &ScatteringTab::apply);
    connect(m_phi,   &QLineEdit::editingFinished, this, &ScatteringTab::apply);
    connect(m_pol, &QComboBox::currentIndexChanged, this, &ScatteringTab::apply);
    connect(m_sweepAxis, &QComboBox::currentIndexChanged, this,
            &ScatteringTab::apply);
    for (QLineEdit *e : { m_sweepFrom, m_sweepTo, m_sweepPts })
        connect(e, &QLineEdit::editingFinished, this, [this] {
            apply();
            updateSweepUi();
        });

    // SourceTab など他ビューでの平面波編集も反映する (同一モデルの共有)
    connect(project, &Project::loaded,  this, &ScatteringTab::refresh);
    connect(project, &Project::loaded,  this, &ScatteringTab::refreshRcsResult);
    connect(project, &Project::changed, this, &ScatteringTab::refresh);
    refresh();
    // 実行結果の RCS はモデルではなくファイル (<kernel>.log) から来るので、
    // 構築時にも一度読む (loaded シグナルはタブ生成前に飛んでいることがある)
    refreshRcsResult();
}

// widgets → model。入射波 (θ/φ/偏波) のみ。平面波の有効/無効は SourceTab が
// 受け持つため enabled には触れない (planewave 行の書出条件は enabled)。
void ScatteringTab::apply()
{
    if (m_updating) return;
    PlaneWave &pw = m_p->planewave();
    pw.theta = m_theta->text().toDouble();
    pw.phi   = m_phi->text().toDouble();
    // pol 対応は SourceTab と同一: index 0 = V → 1, index 1 = H → 2。
    // 円偏波 (index 2) は選択不可だが、万一の場合もモデルへは書かない。
    if (m_pol->currentIndex() >= 0 && m_pol->currentIndex() <= 1)
        pw.pol = m_pol->currentIndex() + 1;

    ScatteringOpts &s = m_p->scattering();
    s.sweepEnabled = m_sweep->isChecked();
    s.sweepAxis = qBound(0, m_sweepAxis->currentIndex(), 1);
    s.sweepFrom_deg = m_sweepFrom->text().toDouble();
    s.sweepTo_deg = m_sweepTo->text().toDouble();
    s.sweepPoints = m_sweepPts->text().toInt();
    m_p->touch();
}

// スイープ設定の有効性で操作可否を決める。
// 不正な範囲のまま「実行」を押せると、押してから初めて失敗する
// (.claude/rules/gui.md: 警告表示だけで済ませず実行をブロックする)。
void ScatteringTab::updateSweepUi()
{
    const bool on = m_sweep->isChecked();
    const bool busy = m_sweeper && m_sweeper->isRunning();
    m_sweepAxis->setEnabled(on && !busy);
    m_sweepFrom->setEnabled(on && !busy);
    m_sweepTo->setEnabled(on && !busy);
    m_sweepPts->setEnabled(on && !busy);

    const ScatteringOpts &s = m_p->scattering();
    const bool valid = s.sweepValid();
    m_sweepRun->setEnabled(on && (busy || valid));
    m_sweepRun->setToolTip(valid ? QString() : I18n::tr("sct_sweep_need"));
    if (!busy && on && !valid)
        m_sweepStatus->setText(I18n::tr("sct_sweep_need"));
}

void ScatteringTab::startSweep()
{
    if (m_sweeper->isRunning()) {   // 実行中の押下は中止
        m_sweeper->stop();
        return;
    }
    const ScatteringOpts &s = m_p->scattering();
    if (!s.sweepValid()) {
        m_sweepStatus->setText(I18n::tr("sct_sweep_need"));
        return;
    }
    SweepConfig cfg;
    cfg.kind = (s.sweepAxis == 1) ? SweepKind::PlaneWavePhi
                                  : SweepKind::PlaneWaveTheta;
    cfg.from = s.sweepFrom_deg;
    cfg.to = s.sweepTo_deg;
    cfg.points = s.sweepPoints;
    cfg.run = m_runCfg;
    // スイープは 1 点ずつ完結させる (ポストまで走らせて far1d.log を出す)
    cfg.run.mode = RunMode::Both;
    cfg.run.kernel = Runner::kernelForProject(*m_p);

    m_sweepTable->setRowCount(0);
    m_sweepCsv->setEnabled(false);
    m_sweepProgress->setVisible(true);
    m_sweepProgress->setRange(0, cfg.points);
    m_sweepProgress->setValue(0);
    if (!m_sweeper->start(*m_p, cfg)) {
        m_sweepProgress->setVisible(false);
        m_sweepStatus->setText(I18n::tr("sct_sweep_need"));
        return;
    }
    m_sweepRun->setText(I18n::tr("sct_sweep_stop"));
    updateSweepUi();
}

void ScatteringTab::exportCsv()
{
    const QVector<SweepResult> &rs = m_sweeper->results();
    if (rs.isEmpty()) return;
    const QString p = QFileDialog::getSaveFileName(
        this, I18n::tr("sct_sweep_saveto"),
        QStringLiteral("sweep.csv"), QStringLiteral("CSV (*.csv)"));
    if (p.isEmpty()) return;
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit sweepLog(QStringLiteral("sweep: cannot write %1: %2")
                          .arg(p, f.errorString()));
        return;
    }
    f.write(SweepRunner::toCsv(rs).toUtf8());
    emit sweepLog(QStringLiteral("sweep: wrote %1").arg(p));
}

// model → widgets (m_updating ガード付き)
// ── 実行結果の RCS を読み直す ───────────────────────────────────────────────
// <kernel>.log の "=== cross section ===" を読み、選択中の単位で表に出す。
// 無ければ表を隠して理由を出す (空欄を並べない — 絶対規則 5)。
void ScatteringTab::refreshRcsResult()
{
    if (!m_rcsTable) return;
    m_rcsTable->setRowCount(0);
    m_rcsTable->setVisible(false);

    if (m_p->filePath().isEmpty()) {
        m_rcsResultNote->setText(I18n::tr("sct_res_none"));
        return;
    }
    const QFileInfo fi(m_p->filePath());
    // 実行はプロジェクトの隣に <ケース名>.log を作る (Runner の作業ディレクトリ)
    const QString logPath = fi.path() + QLatin1Char('/')
                            + fi.completeBaseName() + QStringLiteral(".log");
    QVector<CrossSectionPoint> cs =
        KernelResultReader::readCrossSection(logPath);
    QString used = logPath;
    if (cs.isEmpty()) {                       // ofd.log という名前でも探す
        const QString alt = fi.path() + QStringLiteral("/ofd.log");
        cs = KernelResultReader::readCrossSection(alt);
        used = alt;
    }
    const bool wantMono = !m_rcsMono || m_rcsMono->isChecked();
    if (cs.isEmpty() || !wantMono) {
        if (cs.isEmpty()) m_rcsResultNote->setText(I18n::tr("sct_res_none"));
        else              m_rcsResultNote->clear();
        refreshBistatic();
        return;
    }

    // ka の基準にする半径: 形状の最大半寸法 (外接直方体の半辺の最大)。
    // 球 (shape 2) ではこれが半径そのものになる。半対角 (外接球半径) を使うと
    // 球でも √3 倍に膨らんで ka がずれるので使わない。球以外では代表寸法の
    // 目安でしかないため、注記でそう言う。
    double radius = 0.0;
    bool isSphere = false;
    for (const Geometry &g : m_p->geometries()) {
        const double hx = 0.5 * std::fabs(g.g[1] - g.g[0]);
        const double hy = 0.5 * std::fabs(g.g[3] - g.g[2]);
        const double hz = 0.5 * std::fabs(g.g[5] - g.g[4]);
        const double h = std::max(hx, std::max(hy, hz));
        if (h > radius) { radius = h; isSphere = (g.shape == 2); }
    }
    // Mie は「その他散乱量」の Mie にチェックがあり、かつ形状が球のときだけ。
    // 球でない形へ球の厳密解を並べると比較になっていない値を出すことになる。
    const bool wantMie = (m_misc.size() > 4 && m_misc[4]
                          && m_misc[4]->isChecked());
    const bool showMie = wantMie && isSphere && radius > 0.0;
    m_rcsTable->setColumnHidden(2, !showMie);
    m_rcsTable->setColumnHidden(4, !showMie);

    const int unit = m_rcsUnit ? m_rcsUnit->currentIndex() : 0;
    auto fmt = [&](double sigma, double f) {
        switch (unit) {
        case 1: {                              // dBsm
            const double db = em::rcsDbsm(sigma);
            return std::isfinite(db)
                       ? QStringLiteral("%1 dBsm").arg(db, 0, 'f', 2)
                       : QStringLiteral("−∞ dBsm");
        }
        case 2:                                // σ/λ²
            return QStringLiteral("%1 λ²")
                .arg(em::rcsPerWavelengthSq(sigma, f), 0, 'g', 4);
        default:                               // m²
            return QStringLiteral("%1 m²").arg(sigma, 0, 'g', 4);
        }
    };
    for (const CrossSectionPoint &p : cs) {
        const int r = m_rcsTable->rowCount();
        m_rcsTable->insertRow(r);
        auto ro = [](const QString &t) {
            auto *it = new QTableWidgetItem(t);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        m_rcsTable->setItem(r, 0, ro(QStringLiteral("%1 MHz")
                                         .arg(p.freqHz * 1e-6, 0, 'g', 6)));
        m_rcsTable->setItem(r, 1, ro(fmt(p.backward_m2, p.freqHz)));
        m_rcsTable->setItem(r, 3, ro(fmt(p.forward_m2, p.freqHz)));
        if (showMie) {
            const em::MieSphereRcs m = em::pecSphereRcs(radius, p.freqHz);
            m_rcsTable->setItem(r, 2, ro(m.valid
                ? fmt(m.backward_m2, p.freqHz) : QStringLiteral("—")));
            m_rcsTable->setItem(r, 4, ro(m.valid
                ? fmt(m.forward_m2, p.freqHz) : QStringLiteral("—")));
        } else {
            m_rcsTable->setItem(r, 2, ro(QStringLiteral("—")));
            m_rcsTable->setItem(r, 4, ro(QStringLiteral("—")));
        }
        const double ka = em::sphereKa(radius, p.freqHz);
        m_rcsTable->setItem(r, 5, ro(ka > 0
            ? QStringLiteral("%1").arg(ka, 0, 'f', 3)
            : QStringLiteral("—")));
    }
    m_rcsTable->resizeColumnsToContents();
    m_rcsTable->setVisible(true);
    QString note = I18n::tr("sct_res_ok")
                       .arg(QDir::toNativeSeparators(used))
                       .arg(cs.size())
                       .arg(QString::number(radius, 'g', 4));
    if (showMie)            note += QLatin1Char(' ') + I18n::tr("sct_mie_note");
    else if (wantMie)       note += QLatin1Char(' ')
                                  + I18n::tr("sct_mie_notsphere");
    m_rcsResultNote->setText(note);
    refreshBistatic();
}

// ── バイスタティック RCS と偏波散乱行列 (far1d.log) ────────────────────────
// far1d.log の [dB] 列が RCS になるのは **平面波入射で給電点が無い問題だけ**
// (em::far1dIsRcs)。給電点があると同じ列が相対利得になるので、そのときは
// 値を出さずに理由を出す (絶対規則 5)。
void ScatteringTab::refreshBistatic()
{
    if (!m_biTable || !m_biNote) return;
    const bool wantBi = m_rcsBi && m_rcsBi->isChecked();
    const bool wantMx = m_rcsMatrix && m_rcsMatrix->isChecked();
    m_biTable->setRowCount(0);
    m_biTable->setVisible(false);
    m_mxNote->setVisible(wantMx);
    m_mxNote->clear();
    m_biNote->setVisible(wantBi || wantMx);
    if (!wantBi && !wantMx) return;

    if (m_p->filePath().isEmpty()) {
        m_biNote->setText(I18n::tr("sct_bi_none"));
        return;
    }
    if (!em::far1dIsRcs(m_p->planewave().enabled, m_p->feeds().size())) {
        m_biNote->setText(I18n::tr("sct_bi_notrcs"));
        return;
    }
    const QFileInfo fi(m_p->filePath());
    QVector<FarPattern> pats =
        KernelResultReader::readFar1d(fi.path() + QStringLiteral("/far1d.log"));
    if (pats.isEmpty()) {
        m_biNote->setText(I18n::tr("sct_bi_none"));
        return;
    }

    const int unit = m_rcsUnit ? m_rcsUnit->currentIndex() : 0;
    auto fmt = [&](double sigma, double f) {
        switch (unit) {
        case 1: {
            const double db = em::rcsDbsm(sigma);
            return std::isfinite(db)
                       ? QStringLiteral("%1 dBsm").arg(db, 0, 'f', 2)
                       : QStringLiteral("−∞ dBsm");
        }
        case 2:
            return QStringLiteral("%1 λ²")
                .arg(em::rcsPerWavelengthSq(sigma, f), 0, 'g', 4);
        default:
            return QStringLiteral("%1 m²").arg(sigma, 0, 'g', 4);
        }
    };
    auto ro = [](const QString &t) {
        auto *it = new QTableWidgetItem(t);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return it;
    };

    if (wantBi) {
        QSet<QString> planes;
        for (const FarPattern &fp : pats) {
            if (fp.deg.isEmpty()) continue;
            planes.insert(fp.plane);
            int imax = 0, imin = 0;
            for (int i = 1; i < fp.eAbsDb.size(); ++i) {
                if (fp.eAbsDb[i] > fp.eAbsDb[imax]) imax = i;
                if (fp.eAbsDb[i] < fp.eAbsDb[imin]) imin = i;
            }
            // 後方 = 入射方向 (θ_inc) にいちばん近い角度の点
            const double back = m_p->planewave().theta;
            int ib = 0;
            for (int i = 1; i < fp.deg.size(); ++i)
                if (std::fabs(fp.deg[i] - back) < std::fabs(fp.deg[ib] - back))
                    ib = i;
            const int r = m_biTable->rowCount();
            m_biTable->insertRow(r);
            m_biTable->setItem(r, 0, ro(QStringLiteral("%1 / %2 MHz")
                                            .arg(fp.plane)
                                            .arg(fp.freqHz * 1e-6, 0, 'g', 6)));
            m_biTable->setItem(r, 1, ro(fmt(em::rcsFromFar1dDbsm(fp.eAbsDb[ib]),
                                            fp.freqHz)));
            m_biTable->setItem(r, 2, ro(fmt(em::rcsFromFar1dDbsm(fp.eAbsDb[imax]),
                                            fp.freqHz)));
            m_biTable->setItem(r, 3, ro(QStringLiteral("%1°")
                                            .arg(fp.deg[imax], 0, 'f', 1)));
            m_biTable->setItem(r, 4, ro(fmt(em::rcsFromFar1dDbsm(fp.eAbsDb[imin]),
                                            fp.freqHz)));
        }
        m_biTable->resizeColumnsToContents();
        m_biTable->setVisible(m_biTable->rowCount() > 0);
        m_biNote->setText(I18n::tr("sct_bi_ok")
                              .arg(planes.size()).arg(pats.size()));
    }

    if (wantMx) {
        // 偏波散乱行列の「入射させた偏波の列」= 後方方向の θ / φ 散乱成分。
        const FarPattern &fp = pats.first();
        if (fp.eThetaDb.isEmpty() || fp.ePhiDb.isEmpty()) {
            m_mxNote->setText(I18n::tr("sct_mx_nocols"));
        } else {
            const double back = m_p->planewave().theta;
            int ib = 0;
            for (int i = 1; i < fp.deg.size(); ++i)
                if (std::fabs(fp.deg[i] - back) < std::fabs(fp.deg[ib] - back))
                    ib = i;
            const QString pol = (m_p->planewave().pol == 2)
                                    ? QStringLiteral("H (φ)")
                                    : QStringLiteral("V (θ)");
            m_mxNote->setText(I18n::tr("sct_mx_fmt").arg(
                pol,
                fmt(em::rcsFromFar1dDbsm(fp.eThetaDb.value(ib, -300.0)),
                    fp.freqHz),
                fmt(em::rcsFromFar1dDbsm(fp.ePhiDb.value(ib, -300.0)),
                    fp.freqHz)));
        }
    }
}

void ScatteringTab::refresh()
{
    m_updating = true;
    const PlaneWave &pw = m_p->planewave();
    m_theta->setText(QString::number(pw.theta, 'g', 8));
    m_phi->setText(QString::number(pw.phi, 'g', 8));
    m_pol->setCurrentIndex(pw.pol == 2 ? 1 : 0);

    const ScatteringOpts &s = m_p->scattering();
    m_sweep->setChecked(s.sweepEnabled);
    m_sweepAxis->setCurrentIndex(qBound(0, s.sweepAxis, 1));
    m_sweepFrom->setText(QString::number(s.sweepFrom_deg, 'g', 8));
    m_sweepTo->setText(QString::number(s.sweepTo_deg, 'g', 8));
    m_sweepPts->setText(QString::number(s.sweepPoints));
    m_updating = false;
    updateSweepUi();
}

SectionBox *ScatteringTab::checkSection(QWidget *parent, const char *titleKey,
                                        const char *const *keys, const bool *checked,
                                        int n, QVector<QCheckBox *> *out)
{
    auto *s = new SectionBox(I18n::tr(titleKey), parent);
    for (int i = 0; i < n; ++i) {
        auto *ck = new QCheckBox(I18n::tr(keys[i]), s);
        ck->setChecked(checked[i]);
        s->vbox()->addWidget(ck);
        out->push_back(ck);
    }
    return s;
}
