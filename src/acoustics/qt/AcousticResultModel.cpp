// AcousticResultModel.cpp
#include "AcousticResultModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace ofd;
using namespace ofd::acoustics;

namespace {

struct MetricDef {
    const char *name;
    const char *unit;
    int decimals;
    const MetricValue AcousticMetricsSet::*member;
};

// 表示順: 残響系 → 明瞭度系 → 重心時間
const MetricDef kMetrics[] = {
    { "EDT", "s",  2, &AcousticMetricsSet::edt },
    { "T20", "s",  2, &AcousticMetricsSet::t20 },
    { "T30", "s",  2, &AcousticMetricsSet::t30 },
    { "C50", "dB", 1, &AcousticMetricsSet::c50 },
    { "C80", "dB", 1, &AcousticMetricsSet::c80 },
    { "D50", "-",  2, &AcousticMetricsSet::d50 },
    { "Ts",  "ms", 1, &AcousticMetricsSet::ts },
};
const int kMetricCount = int(sizeof(kMetrics) / sizeof(kMetrics[0]));

// Ts は秒で来るので表示は ms に換算する
double displayValue(const MetricDef &def, double v)
{
    return (QLatin1String(def.name) == QLatin1String("Ts")) ? v * 1000.0 : v;
}

QString bandLabel(const Band &band)
{
    if (band.fullBand) return QStringLiteral("Full band");
    return QString::fromStdString(band.label);
}

QJsonObject metricJson(const MetricValue &m)
{
    return QJsonObject{
        { "value", m.value },
        { "valid", m.valid },
        { "quality", AcousticResultModel::qualityToken(m.quality) },
        { "warning", QString::fromStdString(m.warning) } };
}

QString csvEscape(QString s)
{
    if (s.contains(',') || s.contains('"') || s.contains('\n')) {
        s.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        s = QStringLiteral("\"") + s + QStringLiteral("\"");
    }
    return s;
}

} // namespace

QString AcousticResultModel::qualityToken(AnalysisQuality q)
{
    switch (q) {
    case AnalysisQuality::Valid:   return QStringLiteral("valid");
    case AnalysisQuality::Warning: return QStringLiteral("warning");
    case AnalysisQuality::Invalid: break;
    }
    return QStringLiteral("invalid");
}

QString AcousticResultModel::reflectionBinLabel(double delaySeconds)
{
    const double ms = delaySeconds * 1000.0;
    if (ms < 20.0)  return QStringLiteral("0-20ms");
    if (ms < 80.0)  return QStringLiteral("20-80ms");
    if (ms < 200.0) return QStringLiteral("80-200ms");
    return QStringLiteral("200+ms");
}

QVector<AcousticResultRow>
AcousticResultModel::metricRows(const RirAnalysisResult &result)
{
    QVector<AcousticResultRow> rows;
    rows.reserve(int(result.bands.size()) * kMetricCount);
    for (const BandMetricsResult &bm : result.bands) {
        for (int i = 0; i < kMetricCount; ++i) {
            const MetricDef &def = kMetrics[i];
            const MetricValue &m = bm.metrics.*(def.member);
            AcousticResultRow row;
            row.metric = QLatin1String(def.name);
            row.band = bandLabel(bm.band);
            row.unit = QLatin1String(def.unit);
            if (!bm.filterOk) {
                row.valid = false;
                row.quality = QStringLiteral("invalid");
                row.warning = QString::fromStdString(bm.filterWarning);
            } else {
                row.valid = m.valid;
                row.value = displayValue(def, m.value);
                if (m.valid)
                    row.valueText = QString::number(row.value, 'f', def.decimals);
                row.quality = qualityToken(m.quality);
                row.warning = QString::fromStdString(m.warning);
            }
            rows.push_back(row);
        }
    }
    return rows;
}

QString AcousticResultModel::toCsv(const RirAnalysisResult &result)
{
    QString out;
    out += QStringLiteral("section,metric,band,value,unit,valid,quality,warning\n");

    for (const AcousticResultRow &r : metricRows(result)) {
        out += QStringLiteral("metrics,%1,%2,%3,%4,%5,%6,%7\n")
                   .arg(r.metric, csvEscape(r.band),
                        r.valid ? r.valueText : QString(),
                        r.unit, r.valid ? QStringLiteral("1")
                                        : QStringLiteral("0"),
                        r.quality, csvEscape(r.warning));
    }

    // 直接音・前処理サマリ
    const PreprocessInfo &pp = result.preprocess;
    out += QStringLiteral("summary,direct_time,,%1,s,%2,%3,%4\n")
               .arg(QString::number(result.directSound.timeSeconds, 'f', 6),
                    result.directSound.found ? QStringLiteral("1")
                                             : QStringLiteral("0"),
                    qualityToken(result.directSound.quality),
                    csvEscape(QString::fromStdString(result.directSound.warning)));
    out += QStringLiteral("summary,dynamic_range,,%1,dB,1,,\n")
               .arg(QString::number(pp.dynamicRangeDb, 'f', 1));
    out += QStringLiteral("summary,noise_floor,,%1,dBFS,1,,\n")
               .arg(QString::number(pp.noiseFloorDb, 'f', 1));
    out += QStringLiteral("summary,overall_quality,,%1,,1,%1,\n")
               .arg(qualityToken(result.overallQuality));
    if (result.absoluteSplDb.valid)
        out += QStringLiteral("summary,absolute_spl,,%1,dB SPL,1,%2,\n")
                   .arg(QString::number(result.absoluteSplDb.value, 'f', 1),
                        qualityToken(result.absoluteSplDb.quality));

    // 反射一覧
    out += QStringLiteral(
        "section,index,arrival_ms,delay_ms,relative_level_db,bin,confidence,\n");
    int idx = 1;
    for (const ReflectionEvent &e : result.reflections) {
        out += QStringLiteral("reflections,%1,%2,%3,%4,%5,%6,\n")
                   .arg(QString::number(idx++),
                        QString::number(e.arrivalTime * 1000.0, 'f', 2),
                        QString::number(e.delayFromDirect * 1000.0, 'f', 2),
                        QString::number(e.relativeLevelDb, 'f', 1),
                        reflectionBinLabel(e.delayFromDirect),
                        QString::number(e.confidence, 'f', 2));
    }

    // 警告
    for (const std::string &w : result.warnings)
        out += QStringLiteral("warnings,,,,,,,%1\n")
                   .arg(csvEscape(QString::fromStdString(w)));
    return out;
}

QString AcousticResultModel::toJson(const RirAnalysisResult &result)
{
    QJsonObject root;
    root["overall_quality"] = qualityToken(result.overallQuality);

    const PreprocessInfo &pp = result.preprocess;
    root["preprocess"] = QJsonObject{
        { "sample_count", double(pp.sampleCount) },
        { "duration_s", pp.durationSeconds },
        { "dc_offset", pp.dcOffset },
        { "dc_removed", pp.dcRemoved },
        { "clipping_detected", pp.clippingDetected },
        { "clipped_run_count", pp.clippedRunCount },
        { "peak_dbfs", pp.peakDb },
        { "noise_floor_dbfs", pp.noiseFloorDb },
        { "dynamic_range_db", pp.dynamicRangeDb } };

    const DirectSoundResult &ds = result.directSound;
    root["direct_sound"] = QJsonObject{
        { "found", ds.found },
        { "sample_index", double(ds.sampleIndex) },
        { "time_s", ds.timeSeconds },
        { "level_dbfs", ds.levelDb },
        { "quality", qualityToken(ds.quality) },
        { "warning", QString::fromStdString(ds.warning) } };

    QJsonArray bands;
    for (const BandMetricsResult &bm : result.bands) {
        QJsonObject b;
        b["band"] = bandLabel(bm.band);
        b["center_hz"] = bm.band.centerHz;
        b["filter_ok"] = bm.filterOk;
        if (!bm.filterOk)
            b["filter_warning"] = QString::fromStdString(bm.filterWarning);
        b["edt"] = metricJson(bm.metrics.edt);
        b["t20"] = metricJson(bm.metrics.t20);
        b["t30"] = metricJson(bm.metrics.t30);
        b["c50"] = metricJson(bm.metrics.c50);
        b["c80"] = metricJson(bm.metrics.c80);
        b["d50"] = metricJson(bm.metrics.d50);
        b["ts"]  = metricJson(bm.metrics.ts);
        bands.append(b);
    }
    root["bands"] = bands;

    QJsonArray refl;
    for (const ReflectionEvent &e : result.reflections) {
        refl.append(QJsonObject{
            { "arrival_ms", e.arrivalTime * 1000.0 },
            { "delay_ms", e.delayFromDirect * 1000.0 },
            { "relative_level_db", e.relativeLevelDb },
            { "bin", reflectionBinLabel(e.delayFromDirect) },
            { "confidence", e.confidence } });
    }
    root["reflections"] = refl;

    const ReflectionTimeSummary &rs = result.reflectionSummary;
    static const char *kBinKeys[ReflectionTimeSummary::kBinCount] = {
        "0_20ms", "20_80ms", "80_200ms", "200ms_plus" };
    QJsonObject bins;
    for (int i = 0; i < ReflectionTimeSummary::kBinCount; ++i) {
        bins[kBinKeys[i]] = QJsonObject{
            { "count", rs.counts[i] },
            { "energy", rs.energies[i] },
            { "energy_ratio", rs.energyRatios[i] } };
    }
    root["reflection_summary"] = bins;

    if (result.absoluteSplDb.valid)
        root["absolute_spl_db"] = metricJson(result.absoluteSplDb);

    QJsonArray warnings;
    for (const std::string &w : result.warnings)
        warnings.append(QString::fromStdString(w));
    root["warnings"] = warnings;

    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Indented));
}

// ── 歌声分析 (VocalAnalysisResult) ──────────────────────────────────────────
namespace {

AcousticResultRow vocalRow(const char *metric, const QString &band,
                           const MetricValue &m, const char *unit,
                           int decimals)
{
    AcousticResultRow row;
    row.metric = QLatin1String(metric);
    row.band = band;
    row.unit = QLatin1String(unit);
    row.valid = m.valid;
    row.value = m.value;
    if (m.valid)
        row.valueText = QString::number(m.value, 'f', decimals);
    row.quality = AcousticResultModel::qualityToken(m.quality);
    row.warning = QString::fromStdString(m.warning);
    return row;
}

} // namespace

QVector<AcousticResultRow>
AcousticResultModel::vocalRows(const VocalAnalysisResult &r)
{
    const QString full = QStringLiteral("Full band");
    QVector<AcousticResultRow> rows;
    rows.reserve(16 + int(r.bandEnergies.size()));

    // F0 統計
    rows.push_back(vocalRow("F0 median", full, r.f0MedianHz, "Hz", 1));
    rows.push_back(vocalRow("F0 mean", full, r.f0MeanHz, "Hz", 1));
    rows.push_back(vocalRow("F0 min", full, r.f0MinHz, "Hz", 1));
    rows.push_back(vocalRow("F0 max", full, r.f0MaxHz, "Hz", 1));
    rows.push_back(vocalRow("Pitch stability", full,
                            r.pitchStabilityCents, "cent", 1));

    // ビブラート (metric 側の warning が空なら vibrato 全体の理由を使う)
    AcousticResultRow vr =
        vocalRow("Vibrato rate", full, r.vibrato.rateHz, "Hz", 2);
    AcousticResultRow vd =
        vocalRow("Vibrato depth", full, r.vibrato.depthCents, "cent", 1);
    const QString vibratoWhy = QString::fromStdString(r.vibrato.warning);
    if (vr.warning.isEmpty()) vr.warning = vibratoWhy;
    if (vd.warning.isEmpty()) vd.warning = vibratoWhy;
    rows.push_back(vr);
    rows.push_back(vd);

    // スペクトル系・声質系
    rows.push_back(vocalRow("HNR", full, r.hnrDb, "dB", 1));
    rows.push_back(vocalRow("Spectral centroid", full,
                            r.spectralCentroidHz, "Hz", 0));
    rows.push_back(vocalRow("Singer formant ratio (2-4k/0-2k)", full,
                            r.singerFormantRatioDb, "dB", 1));

    // 帯域エネルギー (LTAS 積算)
    for (const BandEnergyValue &b : r.bandEnergies)
        rows.push_back(vocalRow("Band energy", bandLabel(b.band),
                                b.levelDb, "dB", 1));

    // レベル (dBFS は常に valid、SPL は Absolute 校正時のみ valid)
    rows.push_back(vocalRow("Peak", full, r.peakDbfs, "dBFS", 1));
    rows.push_back(vocalRow("RMS", full, r.rmsDbfs, "dBFS", 1));
    rows.push_back(vocalRow("Leq", full, r.leqDbfs, "dBFS", 1));
    rows.push_back(vocalRow("Leq (SPL)", full, r.leqSplDb, "dB SPL", 1));
    rows.push_back(vocalRow("Peak (SPL)", full, r.peakSplDb, "dB SPL", 1));
    return rows;
}

QString AcousticResultModel::toCsv(const VocalAnalysisResult &r)
{
    QString out;
    out += QStringLiteral("section,metric,band,value,unit,valid,quality,warning\n");
    for (const AcousticResultRow &row : vocalRows(r)) {
        out += QStringLiteral("metrics,%1,%2,%3,%4,%5,%6,%7\n")
                   .arg(csvEscape(row.metric), csvEscape(row.band),
                        row.valid ? row.valueText : QString(),
                        row.unit, row.valid ? QStringLiteral("1")
                                            : QStringLiteral("0"),
                        row.quality, csvEscape(row.warning));
    }

    // サマリー (分析パラメータと有声率)
    out += QStringLiteral("summary,total_frames,,%1,,1,,\n")
               .arg(QString::number(qulonglong(r.totalFrameCount)));
    out += QStringLiteral("summary,voiced_frames,,%1,,1,,\n")
               .arg(QString::number(qulonglong(r.voicedFrameCount)));
    out += QStringLiteral("summary,voiced_ratio,,%1,,1,,\n")
               .arg(QString::number(r.voicedRatio, 'f', 3));
    out += QStringLiteral("summary,frame_length,,%1,s,1,,\n")
               .arg(QString::number(r.frameSeconds, 'f', 4));
    out += QStringLiteral("summary,hop,,%1,s,1,,\n")
               .arg(QString::number(r.hopSeconds, 'f', 4));
    out += QStringLiteral("summary,f0_search_min,,%1,Hz,1,,\n")
               .arg(QString::number(r.f0SearchMinHz, 'f', 1));
    out += QStringLiteral("summary,f0_search_max,,%1,Hz,1,,\n")
               .arg(QString::number(r.f0SearchMaxHz, 'f', 1));
    out += QStringLiteral("summary,overall_quality,,%1,,1,%1,\n")
               .arg(qualityToken(r.overallQuality));

    // 倍音レベル (H1 相対)
    for (int i = 0; i < int(r.harmonicLevelsDb.size()); ++i) {
        const MetricValue &h = r.harmonicLevelsDb[std::size_t(i)];
        out += QStringLiteral("harmonics,H%1,,%2,dB re H1,%3,%4,%5\n")
                   .arg(QString::number(i + 1),
                        h.valid ? QString::number(h.value, 'f', 1) : QString(),
                        h.valid ? QStringLiteral("1") : QStringLiteral("0"),
                        qualityToken(h.quality),
                        csvEscape(QString::fromStdString(h.warning)));
    }

    // F0 軌跡 (無声フレームは f0 空欄)
    out += QStringLiteral("section,time_s,f0_hz,voiced,rms_dbfs,,,\n");
    for (const F0Frame &f : r.f0Track) {
        out += QStringLiteral("f0_track,%1,%2,%3,%4,,,\n")
                   .arg(QString::number(f.timeSeconds, 'f', 4),
                        f.voiced ? QString::number(f.f0Hz, 'f', 2) : QString(),
                        f.voiced ? QStringLiteral("1") : QStringLiteral("0"),
                        QString::number(f.rmsDbfs, 'f', 1));
    }

    for (const std::string &w : r.warnings)
        out += QStringLiteral("warnings,,,,,,,%1\n")
                   .arg(csvEscape(QString::fromStdString(w)));
    return out;
}

QString AcousticResultModel::toJson(const VocalAnalysisResult &r)
{
    QJsonObject root;
    root["overall_quality"] = qualityToken(r.overallQuality);

    root["analysis"] = QJsonObject{
        { "frame_s", r.frameSeconds },
        { "hop_s", r.hopSeconds },
        { "f0_search_min_hz", r.f0SearchMinHz },
        { "f0_search_max_hz", r.f0SearchMaxHz },
        { "total_frames", double(r.totalFrameCount) },
        { "voiced_frames", double(r.voicedFrameCount) },
        { "voiced_ratio", r.voicedRatio } };

    root["f0"] = QJsonObject{
        { "median_hz", metricJson(r.f0MedianHz) },
        { "mean_hz", metricJson(r.f0MeanHz) },
        { "min_hz", metricJson(r.f0MinHz) },
        { "max_hz", metricJson(r.f0MaxHz) },
        { "stability_cents", metricJson(r.pitchStabilityCents) } };

    root["vibrato"] = QJsonObject{
        { "valid", r.vibrato.valid },
        { "rate_hz", metricJson(r.vibrato.rateHz) },
        { "depth_cents", metricJson(r.vibrato.depthCents) },
        { "warning", QString::fromStdString(r.vibrato.warning) } };

    root["hnr_db"] = metricJson(r.hnrDb);
    root["spectral_centroid_hz"] = metricJson(r.spectralCentroidHz);
    root["singer_formant_ratio_db"] = metricJson(r.singerFormantRatioDb);

    QJsonArray harmonics;
    for (const MetricValue &h : r.harmonicLevelsDb)
        harmonics.append(metricJson(h));
    root["harmonic_levels_db_re_h1"] = harmonics;

    QJsonArray bands;
    for (const BandEnergyValue &b : r.bandEnergies) {
        bands.append(QJsonObject{
            { "band", bandLabel(b.band) },
            { "center_hz", b.band.centerHz },
            { "level_db", metricJson(b.levelDb) } });
    }
    root["band_energies"] = bands;

    root["levels"] = QJsonObject{
        { "peak_dbfs", metricJson(r.peakDbfs) },
        { "rms_dbfs", metricJson(r.rmsDbfs) },
        { "leq_dbfs", metricJson(r.leqDbfs) },
        { "leq_spl_db", metricJson(r.leqSplDb) },
        { "peak_spl_db", metricJson(r.peakSplDb) } };

    QJsonArray track;
    for (const F0Frame &f : r.f0Track) {
        track.append(QJsonObject{
            { "time_s", f.timeSeconds },
            { "f0_hz", f.voiced ? f.f0Hz : QJsonValue() },
            { "voiced", f.voiced },
            { "rms_dbfs", f.rmsDbfs } });
    }
    root["f0_track"] = track;

    QJsonObject ltas;
    ltas["valid"] = r.ltas.valid;
    ltas["frame_count"] = double(r.ltas.frameCount);
    ltas["warning"] = QString::fromStdString(r.ltas.warning);
    QJsonArray freqs, levels;
    for (double f : r.ltas.frequenciesHz) freqs.append(f);
    for (double l : r.ltas.levelsDb) levels.append(l);
    ltas["frequencies_hz"] = freqs;
    ltas["levels_db"] = levels;
    root["ltas"] = ltas;

    QJsonArray warnings;
    for (const std::string &w : r.warnings)
        warnings.append(QString::fromStdString(w));
    root["warnings"] = warnings;

    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Indented));
}
