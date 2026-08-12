// AntennaCharTab.cpp
#include "AntennaCharTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../em/Directivity.h"
#include "../em/PatternMetrics.h"
#include "../io/KernelResultReader.h"
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <cmath>
#include "../I18n.h"
#include "TabHelpers.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 ant_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("ant_title", "アンテナ特性 (OpenFDTD §2.13)",
              "Antenna characteristics (OpenFDTD §2.13)");
    I18n::reg("ant_hint",
              "アンテナ解析専用の評価指標 (計算は未実装 — 出力項目の選択のみ)。",
              "Antenna-specific figures of merit (computation not implemented — "
              "this page only selects output items).");

    // 入力特性 / Input characteristics
    I18n::reg("ant_input", "入力特性", "Input characteristics");
    I18n::reg("ant_in_z", "入力インピーダンス Z(f)", "Input impedance Z(f)");
    I18n::reg("ant_in_vswr", "VSWR(f)", "VSWR(f)");
    I18n::reg("ant_in_gamma", "反射係数 Γ(f), S11", "Reflection coefficient Γ(f), S11");
    I18n::reg("ant_in_res", "共振周波数の自動抽出", "Auto-extract resonant frequencies");
    I18n::reg("ant_in_bw", "-10dB 帯域幅", "-10 dB bandwidth");
    I18n::reg("ant_in_smith", "スミスチャート出力", "Smith chart output");

    // 放射特性 / Radiation characteristics
    I18n::reg("ant_rad", "放射特性", "Radiation characteristics");
    I18n::reg("ant_rad_pattern", "放射パターン (θ, φ) 全方向",
              "Radiation pattern (θ, φ), all directions");
    I18n::reg("ant_rad_beam", "主ビーム方向・3dB幅", "Main-beam direction / 3 dB width");
    I18n::reg("ant_rad_sll", "サイドローブレベル (SLL)", "Side-lobe level (SLL)");
    I18n::reg("ant_rad_fb", "フロントバック比 (F/B)", "Front-to-back ratio (F/B)");
    I18n::reg("ant_rad_gain", "ゲイン (dBi, 絶対)", "Gain (dBi, absolute)");
    I18n::reg("ant_rad_dir", "指向性 (Directivity)", "Directivity");
    I18n::reg("ant_rad_eff", "放射効率 η_rad", "Radiation efficiency η_rad");
    I18n::reg("ant_rad_tot", "アンテナ効率 η_tot (整合損含む)",
              "Antenna efficiency η_tot (incl. mismatch loss)");

    // 偏波特性 / Polarization
    I18n::reg("ant_pol", "偏波特性", "Polarization");
    I18n::reg("ant_pol_comp", "θ成分 / φ成分", "θ component / φ component");
    I18n::reg("ant_pol_ar", "軸比 AR (円偏波)", "Axial ratio AR (circular polarization)");
    I18n::reg("ant_pol_cp", "LHCP / RHCP 分離出力", "LHCP / RHCP separated output");
    I18n::reg("ant_pol_xpd", "交差偏波識別度 XPD", "Cross-polar discrimination XPD");

    // アレイ特性 / Array
    I18n::reg("ant_array", "アレイ特性 (多素子)", "Array (multi-element)");
    I18n::reg("ant_arr_zact", "アクティブインピーダンス Z_act(n)",
              "Active impedance Z_act(n)");
    I18n::reg("ant_arr_coupling", "結合度行列 [S]", "Coupling matrix [S]");
    I18n::reg("ant_arr_steer", "ビームステアリング解析", "Beam-steering analysis");
    I18n::reg("ant_arr_grating", "グレーティングローブ検出", "Grating-lobe detection");

    I18n::reg("ant_output", "出力先", "Outputs");
    I18n::reg("ant_csv_hint",
              "CSV は直近の計算結果 (<ケース名>.log の給電点表と far1d.log の"
              "遠方界パターン) から書き出します。HDF5 / NEC・FFE の書き出しは"
              "未実装です。",
              "The CSV is written from the latest run (<case>.log feed table and "
              "the far1d.log patterns). HDF5 and NEC/FFE export are not "
              "implemented.");
    I18n::reg("ant_csv_none",
              "書き出せる結果がありません。先に「計算」と「ポスト処理」を"
              "実行してください (%1 に .log が見つかりません)。",
              "There is no result to export. Run the solver and the "
              "post-processing first (no .log found in %1).");
    I18n::reg("ant_csv_ok",
              "書き出しました: %1 — 給電点 %2 個 (%3 周波数)、遠方界 %4 面",
              "Exported %1 \u2014 %2 feed(s) over %3 frequencies, %4 pattern cut(s)");
    I18n::reg("ant_csv_fail", "書き出しに失敗しました: %1",
              "Export failed: %1");
    I18n::reg("ant_uw_items",
              "放射効率・アンテナ効率・偏波成分/軸比/XPD・アレイ特性のチェック",
              "the radiation/total efficiency, polarisation (components, axial "
              "ratio, XPD) and array check boxes");
    I18n::reg("ant_uw_items_ok",
              "放射パターン・主ビーム方向と 3 dB 幅・サイドローブレベル・"
              "前後比・ゲイン (下の「パターン指標」表と CSV に出ます) と、"
              "指向性 (far2d.log があれば全球積分から出します)",
              "the radiation pattern, main-beam direction and 3 dB width, "
              "side-lobe level, front-to-back ratio and gain (they appear in "
              "the pattern-metrics table below and in the CSV), and the "
              "directivity when a far2d.log is available");
    I18n::reg("ant_uw_items_why",
              "放射効率とアンテナ効率は入力電力と放射電力の比なので、"
              "パターンの形だけでは決まりません (指向性はパターンの定数倍に"
              "不変なので出せます)。偏波成分・軸比・XPD は far1d.log が "
              "E-abs しか持たないため出せません。",
              "The radiation and total efficiencies are ratios of radiated to "
              "input power, so the pattern shape alone does not determine them "
              "(the directivity is obtainable because it is invariant under "
              "scaling of the pattern). The polarisation components, axial "
              "ratio and XPD are not available because far1d.log carries only "
              "E-abs.");
    // 指向性 (far2d.log の全球積分)
    I18n::reg("ant_dir_title", "指向性 (far2d.log の全球積分)",
              "Directivity (full-sphere integral of far2d.log)");
    I18n::reg("ant_dir_none",
              "far2d.log がまだありません。ポスト処理で「遠方界 2D」を出すと"
              "全球積分から指向性を出します。",
              "There is no far2d.log yet. Enable the 2D far field in the "
              "post-processing to get the directivity from the full-sphere "
              "integral.");
    I18n::reg("ant_dir_fmt",
              "%1 MHz: D = %2 dBi (真値 %3) / ビーム立体角 %4 sr / "
              "最大は θ = %5°, φ = %6°",
              "%1 MHz: D = %2 dBi (%3 as a ratio) / beam solid angle %4 sr / "
              "peak at theta = %5 deg, phi = %6 deg");
    I18n::reg("ant_dir_note",
              "指向性はパターンの定数倍に不変なので、遠方界の正規化に依らず"
              "求まります。利得にするには放射効率が要りますが、それは入力電力と"
              "放射電力の比なのでパターンからは出ません。",
              "The directivity is invariant under a constant scaling of the "
              "pattern, so it does not depend on how the far field is "
              "normalised. Turning it into gain needs the radiation "
              "efficiency, which is a ratio of powers and cannot come from the "
              "pattern.");
    I18n::reg("ant_met_title", "パターン指標 (far1d.log の切断面ごと)",
              "Pattern metrics (per cut in far1d.log)");
    I18n::reg("ant_met_none",
              "far1d.log がまだありません。ポスト処理で「遠方界 1D "
              "(plotfar1d)」を有効にして実行すると、.ofd と同じディレクトリに"
              "出力されます。",
              "There is no far1d.log yet. Enable \"far field 1D (plotfar1d)\" "
              "in post processing and run - it is written next to the .ofd.");
    I18n::reg("ant_met_ok", "%1 の %2 面から算出しました。",
              "Computed from %2 cuts in %1.");
    I18n::reg("ant_met_plane", "面", "Cut");
    I18n::reg("ant_met_freq", "周波数", "Frequency");
    I18n::reg("ant_met_peak", "ピーク", "Peak");
    I18n::reg("ant_met_dir", "主ビーム方向", "Main-beam direction");
    I18n::reg("ant_met_hpbw", "3 dB 幅", "3 dB width");
    I18n::reg("ant_met_sll", "SLL", "SLL");
    I18n::reg("ant_met_fb", "F/B", "F/B");
    I18n::reg("ant_met_na", "—", "—");
    return true;
}();

// モックの <Check checked> をそのまま転記 (キー + 既定チェック状態)
const char *const kInputKeys[] = { "ant_in_z", "ant_in_vswr", "ant_in_gamma",
                                   "ant_in_res", "ant_in_bw", "ant_in_smith" };
const bool kInputOn[] = { true, true, true, true, true, false };

const char *const kRadKeys[] = { "ant_rad_pattern", "ant_rad_beam", "ant_rad_sll",
                                 "ant_rad_fb", "ant_rad_gain", "ant_rad_dir",
                                 "ant_rad_eff", "ant_rad_tot" };
const bool kRadOn[] = { true, true, true, false, true, true, true, false };

const char *const kPolKeys[] = { "ant_pol_comp", "ant_pol_ar", "ant_pol_cp",
                                 "ant_pol_xpd" };
const bool kPolOn[] = { true, false, false, false };

const char *const kArrKeys[] = { "ant_arr_zact", "ant_arr_coupling",
                                 "ant_arr_steer", "ant_arr_grating" };
const bool kArrOn[] = { false, false, false, false };
} // namespace

AntennaCharTab::AntennaCharTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── アンテナ特性 (説明) ────────────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("ant_title"), body);
    auto *hint = new QLabel(I18n::tr("ant_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);
    v->addWidget(sTop);

    v->addWidget(checkSection(body, "ant_input", kInputKeys, kInputOn,
                              int(sizeof(kInputKeys) / sizeof(kInputKeys[0])), &m_input));
    v->addWidget(checkSection(body, "ant_rad", kRadKeys, kRadOn,
                              int(sizeof(kRadKeys) / sizeof(kRadKeys[0])), &m_radiation));
    v->addWidget(checkSection(body, "ant_pol", kPolKeys, kPolOn,
                              int(sizeof(kPolKeys) / sizeof(kPolKeys[0])), &m_polar));
    v->addWidget(checkSection(body, "ant_array", kArrKeys, kArrOn,
                              int(sizeof(kArrKeys) / sizeof(kArrKeys[0])), &m_array));

    // ── パターン指標 (far1d.log の切断面から実計算) ────────────────────────
    {
        auto *sMet = new SectionBox(I18n::tr("ant_met_title"), body);
        m_metricsNote = new QLabel(sMet);
        m_metricsNote->setWordWrap(true);
        m_metricsNote->setStyleSheet("font-size:11px; color:palette(mid);");
        sMet->vbox()->addWidget(m_metricsNote);
        m_metrics = new QTableWidget(0, 7, sMet);
        m_metrics->setHorizontalHeaderLabels(
            { I18n::tr("ant_met_plane"), I18n::tr("ant_met_freq"),
              I18n::tr("ant_met_peak"), I18n::tr("ant_met_dir"),
              I18n::tr("ant_met_hpbw"), I18n::tr("ant_met_sll"),
              I18n::tr("ant_met_fb") });
        m_metrics->horizontalHeader()->setStretchLastSection(true);
        m_metrics->verticalHeader()->setVisible(false);
        m_metrics->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_metrics->setMinimumHeight(110);
        m_metrics->setVisible(false);
        sMet->vbox()->addWidget(m_metrics);
        // 指向性は切断面では出せない (全球積分)。far2d.log があれば出す。
        m_dirNote = new QLabel(sMet);
        m_dirNote->setWordWrap(true);
        m_dirNote->setStyleSheet("font-size:11px; color:palette(mid);");
        sMet->vbox()->addWidget(m_dirNote);
        v->addWidget(sMet);
        connect(project, &Project::loaded, this,
                &AntennaCharTab::refreshMetrics);
        refreshMetrics();
    }

    // ── 出力先 ────────────────────────────────────────────────────────────
    auto *sOut = new SectionBox(I18n::tr("ant_output"), body);
    auto *row = new QHBoxLayout();
    auto *csvBtn = new QPushButton("📄 antenna_report.csv", sOut);
    auto *h5Btn  = new QPushButton("📊 antenna_pattern.h5", sOut);
    auto *necBtn = new QPushButton("📐 .nec / .ffe", sOut);
    // CSV は実装済み (ofd の実行結果を読んで書く)。HDF5 / NEC は未実装。
    connect(csvBtn, &QPushButton::clicked, this, &AntennaCharTab::exportCsv);
    tabhelp::markNotImplemented(h5Btn);
    tabhelp::markNotImplemented(necBtn);
    row->addWidget(csvBtn);
    row->addWidget(h5Btn);
    row->addWidget(necBtn);
    row->addStretch(1);
    sOut->vbox()->addLayout(row);
    m_exportNote = new QLabel(I18n::tr("ant_csv_hint"), sOut);
    m_exportNote->setWordWrap(true);
    m_exportNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sOut->vbox()->addWidget(m_exportNote);
    v->addWidget(sOut);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
}

SectionBox *AntennaCharTab::checkSection(QWidget *parent, const char *titleKey,
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
    // 切断面から出せる 5 項目は下の「パターン指標」表に出る。残りは出せない
    // 理由を添える (全球積分が要る / far1d.log に成分が無い)
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("ant_uw_items"),
                                              I18n::tr("ant_uw_items_ok")));
    auto *why = new QLabel(I18n::tr("ant_uw_items_why"), s);
    why->setWordWrap(true);
    why->setStyleSheet("font-size:11px; color:palette(mid);");
    s->vbox()->addWidget(why);
    return s;
}

// アンテナ特性レポート (CSV) — 直近の計算結果から書き出す。
// 給電点表 (<ケース名>.log) と遠方界パターン (far1d.log) を 1 ファイルに
// まとめる。結果が無ければ **書かずに理由を出す** (絶対規則 5)。
void AntennaCharTab::exportCsv()
{
    const QString dir = m_p->filePath().isEmpty()
                            ? QString()
                            : QFileInfo(m_p->filePath()).path();
    if (dir.isEmpty()) {
        m_exportNote->setText(I18n::tr("ant_csv_none")
                                  .arg(QStringLiteral("(未保存のプロジェクト)")));
        return;
    }
    const QString base = QFileInfo(m_p->filePath()).completeBaseName();
    const QVector<FeedSweep> feeds =
        KernelResultReader::readFeedSweeps(dir + QLatin1Char('/') + base + ".log");
    const QVector<FarPattern> cuts =
        KernelResultReader::readFar1d(dir + QStringLiteral("/far1d.log"));
    if (feeds.isEmpty() && cuts.isEmpty()) {
        m_exportNote->setText(
            I18n::tr("ant_csv_none").arg(QDir::toNativeSeparators(dir)));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("ant_output"),
        dir + QStringLiteral("/antenna_report.csv"),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_exportNote->setText(I18n::tr("ant_csv_fail").arg(f.errorString()));
        return;
    }
    QTextStream out(&f);
    out << "# OpenFDTD-X antenna report\n";
    out << "# project," << QFileInfo(m_p->filePath()).fileName() << "\n";
    int freqCount = 0;
    for (const FeedSweep &fs : feeds) {
        out << "\n[feed " << fs.feedIndex << "] z0[ohm]," << fs.z0 << "\n";
        out << "frequency[Hz],Rin[ohm],Xin[ohm],|Z|[ohm],Ref[dB],VSWR\n";
        for (const FeedSweepPoint &p : fs.points) {
            out << QString::number(p.freqHz, 'g', 10) << ","
                << QString::number(p.rin, 'g', 8) << ","
                << QString::number(p.xin, 'g', 8) << ","
                << QString::number(std::hypot(p.rin, p.xin), 'g', 8) << ","
                << QString::number(p.refDb, 'g', 6) << ","
                << QString::number(p.vswr, 'g', 6) << "\n";
            ++freqCount;
        }
    }
    for (const FarPattern &c : cuts) {
        out << "\n[pattern] plane," << c.plane << ",frequency[Hz],"
            << QString::number(c.freqHz, 'g', 10) << "\n";
        // 切断面から確定する指標 (パターン指標表と同じ値)
        {
            const std::vector<double> dv(c.deg.begin(), c.deg.end());
            const std::vector<double> bv(c.eAbsDb.begin(), c.eAbsDb.end());
            const em::PatternMetrics m = em::patternMetrics(dv, bv);
            out << "metric,value,unit\n";
            if (m.hasPeak) {
                out << "peak," << QString::number(m.peakDb, 'g', 6) << ",dB\n";
                out << "peak_direction," << QString::number(m.peakDeg, 'g', 6)
                    << ",deg\n";
            }
            if (m.hasHpbw)
                out << "hpbw," << QString::number(m.hpbwDeg, 'g', 6) << ",deg\n";
            if (m.hasSll)
                out << "sll," << QString::number(m.sllDb, 'g', 6) << ",dB\n";
            if (m.hasFb)
                out << "front_to_back," << QString::number(m.fbDb, 'g', 6)
                    << ",dB\n";
        }
        out << "angle[deg],E-abs[dB]\n";
        for (int i = 0; i < c.deg.size() && i < c.eAbsDb.size(); ++i)
            out << QString::number(c.deg[i], 'g', 6) << ","
                << QString::number(c.eAbsDb[i], 'g', 6) << "\n";
    }
    f.close();
    m_exportNote->setText(I18n::tr("ant_csv_ok")
                              .arg(QFileInfo(path).fileName())
                              .arg(feeds.size()).arg(freqCount).arg(cuts.size()));
}

// far1d.log の切断面ごとに、そこから確定する指標を出す。
// 指向性・効率は全球積分が要るのでここには出さない (出せない理由は注記に出す)。
void AntennaCharTab::refreshMetrics()
{
    if (!m_metrics) return;
    m_metrics->setRowCount(0);
    m_metrics->setVisible(false);
    const QString dir = m_p->filePath().isEmpty()
                            ? QString()
                            : QFileInfo(m_p->filePath()).path();
    if (dir.isEmpty()) {
        m_metricsNote->setText(I18n::tr("ant_met_none"));
        return;
    }
    const QString path = dir + QStringLiteral("/far1d.log");
    const QVector<FarPattern> cuts = KernelResultReader::readFar1d(path);
    if (cuts.isEmpty()) {
        m_metricsNote->setText(I18n::tr("ant_met_none"));
        return;
    }
    auto cell = [](const QString &t) {
        auto *it = new QTableWidgetItem(t);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return it;
    };
    const QString na = I18n::tr("ant_met_na");
    for (const FarPattern &c : cuts) {
        const std::vector<double> deg(c.deg.begin(), c.deg.end());
        const std::vector<double> db(c.eAbsDb.begin(), c.eAbsDb.end());
        const em::PatternMetrics m = em::patternMetrics(deg, db);
        const int r = m_metrics->rowCount();
        m_metrics->insertRow(r);
        m_metrics->setItem(r, 0, cell(c.plane));
        m_metrics->setItem(r, 1, cell(QStringLiteral("%1 MHz")
                                          .arg(c.freqHz * 1e-6, 0, 'g', 6)));
        m_metrics->setItem(r, 2, cell(m.hasPeak
            ? QStringLiteral("%1 dB").arg(m.peakDb, 0, 'f', 2) : na));
        m_metrics->setItem(r, 3, cell(m.hasPeak
            ? QStringLiteral("%1°").arg(m.peakDeg, 0, 'f', 1) : na));
        m_metrics->setItem(r, 4, cell(m.hasHpbw
            ? QStringLiteral("%1°").arg(m.hpbwDeg, 0, 'f', 1) : na));
        m_metrics->setItem(r, 5, cell(m.hasSll
            ? QStringLiteral("%1 dB").arg(m.sllDb, 0, 'f', 2) : na));
        m_metrics->setItem(r, 6, cell(m.hasFb
            ? QStringLiteral("%1 dB").arg(m.fbDb, 0, 'f', 2) : na));
    }
    m_metrics->resizeColumnsToContents();
    m_metrics->setVisible(true);
    m_metricsNote->setText(I18n::tr("ant_met_ok")
                               .arg(QDir::toNativeSeparators(path))
                               .arg(cuts.size()));
    refreshDirectivity(dir);
}

// ── 指向性: far2d.log の全球積分 (em/Directivity) ──────────────────────────
// far2d.log の E-abs[dB] は**振幅の 20log10** (カーネルの outputFar2d.c)。
// 放射強度は U = |E|² なので 10^(dB/10) で線形へ戻す。
// D はパターンの定数倍に不変なので、遠方界の正規化に依らず求まる。
void AntennaCharTab::refreshDirectivity(const QString &dir)
{
    if (!m_dirNote) return;
    const QString path = dir + QStringLiteral("/far2d.log");
    const QVector<FieldMap> maps = KernelResultReader::readFar2d(path);
    if (maps.isEmpty()) {
        m_dirNote->setText(I18n::tr("ant_dir_none"));
        return;
    }
    QStringList lines;
    for (const FieldMap &m : maps) {
        if (!m.isValid()) continue;
        em::SphericalPattern sp;
        for (int i = 0; i < m.rows; ++i)
            sp.theta_deg.push_back(m.rows > 1
                ? m.rowMin + (m.rowMax - m.rowMin) * i / (m.rows - 1) : m.rowMin);
        for (int j = 0; j < m.cols; ++j)
            sp.phi_deg.push_back(m.cols > 1
                ? m.colMin + (m.colMax - m.colMin) * j / (m.cols - 1) : m.colMin);
        sp.u.reserve(static_cast<size_t>(m.rows) * m.cols);
        for (double v : m.values) sp.u.push_back(em::intensityFromEabsDb(v));
        const em::Directivity d = em::directivity(sp);
        if (!d.valid) continue;
        lines << I18n::tr("ant_dir_fmt")
                     .arg(m.freqHz * 1e-6, 0, 'g', 6)
                     .arg(d.directivityDbi, 0, 'f', 2)
                     .arg(d.directivity, 0, 'f', 3)
                     .arg(d.beamSolidAngle, 0, 'g', 3)
                     .arg(d.peakTheta_deg, 0, 'f', 1)
                     .arg(d.peakPhi_deg, 0, 'f', 1);
    }
    if (lines.isEmpty()) {
        m_dirNote->setText(I18n::tr("ant_dir_none"));
        return;
    }
    m_dirNote->setText(I18n::tr("ant_dir_title") + QStringLiteral(" — ")
                       + lines.join(QStringLiteral(" / ")) + QStringLiteral(" ")
                       + I18n::tr("ant_dir_note"));
}
