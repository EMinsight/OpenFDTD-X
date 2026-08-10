// AntennaCharTab.cpp
#include "AntennaCharTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../io/KernelResultReader.h"
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <cmath>
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
    I18n::reg("ant_uw_items", "出力項目のチェック",
              "the output-item check boxes");
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
    // チェック状態はまだどこにも読まれない (ローカル状態のみ)
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("ant_uw_items")));
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
