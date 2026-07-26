// ResourceDialog.cpp
#include "ResourceDialog.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

using namespace ofd;

// ── ダイアログ固有語彙 (res_) — file-local 登録 ─────────────────────────────
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("res_title", "⚙ Resource Configuration (Lumerical風)",
                   "⚙ Resource Configuration (Lumerical-style)");
    ofd::I18n::reg("res_hint",
        "FDTDの並列実行設定。プロセス数 × スレッド数 = 利用コア数。\n"
        "「プロセス × スレッド = 全コア」となるように設定してください。",
        "Parallel execution for FDTD: processes x threads = cores in use.\n"
        "Configure so that processes x threads equals the total core count.");
    ofd::I18n::reg("res_processes", "プロセス数 (MPI)", "Processes (MPI)");
    ofd::I18n::reg("res_threads", "スレッド数 (OpenMP)", "Threads (OpenMP)");
    ofd::I18n::reg("res_total", "合計コア使用", "Total cores in use");
    ofd::I18n::reg("res_avail", "/ %1 (利用可能)", "/ %1 (available)");
    ofd::I18n::reg("res_over", "超過!", "Over!");
    ofd::I18n::reg("res_optimal", "最適", "Optimal");
    ofd::I18n::reg("res_idle", "未使用コア %1", "%1 idle cores");
    ofd::I18n::reg("res_sweep", "並列スイープ", "Parallel sweep");
    ofd::I18n::reg("res_sweep_check", "独立シミュレーションを並列実行",
                   "Run independent simulations in parallel");
    ofd::I18n::reg("res_sweep_hint", "スイープ・最適化で有効",
                   "Effective for sweeps and optimization");
    ofd::I18n::reg("res_license", "ライセンス共有", "License sharing");
    ofd::I18n::reg("res_license_check", "新ライセンス共有機能を使う",
                   "Use the new license-sharing feature");
    ofd::I18n::reg("res_gpu", "GPU加速", "GPU acceleration");
    ofd::I18n::reg("res_cuda", "CUDA有効", "Enable CUDA");
    ofd::I18n::reg("res_mem", "メモリ上限", "Memory limit");
    ofd::I18n::reg("res_mem_unit", "GB / プロセス", "GB / process");
    ofd::I18n::reg("res_rate", "解析レート", "Solve rate");
    ofd::I18n::reg("res_rate_hint", "(現在の設定での予測)",
                   "(predicted for the current settings)");
    ofd::I18n::reg("res_bench", "ベンチマーク", "Benchmark");
    ofd::I18n::reg("res_bench_run", "▶ ベンチマーク実行", "▶ Run benchmark");
    ofd::I18n::reg("res_bench_hint", "最適設定を自動探索",
                   "Auto-search for the optimal setting");
    ofd::I18n::reg("res_cancel", "キャンセル", "Cancel");
    ofd::I18n::reg("res_apply", "適用", "Apply");
    return true;
}();

const int kMachineCores = 16;   // mock の machineCores

QFrame *sepH(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Sunken);
    return f;
}
} // namespace

ResourceDialog::ResourceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::tr("res_title"));
    setModal(true);
    setMinimumWidth(640);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *body = new QWidget(this);
    auto *bv = new QVBoxLayout(body);
    bv->setContentsMargins(16, 14, 16, 12);
    bv->setSpacing(6);

    auto *hint = new QLabel(I18n::tr("res_hint"), body);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:11px; color:palette(mid);");
    bv->addWidget(hint);
    bv->addWidget(sepH(body));

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(6);
    bv->addLayout(form);

    // ── プロセス数 / スレッド数 (1〜16 スライダ) ────────────────────────────
    auto sliderRow = [body](QSlider *&s, QLabel *&val, int def) {
        auto *h = new QHBoxLayout();
        s = new QSlider(Qt::Horizontal, body);
        s->setRange(1, 16);
        s->setValue(def);
        s->setTickPosition(QSlider::TicksBelow);
        s->setTickInterval(1);
        h->addWidget(s, 1);
        val = new QLabel(QString::number(def), body);
        val->setMinimumWidth(24);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(val);
        return h;
    };
    form->addRow(I18n::tr("res_processes"),
                 sliderRow(m_processes, m_procVal, 4));
    form->addRow(I18n::tr("res_threads"),
                 sliderRow(m_threads, m_threadVal, 4));

    // ── 合計コア使用 ────────────────────────────────────────────────────────
    auto *totalRow = new QHBoxLayout();
    m_total = new QLabel(body);
    m_total->setStyleSheet("font-size:14px; font-weight:600;");
    totalRow->addWidget(m_total);
    auto *avail = new QLabel(I18n::tr("res_avail").arg(kMachineCores), body);
    avail->setStyleSheet("color:palette(mid);");
    totalRow->addWidget(avail);
    m_badge = new QLabel(body);
    totalRow->addWidget(m_badge);
    totalRow->addStretch(1);
    form->addRow(I18n::tr("res_total"), totalRow);

    bv->addWidget(sepH(body));

    auto *form2 = new QFormLayout();
    form2->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form2->setLabelAlignment(Qt::AlignLeft);
    form2->setHorizontalSpacing(8);
    form2->setVerticalSpacing(6);
    bv->addLayout(form2);

    // ── 並列スイープ ────────────────────────────────────────────────────────
    auto *sweepRow = new QHBoxLayout();
    m_parallelSweep = new QCheckBox(I18n::tr("res_sweep_check"), body);
    m_parallelSweep->setChecked(true);
    sweepRow->addWidget(m_parallelSweep);
    auto *sweepHint = new QLabel(I18n::tr("res_sweep_hint"), body);
    sweepHint->setStyleSheet("font-size:11px; color:palette(mid);");
    sweepRow->addWidget(sweepHint);
    sweepRow->addStretch(1);
    form2->addRow(I18n::tr("res_sweep"), sweepRow);

    // ── ライセンス共有 ──────────────────────────────────────────────────────
    m_licenseShare = new QCheckBox(I18n::tr("res_license_check"), body);
    m_licenseShare->setChecked(true);
    form2->addRow(I18n::tr("res_license"), m_licenseShare);

    // ── GPU 加速 ────────────────────────────────────────────────────────────
    auto *gpuRow = new QHBoxLayout();
    m_cuda = new QCheckBox(I18n::tr("res_cuda"), body);
    gpuRow->addWidget(m_cuda);
    m_gpu = new QComboBox(body);
    m_gpu->addItems({ "GPU 0 (RTX 4090)", "GPU 1 (RTX 4090)" });
    gpuRow->addWidget(m_gpu);
    gpuRow->addStretch(1);
    form2->addRow(I18n::tr("res_gpu"), gpuRow);

    // ── メモリ上限 ──────────────────────────────────────────────────────────
    auto *memRow = new QHBoxLayout();
    m_memLimit = new QLineEdit("64", body);
    m_memLimit->setMaximumWidth(100);
    memRow->addWidget(m_memLimit);
    auto *memUnit = new QLabel(I18n::tr("res_mem_unit"), body);
    memUnit->setStyleSheet("color:palette(mid);");
    memRow->addWidget(memUnit);
    memRow->addStretch(1);
    form2->addRow(I18n::tr("res_mem"), memRow);

    bv->addWidget(sepH(body));

    auto *form3 = new QFormLayout();
    form3->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form3->setLabelAlignment(Qt::AlignLeft);
    form3->setHorizontalSpacing(8);
    form3->setVerticalSpacing(6);
    bv->addLayout(form3);

    // ── 解析レート / ベンチマーク ───────────────────────────────────────────
    auto *rateRow = new QHBoxLayout();
    rateRow->addWidget(new QLabel("~245 Mnode/s", body));
    auto *rateHint = new QLabel(I18n::tr("res_rate_hint"), body);
    rateHint->setStyleSheet("font-size:11px; color:palette(mid);");
    rateRow->addWidget(rateHint);
    rateRow->addStretch(1);
    form3->addRow(I18n::tr("res_rate"), rateRow);

    auto *benchRow = new QHBoxLayout();
    auto *bench = new QPushButton(I18n::tr("res_bench_run"), body);
    benchRow->addWidget(bench);
    auto *benchHint = new QLabel(I18n::tr("res_bench_hint"), body);
    benchHint->setStyleSheet("font-size:11px; color:palette(mid);");
    benchRow->addWidget(benchHint);
    benchRow->addStretch(1);
    form3->addRow(I18n::tr("res_bench"), benchRow);

    bv->addStretch(1);
    v->addWidget(body, 1);

    // ── フッタ ──────────────────────────────────────────────────────────────
    auto *foot = new QWidget(this);
    auto *h = new QHBoxLayout(foot);
    h->setContentsMargins(12, 8, 12, 8);
    h->addStretch(1);
    auto *cancel = new QPushButton(I18n::tr("res_cancel"), foot);
    auto *apply = new QPushButton(I18n::tr("res_apply"), foot);
    apply->setDefault(true);
    h->addWidget(cancel);
    h->addWidget(apply);
    v->addWidget(foot);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_processes, &QSlider::valueChanged, this,
            [this] { updateCores(); });
    connect(m_threads, &QSlider::valueChanged, this,
            [this] { updateCores(); });
    // ベンチマークはカーネルに測定 API が無いため無効表示 (RunDialog の一時停止と同じ扱い)
    bench->setEnabled(false);

    updateCores();
}

// プロセス×スレッド → 合計コア数の表示色とバッジ (mock の三項演算子を転記)
void ResourceDialog::updateCores()
{
    const int p = m_processes->value();
    const int t = m_threads->value();
    const int total = p * t;

    m_procVal->setText(QString::number(p));
    m_threadVal->setText(QString::number(t));

    const QString color = (total > kMachineCores) ? QStringLiteral("#B81818")
                        : (total == kMachineCores) ? QStringLiteral("#0078D4")
                                                   : QStringLiteral("palette(mid)");
    m_total->setStyleSheet(QStringLiteral(
        "font-size:14px; font-weight:600; color:%1;").arg(color));
    m_total->setText(QStringLiteral("%1 × %2 = %3").arg(p).arg(t).arg(total));

    // badge err / ok / warn (styles.css の .badge.* を最小限で再現)
    QString text, css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (total > kMachineCores) {
        text = I18n::tr("res_over");
        css += "background:#FBE5E5; color:#B81818;";
    } else if (total == kMachineCores) {
        text = I18n::tr("res_optimal");
        css += "background:#DFF6DD; color:#0F7B0F;";
    } else {
        text = I18n::tr("res_idle").arg(kMachineCores - total);
        css += "background:#FFF4CE; color:#9D5D00;";
    }
    m_badge->setStyleSheet(css);
    m_badge->setText(text);
}
