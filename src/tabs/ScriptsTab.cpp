// ScriptsTab.cpp
#include "ScriptsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 scr_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("scr_title_fmt", "スクリプトエディタ (%1サンプル)",
              "Script editor (%1 sample)");
    I18n::reg("scr_python", "Python", "Python");
    I18n::reg("scr_lsf", "Script (LSF風)", "Script (LSF-style)");
    I18n::reg("scr_load", "📁 読込", "📁 Load");
    I18n::reg("scr_save", "💾 保存", "💾 Save");
    I18n::reg("scr_examples", "📚 サンプル", "📚 Examples");
    I18n::reg("scr_run", "▶ 実行", "▶ Run");
    I18n::reg("scr_abort", "⏸ 中断", "⏸ Abort");
    I18n::reg("scr_status", "行 12 列 8 · UTF-8", "Ln 12 Col 8 · UTF-8");
    I18n::reg("scr_console", "コンソール", "Console");
    I18n::reg("scr_api_title", "API早見 (ドメイン別)", "Domain-aware API");
    I18n::reg("scr_c_func", "関数", "Function");
    I18n::reg("scr_c_desc", "説明", "Description");
    return true;
}();

// ── mock の samples[lang][domain] をそのまま転記 ────────────────────────────
const char *kPyEm = R"CODE(# OpenFDTD-X Python API — EM example (patch antenna)
import openfdtd_x as ofd

sim = ofd.Simulation.from_active()
sim.set("patch_length", 30e-3)
sim.set("feed_offset",   5e-3)
sim.run()

S11 = sim.monitor("input_port").s_parameter()
bw = S11.bandwidth(threshold_dB=-10)
print(f"Bandwidth: {bw[0]/1e9:.2f}~{bw[1]/1e9:.2f} GHz")

# Sweep patch length
for L in np.linspace(25e-3, 35e-3, 11):
    sim.set("patch_length", L)
    sim.run()
    sim.export_touchstone(f"sweep_L{L*1e3:.1f}mm.s1p")
)CODE";

const char *kPyOptical = R"CODE(# OpenFDTD-X Python API — Optical example (ring resonator)
import openfdtd_x as ofd

sim = ofd.Simulation.from_active()
sim.set("ring_radius",   5.2e-6)
sim.set("coupling_gap",  180e-9)
sim.run()

T_drop = sim.monitor("T_drop").transmission()
print(f"Peak T = {T_drop.max():.4f} @ {T_drop.argmax_lambda()*1e9:.2f} nm")

# Export to tidy3d for high-resolution validation
td_sim = sim.to_tidy3d(resolution="high")
job = td_sim.submit()
print(f"Submitted to tidy3d: {job.id}")
)CODE";

const char *kPyAcoustic = R"CODE(# OpenFDTD-X Python API — Room acoustics example
import openfdtd_x as ofd
import numpy as np

sim = ofd.Simulation.from_active()
sim.set("absorber_thick", 50e-3)
sim.run()

# Get impulse response at receiver P1
irf = sim.monitor("P1_center").impulse_response()
rt60 = irf.rt60_octave()
c80  = irf.clarity_c80()
print(f"RT60 @ 1kHz: {rt60[1000]:.2f} s,  C80: {c80[1000]:+.1f} dB")

# Auralization
dry = ofd.load_wav("anechoic_speech.wav")
wet = ofd.convolve(dry, irf, binaural=True, hrtf="kemar")
ofd.save_wav("auralized.wav", wet, samplerate=48000)
)CODE";

const char *kPyUnderwater = R"CODE(# OpenFDTD-X Python API — Underwater example (SOFAR)
import openfdtd_x as ofd

sim = ofd.Simulation.from_active()
sim.set("sonar_depth", 50)
sim.set("beam_angle",  15)
sim.set_solver("bellhop")  # gaussian beam ray tracing
sim.run()

TL = sim.monitor("TL_range").transmission_loss()
print(f"TL at 50 km: {TL.at(range_km=50):.1f} dB")

# Eigenrays for echo prediction
rays = sim.monitor("eigenrays").rays()
for r in rays.top_n(5):
    print(f"  τ={r.delay_ms:.1f}ms  Δθ={r.angle_deg:+.1f}°  E={r.energy_dB:+.1f}dB")
)CODE";

const char *kLsfEm = R"CODE(## OpenFDTD-X LSF — EM antenna sweep
setnamed("patch", "length", 30e-3);
setnamed("feed",  "offset", 5e-3);
run;

S11 = getresult("input_port", "S");
?"Center freq: " + num2str(getresult("input_port","f0")/1e9) + " GHz";
)CODE";

const char *kLsfOptical = R"CODE(## OpenFDTD-X LSF — Optical ring resonator
switchtolayout;
setnamed("ring",    "radius", 5.2e-6);
setnamed("coupler", "gap",    180e-9);
run;

T = getresult("T_drop", "T");
?T;
)CODE";

const char *kLsfAcoustic = R"CODE(## OpenFDTD-X LSF — Room acoustic RT60
setnamed("absorber", "thickness", 50e-3);
run;

irf = getresult("P1_center", "impulse_response");
?"RT60 = " + num2str(rt60(irf,1000)) + " s";
)CODE";

const char *kLsfUnderwater = R"CODE(## OpenFDTD-X LSF — Underwater Bellhop run
setnamed("sonar", "depth", 50);
setnamed("sonar", "angle", 15);
setsolver("bellhop");
run;

TL = getresult("TL_range", "TL");
?"TL @ 50km = " + num2str(TL_at_range(TL,50e3)) + " dB";
)CODE";

const char *sampleCode(const QString &lang, ofd::Domain d)
{
    const bool py = (lang == "python");
    switch (d) {
        case ofd::Domain::Optical:    return py ? kPyOptical    : kLsfOptical;
        case ofd::Domain::Acoustic:   return py ? kPyAcoustic   : kLsfAcoustic;
        case ofd::Domain::Underwater: return py ? kPyUnderwater : kLsfUnderwater;
        default:                      return py ? kPyEm         : kLsfEm;
    }
}

// コンソール最終行 (mock のドメイン別出力)
const char *consoleResult(ofd::Domain d)
{
    switch (d) {
        case ofd::Domain::Optical:    return "Peak T = 0.8472 @ 1551.83 nm";
        case ofd::Domain::Acoustic:   return "RT60 @ 1kHz: 1.42 s,  C80: +1.8 dB";
        case ofd::Domain::Underwater: return "TL at 50 km: -82.4 dB";
        default:                      return "Bandwidth: 2.41~2.49 GHz";
    }
}

// API 早見表 (mock の表をそのまま転記)
struct ApiRow { const char *func, *desc; };
const ApiRow kApiHead[2] = {
    { "sim.set(key, val)", "パラメータ設定" },
    { "sim.run()",         "FDTD実行" },
};
const ApiRow kApiEm[3] = {
    { "monitor.s_parameter()",         "Sパラメータ抽出" },
    { "monitor.radiation_pattern()",   "放射パターン" },
    { "sim.export_touchstone(path)",   ".s2p書出し" },
};
const ApiRow kApiOptical[3] = {
    { "monitor.transmission()",        "透過率スペクトル T(λ)" },
    { "monitor.mode_overlap()",        "モード結合効率" },
    { "sim.to_tidy3d()",               "tidy3d変換" },
};
const ApiRow kApiAcoustic[3] = {
    { "monitor.impulse_response()",    "インパルス応答" },
    { "irf.rt60_octave()",             "RT60 (1/3オクターブ)" },
    { "ofd.convolve(...binaural=True)", "HRTF畳み込み" },
};
const ApiRow kApiUnderwater[3] = {
    { "monitor.transmission_loss()",   "TL(range)" },
    { "monitor.eigenrays()",           "固有線抽出" },
    { "sim.set_solver(\"bellhop\")",   "Bellhopソルバ切替" },
};
const ApiRow kApiTail[2] = {
    { "sim.save(\"file.ofd\")",         "プロジェクト保存" },
    { "sim.export_hdf5(\"results.h5\")", "HDF5書出し" },
};
} // namespace

// ── ScriptsTab ──────────────────────────────────────────────────────────────
ScriptsTab::ScriptsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // ── スクリプトエディタ ─────────────────────────────────────────────────
    m_editorSec = new SectionBox(QString(), body);
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    auto addLangBtn = [this, topRow](const char *key, const char *lang,
                                     QWidget *owner) {
        auto *b = new QPushButton(I18n::tr(key), owner);
        b->setCheckable(true);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        b->setProperty("lang", QString::fromUtf8(lang));
        const QString l = QString::fromUtf8(lang);
        connect(b, &QPushButton::clicked, this, [this, l] { setLang(l); });
        topRow->addWidget(b);
        m_langBtns.push_back(b);
    };
    addLangBtn("scr_python", "python", m_editorSec);
    addLangBtn("scr_lsf", "lsf", m_editorSec);
    topRow->addStretch(1);
    for (const char *key : { "scr_load", "scr_save", "scr_examples" }) {
        auto *b = new QPushButton(I18n::tr(key), m_editorSec);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        topRow->addWidget(b);
    }
    m_editorSec->vbox()->addLayout(topRow);

    m_editor = new QPlainTextEdit(m_editorSec);
    m_editor->setFont(mono);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setMinimumHeight(280);
    m_editor->setStyleSheet("background:#0E1116; color:#DDE2E8;"
                            "border:1px solid palette(dark); font-size:11px;");
    m_editorSec->vbox()->addWidget(m_editor);

    auto *runRow = new QHBoxLayout();
    auto *runBtn = new QPushButton(I18n::tr("scr_run"), m_editorSec);
    runBtn->setStyleSheet("font-weight:600;");
    runRow->addWidget(runBtn);
    runRow->addWidget(new QPushButton(I18n::tr("scr_abort"), m_editorSec));
    runRow->addStretch(1);
    runRow->addWidget(new QLabel(I18n::tr("scr_status"), m_editorSec));
    m_editorSec->vbox()->addLayout(runRow);
    v->addWidget(m_editorSec);

    // ── コンソール ─────────────────────────────────────────────────────────
    auto *sCon = new SectionBox(I18n::tr("scr_console"), body);
    m_console = new QPlainTextEdit(sCon);
    m_console->setReadOnly(true);
    m_console->setFont(mono);
    m_console->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_console->setMinimumHeight(140);
    m_console->setStyleSheet("background:#0E1116; color:#DDE2E8;"
                             "border:1px solid palette(dark); font-size:11px;");
    sCon->vbox()->addWidget(m_console);
    v->addWidget(sCon);

    // ── API 早見表 ─────────────────────────────────────────────────────────
    auto *sApi = new SectionBox(I18n::tr("scr_api_title"), body);
    m_api = new QTableWidget(0, 2, sApi);
    m_api->setHorizontalHeaderLabels({ I18n::tr("scr_c_func"),
                                       I18n::tr("scr_c_desc") });
    m_api->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_api->verticalHeader()->setVisible(false);
    m_api->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_api->setMinimumHeight(230);
    sApi->vbox()->addWidget(m_api);
    v->addWidget(sApi);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &ScriptsTab::rebuild);
    rebuild();
}

void ScriptsTab::setLang(const QString &lang)
{
    m_lang = lang;
    rebuild();
}

void ScriptsTab::rebuild()
{
    const Domain d = m_p->activeDomain();
    for (QPushButton *b : m_langBtns)
        b->setChecked(b->property("lang").toString() == m_lang);

    m_editorSec->setTitle(I18n::tr("scr_title_fmt").arg(domainKey(d).toUpper()));
    m_editor->setPlainText(QString::fromUtf8(sampleCode(m_lang, d)));

    // ── コンソール (mock の固定ログ + ドメイン別結果) ───────────────────────
    m_console->clear();
    auto line = [this](const QString &text, const char *color) {
        m_console->appendHtml(QStringLiteral(
            "<pre style=\"margin:0; color:%1;\">%2</pre>").arg(
            QString::fromUtf8(color), text.toHtmlEscaped()));
    };
    line(">>> sim.run()", "#4FA3E3");
    line("Loading project... ✓", "#DDE2E8");
    line("Iterating 1000 steps...", "#DDE2E8");
    line("[100%] converged at step 624 (Δ = 8.2e-04)", "#5FD68B");
    line(">>> result", "#4FA3E3");
    line(QString::fromUtf8(consoleResult(d)), "#DDE2E8");

    // ── API 早見表 ─────────────────────────────────────────────────────────
    const ApiRow *mid = kApiEm;
    switch (d) {
        case Domain::Optical:    mid = kApiOptical;    break;
        case Domain::Acoustic:   mid = kApiAcoustic;   break;
        case Domain::Underwater: mid = kApiUnderwater; break;
        default:                 mid = kApiEm;         break;
    }
    m_api->setRowCount(7);
    int r = 0;
    auto addRow = [this, &r](const ApiRow &a) {
        auto *f = new QTableWidgetItem(QString::fromUtf8(a.func));
        f->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_api->setItem(r, 0, f);
        m_api->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(a.desc)));
        ++r;
    };
    for (const ApiRow &a : kApiHead) addRow(a);
    for (int i = 0; i < 3; ++i)      addRow(mid[i]);
    for (const ApiRow &a : kApiTail) addRow(a);
}
