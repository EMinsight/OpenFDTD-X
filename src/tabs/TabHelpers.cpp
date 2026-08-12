// TabHelpers.cpp
#include "TabHelpers.h"
#include <QStandardItemModel>
#include <QComboBox>
#include "../core/PostPrereq.h"
#include "../I18n.h"

#include <QAbstractButton>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTextStream>
#include <algorithm>

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("th_notimpl", "未実装", "Not implemented");
    ofd::I18n::reg("th_notimpl_why", "未実装 — %1", "Not implemented - %1");
    // 使い回せる「できない理由」(tabhelp::notimpl::)
    ofd::I18n::reg("th_ni_format",
                   "書き出す/読み込む外部書式の仕様が確認できていません",
                   "the specification of the external file format is not "
                   "confirmed");
    ofd::I18n::reg("th_ni_parser", "その書式の読み手 (パーサ) がありません",
                   "there is no parser for that format");
    ofd::I18n::reg("th_ni_kernel", "カーネル側に対応する入力・出力がありません",
                   "the kernel has no matching input or output");
    ofd::I18n::reg("th_ni_data", "同梱していないデータが要ります",
                   "it needs data that is not bundled");
    ofd::I18n::reg("th_ni_engine", "その計算を行うエンジンがありません",
                   "there is no engine that performs that calculation");
    ofd::I18n::reg("th_ni_audio",
                   "アプリは音声の入出力を持ちません (依存を増やさない方針)",
                   "the application has no audio input or output (to avoid "
                   "adding dependencies)");
    ofd::I18n::reg("th_ni_external", "外部アプリの起動が要ります",
                   "it requires launching an external application");
    ofd::I18n::reg("th_ni_control",
                   "実行中の中断・再開をソルバー起動側が持っていません",
                   "the solver launcher cannot pause and resume a run");
    ofd::I18n::reg("th_ni_report", "報告書の様式が決まっていません",
                   "the report layout is not settled");
    ofd::I18n::reg("th_ni_plot", "この図を描く実装がありません",
                   "there is no implementation that draws this plot");
    ofd::I18n::reg("th_ni_model", "その物理モデルが実装されていません",
                   "that physical model is not implemented");
    ofd::I18n::reg("th_sample",
        "⚠ サンプル表示 — 実行結果ではありません (機能未実装)",
        "⚠ Sample display — not a computed result (feature not implemented)");
    ofd::I18n::reg("th_unwired",
        "▸ この設定は現在計算へ反映されません (未実装)",
        "▸ These settings are not applied to any computation yet "
        "(not implemented)");
    // 主語 (%1) は名詞句なので、助詞を続けると「…チェック群 は」のように
    // 不自然になる。ダッシュで受けて助詞を避ける (英語も同じ構文にする)
    ofd::I18n::reg("th_unwired_what",
        "▸ %1 — 現在計算へ反映されません (未実装)",
        "▸ %1 — not applied to any computation yet (not implemented)");
    // ポスト作図の前提条件 (core/PostPrereq)
    ofd::I18n::reg("pp_blocked_fmt",
        "⚠ チェックが入っていても、このプロジェクトでは出力されない項目が "
        "%1 件あります: %2。"
        "給電点は「④ 波源」タブ、観測点は「⑤ モニター」タブ、"
        "frequency1 / frequency2 は「全般」タブで設定します。",
        "⚠ %1 item(s) are checked but produce no plot for this project: %2. "
        "Feeds are set on the Source tab, observation points on the Monitors "
        "tab, and frequency1 / frequency2 on the General tab.");
    ofd::I18n::reg("pp_it_iter",     "収束状況", "convergence");
    ofd::I18n::reg("pp_it_feed",     "給電点波形・スペクトル", "feed waveform");
    ofd::I18n::reg("pp_it_point",    "観測点波形・スペクトル", "probe waveform");
    ofd::I18n::reg("pp_it_smith",    "スミスチャート", "Smith chart");
    ofd::I18n::reg("pp_it_zin",      "入力インピーダンス", "input impedance");
    ofd::I18n::reg("pp_it_yin",      "入力アドミタンス", "input admittance");
    ofd::I18n::reg("pp_it_ref",      "反射係数", "reflection");
    ofd::I18n::reg("pp_it_spara",    "S パラメータ", "S-parameters");
    ofd::I18n::reg("pp_it_coupling", "結合係数", "coupling");
    ofd::I18n::reg("pp_it_far0d",    "遠方界周波数特性", "far field vs frequency");
    ofd::I18n::reg("pp_it_far1d",    "遠方界指向性", "far-field pattern");
    ofd::I18n::reg("pp_it_far2d",    "遠方界全方向 (3D)", "far field (3D)");
    ofd::I18n::reg("pp_it_near1d",   "近傍界 1D", "near field 1D");
    ofd::I18n::reg("pp_it_near2d",   "近傍界 2D", "near field 2D");
    ofd::I18n::reg("pp_why_nofeed",  "給電点が無い", "no feed");
    ofd::I18n::reg("pp_why_nopoint", "観測点が無い", "no observation point");
    ofd::I18n::reg("pp_why_nofeedpoint", "給電点も観測点も無い",
                   "neither a feed nor an observation point");
    ofd::I18n::reg("pp_why_nofreq1", "frequency1 が無い", "no frequency1");
    ofd::I18n::reg("pp_why_nofreq2", "frequency2 が無い", "no frequency2");
    ofd::I18n::reg("pp_why_noentry", "対象の行が 1 つも無い",
                   "no entry of this kind");
    ofd::I18n::reg("th_unwired_mixed",
        "▸ %1 — 現在計算へ反映されません (未実装)。反映されるもの: %2",
        "▸ %1 — not applied to any computation yet (not implemented). "
        "Applied: %2");
    return true;
}();
} // namespace

namespace ofd {
namespace tabhelp {

acoustics::AcousticResult<acoustics::ConvolutionInfo>
convolveWithPrep(const QString &dryPath, const QString &rirPath,
                 const QString &outputPath, int gainMode,
                 const audioedit::SourcePrep &prep, bool *outPrepped,
                 std::vector<double> *outDry, std::vector<double> *outWet,
                 double *outSampleRate,
                 QtAcousticAdapter::RirResampleNote *outResample)
{
    using acoustics::AcousticResult;
    using acoustics::AudioBuffer;
    using acoustics::ConvolutionInfo;
    typedef AcousticResult<ConvolutionInfo> Result;

    if (outPrepped) *outPrepped = false;
    const AcousticResult<AudioBuffer> dry = QtAcousticAdapter::readWav(dryPath);
    if (!dry.success())
        return Result::error(dry.errorCode(),
                             std::string("dry: ") + dry.message());
    const AcousticResult<AudioBuffer> rir = QtAcousticAdapter::readWav(rirPath);
    if (!rir.success())
        return Result::error(rir.errorCode(),
                             std::string("rir: ") + rir.message());

    const AudioBuffer dryBuf = audioedit::prepareSource(dry.value(), prep);
    if (dryBuf.channels.empty() || dryBuf.channels[0].empty())
        return Result::error(acoustics::AcousticErrorCode::InvalidArgument,
                             "the source pre-processing left an empty dry "
                             "signal");
    if (outPrepped) *outPrepped = !prep.isIdentity();
    return QtAcousticAdapter::convolveBuffers(dryBuf, rir.value(), outputPath,
                                              gainMode, outDry, outWet,
                                              outSampleRate, outResample);
}

audioedit::SourcePrep sourcePrep(const AcousticOpts &a)
{
    audioedit::SourcePrep p;
    p.trimStartSec = a.wavTrimStart_s;
    p.trimEndSec   = a.wavTrimEnd_s;
    p.gainDb       = a.wavGain_dB;
    p.highPass     = a.wavHighPass;
    p.highPassHz   = a.wavHighPassHz;
    return p;
}

// RIR のナイキストがこの値未満なら「高域が無い」と警告する。
// 16 kHz は可聴帯域上端 (20 kHz) より下だが、これを下回ると音色が
// はっきり曇るので実用上の境目として採る。
double rirBandWarnThresholdHz() { return 16000.0; }

QStringList rirSampleRateNotes(double rirFsHz, double outFsHz,
                               double validBandHz)
{
    QStringList notes;
    if (!(rirFsHz > 0.0) || !(outFsHz > 0.0)) return notes;
    if (rirFsHz != outFsHz)
        notes << I18n::tr("aur_resampled_note")
                     .arg(QString::number(qRound64(rirFsHz)),
                          QString::number(qRound64(outFsHz)));
    // 有効帯域: ソルバーが申告した fmax があればそれ、無ければナイキスト。
    // FDTD は格子分散があるので fmax ≈ fs/17.5 ≪ fs/2 — 申告値がある方が
    // 正しく、無いときだけナイキストで代用する (実測 RIR など)。
    const bool known = (validBandHz > 0.0);
    const double band = known ? validBandHz : 0.5 * rirFsHz;
    if (band < rirBandWarnThresholdHz())
        notes << I18n::tr(known ? "aur_rir_band_solver_note"
                                : "aur_rir_band_note")
                     .arg(QString::number(qRound64(band)),
                          QString::number(qRound64(rirFsHz)));
    return notes;
}

void markNotImplemented(QAbstractButton *b, const QString &why)
{
    if (!b) return;
    b->setEnabled(false);
    // 「未実装」だけで終わらせず、何が足りないのかを必ず添える
    b->setToolTip(why.isEmpty() ? I18n::tr("th_notimpl")
                                : I18n::tr("th_notimpl_why").arg(why));
}
// 項目を消さずに無効化する (理由をツールチップに残す)。
// QComboBox の項目は QStandardItemModel なので、フラグから Enabled を落とす。
void disableComboItems(QComboBox *box, const QVector<int> &indices,
                       const QString &why)
{
    if (!box) return;
    auto *model = qobject_cast<QStandardItemModel *>(box->model());
    if (!model) return;
    for (int i : indices) {
        QStandardItem *it = model->item(i);
        if (!it) continue;
        it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
        it->setToolTip(why);
    }
}
QLabel *sampleNote(QWidget *parent)
{
    auto *l = new QLabel(I18n::tr("th_sample"), parent);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:#B8860B;");
    return l;
}

QLabel *unwiredNote(QWidget *parent)
{
    auto *l = new QLabel(I18n::tr("th_unwired"), parent);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:palette(mid);");
    return l;
}

QLabel *unwiredNote(QWidget *parent, const QString &what, const QString &wired)
{
    const QString text =
        wired.isEmpty() ? I18n::tr("th_unwired_what").arg(what)
                        : I18n::tr("th_unwired_mixed").arg(what, wired);
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:palette(mid);");
    return l;
}

// ── ポスト作図の前提条件 ────────────────────────────────────────────────────
QString postPrereqWarning(const Project &p, int group)
{
    struct Row { PostItem item; bool enabled; const char *nameKey; };
    const PostOpts &po = p.post();
    const Row rows1[] = {
        { PostItem::Iter,     po.plotiter,      "pp_it_iter" },
        { PostItem::Feed,     po.plotfeed,      "pp_it_feed" },
        { PostItem::Point,    po.plotpoint,     "pp_it_point" },
        { PostItem::Smith,    po.plotsmith,     "pp_it_smith" },
        { PostItem::Zin,      po.zin.enabled,   "pp_it_zin" },
        { PostItem::Yin,      po.yin.enabled,   "pp_it_yin" },
        { PostItem::Ref,      po.ref.enabled,   "pp_it_ref" },
        { PostItem::Spara,    po.spara.enabled, "pp_it_spara" },
        { PostItem::Coupling, po.coupling.enabled, "pp_it_coupling" },
    };
    const Row rows2[] = {
        { PostItem::Far0d,  po.far0d,               "pp_it_far0d" },
        { PostItem::Far1d,  !po.far1d.isEmpty(),    "pp_it_far1d" },
        { PostItem::Far2d,  po.far2d,               "pp_it_far2d" },
        { PostItem::Near1d, !po.near1d.isEmpty(),   "pp_it_near1d" },
        { PostItem::Near2d, !po.near2d.isEmpty(),   "pp_it_near2d" },
    };
    const Row *rows = (group == 0) ? rows1 : rows2;
    const int n = (group == 0) ? int(sizeof(rows1) / sizeof(rows1[0]))
                               : int(sizeof(rows2) / sizeof(rows2[0]));

    const PostInputs in = postInputsOf(p);
    QStringList blocked;
    for (int i = 0; i < n; ++i) {
        if (!rows[i].enabled) continue;             // チェックされていない
        const PostBlocker b = postBlocker(rows[i].item, in);
        if (b == PostBlocker::None) continue;
        const char *why =
            (b == PostBlocker::NoFeed)         ? "pp_why_nofeed" :
            (b == PostBlocker::NoPoint)        ? "pp_why_nopoint" :
            (b == PostBlocker::NoFeedAndPoint) ? "pp_why_nofeedpoint" :
            (b == PostBlocker::NoFreq1)        ? "pp_why_nofreq1" :
            (b == PostBlocker::NoFreq2)        ? "pp_why_nofreq2" : "pp_why_noentry";
        blocked << I18n::tr(rows[i].nameKey) + QStringLiteral(" (")
                       + I18n::tr(why) + QStringLiteral(")");
    }
    if (blocked.isEmpty()) return QString();
    return I18n::tr("pp_blocked_fmt")
               .arg(blocked.size())
               .arg(blocked.join(QStringLiteral("、")));
}

QString qualityBadge(const QString &token)
{
    if (token == QLatin1String("valid"))   return I18n::tr("rir_q_valid");
    if (token == QLatin1String("warning")) return I18n::tr("rir_q_warning");
    return I18n::tr("rir_q_invalid");
}

QColor qualityColor(const QString &token)
{
    if (token == QLatin1String("valid"))   return QColor(0x2E, 0x8B, 0x57);
    if (token == QLatin1String("warning")) return QColor(0xB8, 0x86, 0x0B);
    return QColor(0xC0, 0x39, 0x2B);
}

QTableWidgetItem *roItem(const QString &text)
{
    auto *it = new QTableWidgetItem(text);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return it;
}

void envelopeSeries(const std::vector<double> &x, double fs, int maxBins,
                    TimeUnit unit, QVector<QPointF> &top,
                    QVector<QPointF> &bottom)
{
    top.clear(); bottom.clear();
    if (x.empty() || fs <= 0 || maxBins <= 0) return;
    const double tScale = (unit == TimeUnit::Milliseconds) ? 1000.0 : 1.0;
    const int n = int(x.size());
    const int bins = std::min(maxBins, n);
    const double perBin = double(n) / bins;
    top.reserve(bins); bottom.reserve(bins);
    for (int b = 0; b < bins; ++b) {
        const int i0 = int(b * perBin);
        const int i1 = std::min(n, std::max(i0 + 1, int((b + 1) * perBin)));
        double lo = x[i0], hi = x[i0];
        for (int i = i0 + 1; i < i1; ++i) {
            lo = std::min(lo, x[i]);
            hi = std::max(hi, x[i]);
        }
        const double t = (i0 + (i1 - 1 - i0) * 0.5) / fs * tScale;
        top.push_back(QPointF(t, hi));
        bottom.push_back(QPointF(t, lo));
    }
}

void saveTextFile(QWidget *parent, const QString &caption,
                  const QString &suggested, const QString &filter,
                  const QString &content)
{
    const QString path = QFileDialog::getSaveFileName(parent, caption,
                                                      suggested, filter);
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(parent, caption, f.errorString());
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
}

} // namespace tabhelp
} // namespace ofd
