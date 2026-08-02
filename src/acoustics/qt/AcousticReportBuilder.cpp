// AcousticReportBuilder.cpp
#include "AcousticReportBuilder.h"
#include "AcousticResultModel.h"
#include "../../I18n.h"

#include <QStringList>

using namespace ofd;
using namespace ofd::acoustics;

namespace {

const bool s_i18n = [] {
    ofd::I18n::reg("rep_title",      "オペラ音響 分析レポート",
                                     "Opera acoustics analysis report");
    ofd::I18n::reg("rep_meta",       "対象",              "Subject");
    ofd::I18n::reg("rep_project",    "プロジェクト",      "Project");
    ofd::I18n::reg("rep_rir_file",   "実測 RIR",          "Measured RIR");
    ofd::I18n::reg("rep_voice_file", "歌唱音源",          "Singing source");
    ofd::I18n::reg("rep_calib",      "校正状態",          "Calibration");
    ofd::I18n::reg("rep_calib_abs",  "Absolute (絶対 SPL 換算あり)",
                                     "Absolute (SPL conversion available)");
    ofd::I18n::reg("rep_calib_rel",  "Relative (相対レベルのみ)",
                                     "Relative (relative levels only)");
    ofd::I18n::reg("rep_calib_unc",  "Uncalibrated (未校正)",
                                     "Uncalibrated");
    ofd::I18n::reg("rep_calib_off",  "校正オフセット",    "Calibration offset");
    ofd::I18n::reg("rep_sec_rir",    "1. 実測 RIR 分析 (ISO 3382-1)",
                                     "1. Measured RIR analysis (ISO 3382-1)");
    ofd::I18n::reg("rep_sec_vocal",  "2. 歌声分析",       "2. Singing voice analysis");
    ofd::I18n::reg("rep_sec_aural",  "3. 可聴化 (設定)",  "3. Auralization (settings)");
    ofd::I18n::reg("rep_not_run",    "未実行 — このレポートには結果が含まれません",
                                     "Not run — no results are included in this report");
    ofd::I18n::reg("rep_none_msg",
        "実行済みの分析がありません。実測RIR分析タブまたは歌声分析タブで"
        "分析を実行してからレポートを出力してください。",
        "No completed analysis found. Run the RIR analysis or vocal analysis "
        "tab first, then export the report.");
    ofd::I18n::reg("rep_not_set",    "未設定",            "not set");
    ofd::I18n::reg("rep_overall",    "総合品質",          "Overall quality");
    ofd::I18n::reg("rep_metrics",    "指標",              "Metrics");
    ofd::I18n::reg("rep_summary",    "サマリー",          "Summary");
    ofd::I18n::reg("rep_reflections","初期反射",          "Early reflections");
    ofd::I18n::reg("rep_warnings",   "警告",              "Warnings");
    ofd::I18n::reg("rep_no_warn",    "警告はありません",  "No warnings");
    ofd::I18n::reg("rep_col_metric", "指標",              "Metric");
    ofd::I18n::reg("rep_col_band",   "帯域",              "Band");
    ofd::I18n::reg("rep_col_value",  "値",                "Value");
    ofd::I18n::reg("rep_col_unit",   "単位",              "Unit");
    ofd::I18n::reg("rep_col_qual",   "品質",              "Quality");
    ofd::I18n::reg("rep_col_note",   "備考",              "Note");
    ofd::I18n::reg("rep_col_idx",    "#",                 "#");
    ofd::I18n::reg("rep_col_arr",    "到来 [ms]",         "Arrival [ms]");
    ofd::I18n::reg("rep_col_delay",  "直接音からの遅れ [ms]",
                                     "Delay from direct [ms]");
    ofd::I18n::reg("rep_col_level",  "相対レベル [dB]",   "Relative level [dB]");
    ofd::I18n::reg("rep_col_bin",    "時間区分",          "Time bin");
    ofd::I18n::reg("rep_col_conf",   "確度",              "Confidence");
    ofd::I18n::reg("rep_col_item",   "項目",              "Item");
    ofd::I18n::reg("rep_na",         "算出不可",          "not available");
    ofd::I18n::reg("rep_dry",        "ドライ音源",        "Dry source");
    ofd::I18n::reg("rep_wet",        "出力 (ウェット)",   "Output (wet)");
    ofd::I18n::reg("rep_aural_note",
        "可聴化は畳み込み結果の WAV 書き出しのみで、本レポートには"
        "波形・指標を含みません。",
        "Auralization only writes the convolved WAV; no waveform or metric "
        "is included in this report.");
    // 表示規則の注記 (ADR-0006 / 音響指標の表示規則)。事実の記載のみで、
    // 診断的・評価的な結論は書かない。
    ofd::I18n::reg("rep_footer",
        "本レポートは物理量の測定結果のみを記載します。絶対 SPL は "
        "Absolute 校正時のみ有効で、それ以外は「算出不可」と表示されます。"
        "動的レンジが不足する残響時間も「算出不可」として扱われます。",
        "This report states measured physical quantities only. Absolute SPL is "
        "valid only under Absolute calibration; otherwise it is shown as "
        "\"not available\". Reverberation times with insufficient dynamic range "
        "are likewise treated as not available.");
    ofd::I18n::reg("rep_direct",     "直接音時刻",        "Direct sound time");
    ofd::I18n::reg("rep_dr",         "動的レンジ",        "Dynamic range");
    ofd::I18n::reg("rep_noise",      "ノイズフロア",      "Noise floor");
    ofd::I18n::reg("rep_spl",        "ピーク絶対 SPL",    "Peak absolute SPL");
    ofd::I18n::reg("rep_frames",     "総フレーム数",      "Total frames");
    ofd::I18n::reg("rep_voiced",     "有声フレーム数",    "Voiced frames");
    ofd::I18n::reg("rep_vratio",     "有声率",            "Voiced ratio");
    ofd::I18n::reg("rep_f0range",    "F0 探索範囲",       "F0 search range");
    return true;
}();

QString esc(const QString &s) { return s.toHtmlEscaped(); }

// 品質トークン → 表示ラベル (バッジの文言)
QString qualityText(const QString &token)
{
    if (token == QLatin1String("valid"))   return QStringLiteral("valid");
    if (token == QLatin1String("warning")) return QStringLiteral("warning");
    return QStringLiteral("invalid");
}

// 指標行テーブル (RIR / 歌声で共通)。無効値は「算出不可」+ 理由を出す。
QString rowsTable(const QVector<AcousticResultRow> &rows)
{
    QString h;
    h += QStringLiteral("<table>\n<tr><th>%1</th><th>%2</th><th>%3</th>"
                        "<th>%4</th><th>%5</th><th>%6</th></tr>\n")
             .arg(esc(I18n::tr("rep_col_metric")), esc(I18n::tr("rep_col_band")),
                  esc(I18n::tr("rep_col_value")), esc(I18n::tr("rep_col_unit")),
                  esc(I18n::tr("rep_col_qual")), esc(I18n::tr("rep_col_note")));
    for (const AcousticResultRow &r : rows) {
        const QString value = r.valid ? esc(r.valueText)
                                      : QStringLiteral("<span class=\"na\">%1</span>")
                                            .arg(esc(I18n::tr("rep_na")));
        h += QStringLiteral("<tr><td>%1</td><td>%2</td><td class=\"num\">%3</td>"
                            "<td>%4</td><td><span class=\"q %5\">%6</span></td>"
                            "<td>%7</td></tr>\n")
                 .arg(esc(r.metric), esc(r.band), value,
                      r.valid ? esc(r.unit) : QString(),
                      esc(r.quality), esc(qualityText(r.quality)),
                      esc(r.warning));
    }
    h += QStringLiteral("</table>\n");
    return h;
}

// 「項目 / 値」の 2 列表
QString kvTable(const QVector<QPair<QString, QString>> &kv)
{
    QString h;
    h += QStringLiteral("<table>\n<tr><th>%1</th><th>%2</th></tr>\n")
             .arg(esc(I18n::tr("rep_col_item")), esc(I18n::tr("rep_col_value")));
    for (const QPair<QString, QString> &p : kv)
        h += QStringLiteral("<tr><td>%1</td><td>%2</td></tr>\n")
                 .arg(esc(p.first), esc(p.second));
    h += QStringLiteral("</table>\n");
    return h;
}

QString warningList(const std::vector<std::string> &warnings)
{
    if (warnings.empty())
        return QStringLiteral("<p class=\"ok\">%1</p>\n")
            .arg(esc(I18n::tr("rep_no_warn")));
    QString h = QStringLiteral("<ul class=\"warn\">\n");
    for (const std::string &w : warnings)
        h += QStringLiteral("<li>%1</li>\n")
                 .arg(esc(QString::fromStdString(w)));
    h += QStringLiteral("</ul>\n");
    return h;
}

QString notRun()
{
    return QStringLiteral("<p class=\"notrun\">%1</p>\n")
        .arg(esc(I18n::tr("rep_not_run")));
}

QString orNotSet(const QString &s)
{
    return s.isEmpty() ? I18n::tr("rep_not_set") : s;
}

// 各行に source 列を足して結合 CSV に組み込む
QString prefixCsv(const QString &csv, const QString &source)
{
    QString out;
    const QStringList lines = csv.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.isEmpty()) continue;
        out += source + QLatin1Char(',') + line + QLatin1Char('\n');
    }
    return out;
}

QString csvEscapeField(QString s)
{
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"'))
        || s.contains(QLatin1Char('\n'))) {
        s.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        s = QStringLiteral("\"") + s + QStringLiteral("\"");
    }
    return s;
}

const char *kCss =
    "body{font-family:sans-serif;margin:24px;color:#1a1a1a;background:#fff;"
    "line-height:1.5}"
    "h1{font-size:20px;border-bottom:2px solid #333;padding-bottom:6px}"
    "h2{font-size:16px;margin-top:28px;border-left:4px solid #333;"
    "padding-left:8px}"
    "table{border-collapse:collapse;margin:8px 0;font-size:13px}"
    "th,td{border:1px solid #bbb;padding:3px 8px;text-align:left}"
    "th{background:#eee}"
    "td.num{text-align:right;font-variant-numeric:tabular-nums}"
    ".q{font-size:11px;padding:1px 6px;border-radius:8px}"
    ".q.valid{background:#d8f0d8}"
    ".q.warning{background:#f8ecc8}"
    ".q.invalid{background:#f0d8d8}"
    ".na{color:#888}"
    ".notrun{color:#888;font-style:italic}"
    "ul.warn{color:#8a5000}"
    "p.ok{color:#556}"
    "p.footer{margin-top:32px;font-size:12px;color:#555;border-top:1px solid #ccc;"
    "padding-top:8px}";

} // namespace

QString AcousticReportBuilder::calibrationLabel(int calibrationState)
{
    switch (calibrationState) {
    case 0:  return I18n::tr("rep_calib_abs");
    case 1:  return I18n::tr("rep_calib_rel");
    default: break;
    }
    return I18n::tr("rep_calib_unc");
}

bool AcousticReportBuilder::hasAnyResult(const AcousticReportInput &in)
{
    return in.hasRir || in.hasVocal;
}

QString AcousticReportBuilder::buildHtml(const AcousticReportInput &in)
{
    QString h;
    h += QStringLiteral("<!DOCTYPE html>\n<html>\n<head>\n"
                        "<meta charset=\"utf-8\">\n<title>%1</title>\n"
                        "<style>%2</style>\n</head>\n<body>\n")
             .arg(esc(I18n::tr("rep_title")), QLatin1String(kCss));
    h += QStringLiteral("<h1>%1</h1>\n").arg(esc(I18n::tr("rep_title")));

    // ── 対象 ────────────────────────────────────────────────────────────────
    QVector<QPair<QString, QString>> meta;
    meta.push_back({ I18n::tr("rep_project"), orNotSet(in.projectTitle) });
    meta.push_back({ I18n::tr("rep_rir_file"), orNotSet(in.rirFile) });
    meta.push_back({ I18n::tr("rep_voice_file"), orNotSet(in.voiceFile) });
    meta.push_back({ I18n::tr("rep_calib"), calibrationLabel(in.calibrationState) });
    // オフセットは Absolute のときだけ意味を持つ (それ以外は分析に渡らない)
    if (in.calibrationState == 0)
        meta.push_back({ I18n::tr("rep_calib_off"),
                         QString::number(in.calibrationOffsetDb, 'f', 1)
                             + QStringLiteral(" dB") });
    h += QStringLiteral("<h2>%1</h2>\n").arg(esc(I18n::tr("rep_meta")));
    h += kvTable(meta);

    // ── 1. RIR 分析 ─────────────────────────────────────────────────────────
    h += QStringLiteral("<h2>%1</h2>\n").arg(esc(I18n::tr("rep_sec_rir")));
    if (!in.hasRir) {
        h += notRun();
    } else {
        const RirAnalysisResult &r = in.rir;
        QVector<QPair<QString, QString>> s;
        s.push_back({ I18n::tr("rep_overall"),
                      AcousticResultModel::qualityToken(r.overallQuality) });
        s.push_back({ I18n::tr("rep_direct"),
                      r.directSound.found
                          ? QString::number(r.directSound.timeSeconds * 1000.0,
                                            'f', 2) + QStringLiteral(" ms")
                          : I18n::tr("rep_na") });
        s.push_back({ I18n::tr("rep_dr"),
                      QString::number(r.preprocess.dynamicRangeDb, 'f', 1)
                          + QStringLiteral(" dB") });
        s.push_back({ I18n::tr("rep_noise"),
                      QString::number(r.preprocess.noiseFloorDb, 'f', 1)
                          + QStringLiteral(" dBFS") });
        // 絶対 SPL は Absolute 校正時のみ valid (コア側でゲート済み)
        s.push_back({ I18n::tr("rep_spl"),
                      r.absoluteSplDb.valid
                          ? QString::number(r.absoluteSplDb.value, 'f', 1)
                                + QStringLiteral(" dB SPL")
                          : I18n::tr("rep_na") });
        h += QStringLiteral("<h3>%1</h3>\n").arg(esc(I18n::tr("rep_summary")));
        h += kvTable(s);

        h += QStringLiteral("<h3>%1</h3>\n").arg(esc(I18n::tr("rep_metrics")));
        h += rowsTable(AcousticResultModel::metricRows(r));

        h += QStringLiteral("<h3>%1</h3>\n").arg(esc(I18n::tr("rep_reflections")));
        h += QStringLiteral("<table>\n<tr><th>%1</th><th>%2</th><th>%3</th>"
                            "<th>%4</th><th>%5</th><th>%6</th></tr>\n")
                 .arg(esc(I18n::tr("rep_col_idx")), esc(I18n::tr("rep_col_arr")),
                      esc(I18n::tr("rep_col_delay")), esc(I18n::tr("rep_col_level")),
                      esc(I18n::tr("rep_col_bin")), esc(I18n::tr("rep_col_conf")));
        int idx = 1;
        for (const ReflectionEvent &e : r.reflections)
            h += QStringLiteral("<tr><td class=\"num\">%1</td>"
                                "<td class=\"num\">%2</td><td class=\"num\">%3</td>"
                                "<td class=\"num\">%4</td><td>%5</td>"
                                "<td class=\"num\">%6</td></tr>\n")
                     .arg(QString::number(idx++),
                          QString::number(e.arrivalTime * 1000.0, 'f', 2),
                          QString::number(e.delayFromDirect * 1000.0, 'f', 2),
                          QString::number(e.relativeLevelDb, 'f', 1),
                          esc(AcousticResultModel::reflectionBinLabel(
                              e.delayFromDirect)),
                          QString::number(e.confidence, 'f', 2));
        h += QStringLiteral("</table>\n");

        h += QStringLiteral("<h3>%1</h3>\n").arg(esc(I18n::tr("rep_warnings")));
        h += warningList(r.warnings);
    }

    // ── 2. 歌声分析 ─────────────────────────────────────────────────────────
    h += QStringLiteral("<h2>%1</h2>\n").arg(esc(I18n::tr("rep_sec_vocal")));
    if (!in.hasVocal) {
        h += notRun();
    } else {
        const VocalAnalysisResult &v = in.vocal;
        QVector<QPair<QString, QString>> s;
        s.push_back({ I18n::tr("rep_overall"),
                      AcousticResultModel::qualityToken(v.overallQuality) });
        s.push_back({ I18n::tr("rep_frames"),
                      QString::number(qulonglong(v.totalFrameCount)) });
        s.push_back({ I18n::tr("rep_voiced"),
                      QString::number(qulonglong(v.voicedFrameCount)) });
        s.push_back({ I18n::tr("rep_vratio"),
                      QString::number(v.voicedRatio, 'f', 3) });
        s.push_back({ I18n::tr("rep_f0range"),
                      QString::number(v.f0SearchMinHz, 'f', 1)
                          + QStringLiteral(" – ")
                          + QString::number(v.f0SearchMaxHz, 'f', 1)
                          + QStringLiteral(" Hz") });
        h += QStringLiteral("<h3>%1</h3>\n").arg(esc(I18n::tr("rep_summary")));
        h += kvTable(s);

        h += QStringLiteral("<h3>%1</h3>\n").arg(esc(I18n::tr("rep_metrics")));
        h += rowsTable(AcousticResultModel::vocalRows(v));

        h += QStringLiteral("<h3>%1</h3>\n").arg(esc(I18n::tr("rep_warnings")));
        h += warningList(v.warnings);
    }

    // ── 3. 可聴化 (設定のみ) ────────────────────────────────────────────────
    h += QStringLiteral("<h2>%1</h2>\n").arg(esc(I18n::tr("rep_sec_aural")));
    QVector<QPair<QString, QString>> a;
    a.push_back({ I18n::tr("rep_dry"), orNotSet(in.auralizationDryFile) });
    a.push_back({ I18n::tr("rep_wet"), orNotSet(in.auralizationOutputFile) });
    h += kvTable(a);
    h += QStringLiteral("<p class=\"footer\">%1</p>\n")
             .arg(esc(I18n::tr("rep_aural_note")));

    h += QStringLiteral("<p class=\"footer\">%1</p>\n")
             .arg(esc(I18n::tr("rep_footer")));
    h += QStringLiteral("</body>\n</html>\n");
    return h;
}

QString AcousticReportBuilder::buildCsv(const AcousticReportInput &in)
{
    QString out;
    out += QStringLiteral(
        "source,section,metric,band,value,unit,valid,quality,warning\n");

    // メタ情報 (列幅を本体と揃える)
    const auto metaRow = [&out](const QString &key, const QString &value) {
        out += QStringLiteral("meta,info,%1,,%2,,,,\n")
                   .arg(csvEscapeField(key), csvEscapeField(value));
    };
    metaRow(QStringLiteral("project_title"), in.projectTitle);
    metaRow(QStringLiteral("rir_file"), in.rirFile);
    metaRow(QStringLiteral("voice_file"), in.voiceFile);
    metaRow(QStringLiteral("calibration_state"),
            QString::number(in.calibrationState));
    if (in.calibrationState == 0)
        metaRow(QStringLiteral("calibration_offset_db"),
                QString::number(in.calibrationOffsetDb, 'f', 1));
    metaRow(QStringLiteral("auralization_dry_file"), in.auralizationDryFile);
    metaRow(QStringLiteral("auralization_output_file"), in.auralizationOutputFile);

    // 未実行は「実行していない」ことを明示的に記録する (空欄と区別する)
    out += QStringLiteral("meta,status,rir_analysis,,%1,,,,\n")
               .arg(in.hasRir ? QStringLiteral("done") : QStringLiteral("not_run"));
    out += QStringLiteral("meta,status,vocal_analysis,,%1,,,,\n")
               .arg(in.hasVocal ? QStringLiteral("done")
                                : QStringLiteral("not_run"));

    if (in.hasRir)
        out += prefixCsv(AcousticResultModel::toCsv(in.rir),
                         QStringLiteral("rir"));
    if (in.hasVocal)
        out += prefixCsv(AcousticResultModel::toCsv(in.vocal),
                         QStringLiteral("vocal"));
    return out;
}
