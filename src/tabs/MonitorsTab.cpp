// MonitorsTab.cpp
#include "MonitorsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有語彙 (mon_) + i18n.js 由来の共有キー (mn_) — file-local 登録 ───
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("mon_list", "モニター一覧", "Monitors");
    ofd::I18n::reg("mon_col_kind", "種類", "Type");
    ofd::I18n::reg("mon_col_name", "名前", "Name");
    ofd::I18n::reg("mon_col_pos", "位置・範囲", "Position / range");
    ofd::I18n::reg("mon_col_freq", "周波数/波長", "Frequency / wavelength");
    ofd::I18n::reg("mon_col_band", "周波数帯", "Frequency band");
    ofd::I18n::reg("mon_add_row", "＋ モニターを追加…", "+ Add monitor…");
    ofd::I18n::reg("mon_add_title", "モニタータイプ追加", "Add monitor");
    ofd::I18n::reg("mon_add_suffix", "(%1種 · %2ドメイン)",
                   "(%1 types · %2 domain)");
    ofd::I18n::reg("mon_settings", "モニター設定", "Settings");
    ofd::I18n::reg("mon_sync", "座標範囲を波源領域に同期 (同期は未実装)",
                   "Sync extents to source region (sync not implemented)");
    ofd::I18n::reg("mon_auto", "自動", "Auto");
    ofd::I18n::reg("mon_recorders", "記録レコーダ", "Recorders");
    ofd::I18n::reg("mon_phase", "位相", "Phase");
    ofd::I18n::reg("mon_amp", "振幅", "Amplitude");
    ofd::I18n::reg("mon_apod_start", "開始時", "Start");
    ofd::I18n::reg("mon_apod_end", "終了時", "End");
    ofd::I18n::reg("mon_apod_full", "両端", "Both");
    ofd::I18n::reg("mon_apod_hint", "時間窓関数で過渡を除去",
                   "Time-window apodization removes transients");
    ofd::I18n::reg("mon_srate", "サンプリング周波数", "Sampling frequency");
    ofd::I18n::reg("mon_srate_unit", "Hz (WAV出力用)", "Hz (for WAV output)");

    // モニタータイプ名 (i18n.js の mn_* — 既存キーがあればそちらが優先される)
    ofd::I18n::reg("mn_field_time", "時間領域 E/H場", "Time-domain E/H field");
    ofd::I18n::reg("mn_field_freq", "周波数領域 E/H場",
                   "Frequency-domain E/H field");
    ofd::I18n::reg("mn_mode_exp", "モード展開", "Mode expansion");
    ofd::I18n::reg("mn_movie", "動画モニター", "Movie monitor");
    ofd::I18n::reg("mn_flux", "電力モニター (Flux)", "Power monitor (Flux)");
    ofd::I18n::reg("mn_far_field", "遠方界 (NTFF)", "Far field (NTFF)");
    ofd::I18n::reg("mn_index", "屈折率モニター", "Index monitor");
    ofd::I18n::reg("mn_qanalysis", "Q値解析", "Q-factor analysis");
    ofd::I18n::reg("mn_pml", "PML吸収量", "PML absorption");

    // タイプ追加ボタンの説明文 (mon_d_)
    ofd::I18n::reg("mon_d_point", "E/H 時間応答 1点",
                   "E/H time response at one point");
    ofd::I18n::reg("mon_d_line", "線上 周波数領域",
                   "Frequency domain along a line");
    ofd::I18n::reg("mon_d_plane", "面上 周波数領域 E/H",
                   "Frequency-domain E/H on a plane");
    ofd::I18n::reg("mon_d_volume", "体積 周波数領域",
                   "Frequency domain in a volume");
    ofd::I18n::reg("mon_d_mode", "モード展開→Sパラメータ",
                   "Mode expansion → S-parameters");
    ofd::I18n::reg("mon_d_movie", "動画 (時間スキャン)", "Movie (time scan)");
    ofd::I18n::reg("mon_d_flux", "電力流速 Poynting", "Poynting power flux");
    ofd::I18n::reg("mon_d_ntff", "遠方界 NTFF (RCS/放射)",
                   "Far field NTFF (RCS/radiation)");
    ofd::I18n::reg("mon_d_index", "屈折率/εr 確認", "Refractive index / εr check");
    ofd::I18n::reg("mon_d_q", "Q値 (時間+空間)", "Q factor (time + space)");
    ofd::I18n::reg("mon_d_global", "全領域時間トレース",
                   "Whole-domain time trace");
    ofd::I18n::reg("mon_d_pml", "PML吸収診断", "PML absorption diagnostics");
    ofd::I18n::reg("mon_d_spl", "音圧レベル (dB SPL)",
                   "Sound pressure level (dB SPL)");
    ofd::I18n::reg("mon_d_irf", "インパルス応答 + RT60",
                   "Impulse response + RT60");
    ofd::I18n::reg("mon_d_binaural", "バイノーラル受音(HRTF)",
                   "Binaural receiver (HRTF)");
    ofd::I18n::reg("mon_d_micarray", "複数受音点アレイ", "Multi-receiver array");
    ofd::I18n::reg("mon_d_tl", "伝搬損失 vs 距離", "Transmission loss vs range");
    ofd::I18n::reg("mon_d_beam", "ソナー指向性パターン", "Sonar beam pattern");
    ofd::I18n::reg("mon_d_vswr", "給電点 Z(f), VSWR", "Feed-point Z(f), VSWR");
    ofd::I18n::reg("mon_d_spara", "Sパラ抽出 (.s2p)",
                   "S-parameter extraction (.s2p)");
    ofd::I18n::reg("mon_d_radeff", "放射効率", "Radiation efficiency");
    ofd::I18n::reg("mon_d_sar", "人体局所SAR", "Local SAR in tissue");
    return true;
}();

// ── ドメイン別の既定モニター行 (モックの値をそのまま) ───────────────────────
struct MonRow { bool ck; const char *name, *kind, *pos, *freq; };

const MonRow kOpticalRows[] = {
    { true,  "T_drop",      "▭ Plane",  "Z=2.5μm 面",           "1500~1600 nm (201pt)" },
    { true,  "thru_mode",   "⊛ Mode",   "X=10μm, TE₀",          "1550 nm" },
    { true,  "E_field_3D",  "▦ Volume", "[-2,2]³ μm",           "1550 nm" },
    { false, "propagation", "▶ Movie",  "Y=0 面",               "時間 0~50fs" },
    { false, "far_field",   "⌖ NTFF",   "θ:-90~90° φ:0~360°",   "1550 nm" },
};
const MonRow kAcousticRows[] = {
    { true,  "P1_center",    "⊙ Point", "(0, 1.2, 8)",  "63Hz~16kHz" },
    { true,  "mic_array",    "━ Line",  "Y=1.2m 線",    "全帯域" },
    { true,  "IRF_response", "⌛ Time",  "P1, P2, P3",   "時間 0~3s" },
    { false, "SPL_floor",    "▭ Plane", "Y=1.2m 面",    "1kHz" },
};
const MonRow kEmRows[] = {
    { true,  "E_probe",   "⊙ Point", "(0.02, 0, 0.001)", "2.5 GHz" },
    { true,  "impedance", "⌛ Time",  "feed #1",          "2~3 GHz" },
    { true,  "E_surface", "▭ Plane", "Z=1mm 面",         "2.5 GHz" },
    { false, "far_field", "⌖ NTFF",  "全方向",           "2.5 GHz" },
};

// ── タイプ追加グリッド (ドメインでフィルタ) ─────────────────────────────────
enum { D_EM = 1, D_OPT = 2, D_AC = 4, D_UW = 8, D_ALL = 15 };
struct AddType { const char *ic, *name, *desc; unsigned domains; };

const AddType kAddTypes[] = {
    { "⊙",  "mn_field_time",       "mon_d_point",    D_ALL },
    { "━",  "Line — E/H freq",     "mon_d_line",     D_ALL },
    { "▭",  "mn_field_freq",       "mon_d_plane",    D_ALL },
    { "▦",  "Volume — E/H 3D",     "mon_d_volume",   D_ALL },
    { "⊛",  "mn_mode_exp",         "mon_d_mode",     D_EM | D_OPT },
    { "▶",  "mn_movie",            "mon_d_movie",    D_ALL },
    { "≡",  "mn_flux",             "mon_d_flux",     D_EM | D_OPT },
    { "⌖",  "mn_far_field",        "mon_d_ntff",     D_EM | D_OPT },
    { "⊚",  "mn_index",            "mon_d_index",    D_EM | D_OPT },
    { "⌬",  "mn_qanalysis",        "mon_d_q",        D_EM | D_OPT },
    { "⌛",  "Global time monitor", "mon_d_global",   D_ALL },
    { "⌟",  "mn_pml",              "mon_d_pml",      D_ALL },
    // 音響専用
    { "♪",  "SPL Meter",           "mon_d_spl",      D_AC | D_UW },
    { "⏱",  "IRF (Impulse resp.)", "mon_d_irf",      D_AC | D_UW },
    { "🎤", "Binaural Mic",        "mon_d_binaural", D_AC },
    { "📐", "Mic Array",           "mon_d_micarray", D_AC | D_UW },
    { "🌊", "TL (Transmission Loss)", "mon_d_tl",    D_UW },
    { "⚓", "Beampattern (Sonar)", "mon_d_beam",     D_UW },
    // EM専用
    { "📡", "VSWR/Impedance",      "mon_d_vswr",     D_EM },
    { "📊", "S-parameters",        "mon_d_spara",    D_EM | D_OPT },
    { "⊕",  "Radiation efficiency","mon_d_radeff",   D_EM },
    { "⚡", "SAR (Specific AR)",   "mon_d_sar",      D_EM },
};

unsigned domainBit(Domain d)
{
    switch (d) {
        case Domain::EM:         return D_EM;
        case Domain::Optical:    return D_OPT;
        case Domain::Acoustic:   return D_AC;
        case Domain::Underwater: return D_UW;
    }
    return D_EM;
}
} // namespace

MonitorsTab::MonitorsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── モニター一覧 / Monitors ─────────────────────────────────────────────
    auto *sl = new SectionBox(I18n::tr("mon_list"), body);
    m_list = new QTableWidget(0, 6, sl);
    m_list->setHorizontalHeaderLabels({ "", "#", I18n::tr("mon_col_kind"),
                                        I18n::tr("mon_col_name"),
                                        I18n::tr("mon_col_pos"),
                                        I18n::tr("mon_col_freq") });
    m_list->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_list->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_list->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_list->verticalHeader()->setVisible(false);
    m_list->setMinimumHeight(180);
    sl->vbox()->addWidget(m_list);
    // 一覧はドメイン別の固定サンプル行 (Project には保存されない)
    sl->vbox()->addWidget(tabhelp::sampleNote(sl));
    v->addWidget(sl);

    // ── モニタータイプ追加 / Add monitor ────────────────────────────────────
    m_addSection = new SectionBox(I18n::tr("mon_add_title"), body);
    m_addHost = new QWidget(m_addSection);
    m_addSection->vbox()->addWidget(m_addHost);
    v->addWidget(m_addSection);

    // ── モニター設定 / Settings ─────────────────────────────────────────────
    m_settings = new SectionBox(I18n::tr("mon_settings"), body);
    m_syncAuto = new QCheckBox(I18n::tr("mon_auto"), m_settings);
    m_syncAuto->setChecked(true);
    m_settings->form()->addRow(I18n::tr("mon_sync"), m_syncAuto);

    auto *rec = new QHBoxLayout();
    m_recPhase = new QCheckBox(I18n::tr("mon_phase"), m_settings);
    m_recPhase->setChecked(true);
    m_recAmp = new QCheckBox(I18n::tr("mon_amp"), m_settings);
    m_recAmp->setChecked(true);
    m_recDft = new QCheckBox("DFT", m_settings);
    rec->addWidget(m_recPhase);
    rec->addWidget(m_recAmp);
    rec->addWidget(m_recDft);
    rec->addStretch(1);
    m_settings->form()->addRow(I18n::tr("mon_recorders"), rec);

    // apodization 行 (EM / 光のみ)
    m_apodRow = new QWidget(m_settings);
    auto *ah = new QHBoxLayout(m_apodRow);
    ah->setContentsMargins(0, 0, 0, 0);
    m_apod = new QComboBox(m_apodRow);
    m_apod->addItem("OFF");
    m_apod->addItem(I18n::tr("mon_apod_start"));
    m_apod->addItem(I18n::tr("mon_apod_end"));
    m_apod->addItem(I18n::tr("mon_apod_full"));
    ah->addWidget(m_apod);
    ah->addWidget(new QLabel(I18n::tr("mon_apod_hint"), m_apodRow));
    ah->addStretch(1);
    m_settings->form()->addRow("apodization", m_apodRow);

    // 同期/レコーダ/apodization はどこにも読まれない
    // (サンプリング周波数のみ AcousticOpts へ反映される)
    m_settings->form()->addRow(tabhelp::unwiredNote(m_settings));

    // サンプリング周波数 行 (音響 / 水中のみ) — AcousticOpts::sampleRate
    m_srateRow = new QWidget(m_settings);
    auto *sh = new QHBoxLayout(m_srateRow);
    sh->setContentsMargins(0, 0, 0, 0);
    m_srate = new QLineEdit("48000", m_srateRow);
    m_srate->setMaximumWidth(100);
    sh->addWidget(m_srate);
    sh->addWidget(new QLabel(I18n::tr("mon_srate_unit"), m_srateRow));
    sh->addStretch(1);
    m_settings->form()->addRow(I18n::tr("mon_srate"), m_srateRow);
    v->addWidget(m_settings);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_srate, &QLineEdit::editingFinished, this, [this] { apply(); });
    connect(project, &Project::domainChanged, this,
            [this] { rebuildDomain(); });
    connect(project, &Project::loaded, this, &MonitorsTab::refresh);
    refresh();
}

// サンプリング周波数のみ Project (AcousticOpts) に対応するので永続化
void MonitorsTab::apply()
{
    if (m_updating) return;
    const int sr = m_srate->text().toInt();
    if (sr > 0) {
        m_p->acoustic().sampleRate = sr;
        m_p->touch();
    }
}

void MonitorsTab::refresh()
{
    m_updating = true;
    m_srate->setText(QString::number(m_p->acoustic().sampleRate));
    m_updating = false;
    rebuildDomain();
}

void MonitorsTab::rebuildDomain()
{
    const Domain d = m_p->activeDomain();
    const bool isAcoustic = (d == Domain::Acoustic || d == Domain::Underwater);
    const bool isEmOpt = (d == Domain::EM || d == Domain::Optical);

    // ── 一覧表 ──────────────────────────────────────────────────────────────
    const MonRow *rows;
    int n;
    if (d == Domain::Optical)  { rows = kOpticalRows;  n = 5; }
    else if (isAcoustic)       { rows = kAcousticRows; n = 4; }
    else                       { rows = kEmRows;       n = 4; }

    m_updating = true;
    m_list->clearSpans();
    m_list->setRowCount(n + 1);
    m_list->horizontalHeaderItem(5)->setText(
        I18n::tr(isAcoustic ? "mon_col_band" : "mon_col_freq"));
    auto plain = [](const char *text) {
        auto *it = new QTableWidgetItem(QString::fromUtf8(text));
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        return it;
    };
    for (int i = 0; i < n; ++i) {
        const MonRow &r = rows[i];
        auto *ck = new QTableWidgetItem;
        ck->setCheckState(r.ck ? Qt::Checked : Qt::Unchecked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_list->setItem(i, 0, ck);
        auto *num = plain("");
        num->setText(QString::number(i + 1));
        m_list->setItem(i, 1, num);
        m_list->setItem(i, 2, plain(r.kind));
        m_list->setItem(i, 3, new QTableWidgetItem(QString::fromUtf8(r.name)));
        m_list->setItem(i, 4, plain(r.pos));
        m_list->setItem(i, 5, plain(r.freq));
    }
    // ＋ モニターを追加… 行
    auto *addCk = new QTableWidgetItem;
    addCk->setCheckState(Qt::Unchecked);
    addCk->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    m_list->setItem(n, 0, addCk);
    auto *addIt = new QTableWidgetItem(I18n::tr("mon_add_row"));
    addIt->setFlags(addIt->flags() & ~Qt::ItemIsEditable);
    QFont f = addIt->font();
    f.setItalic(true);
    addIt->setFont(f);
    m_list->setItem(n, 1, addIt);
    m_list->setSpan(n, 1, 1, 5);
    m_updating = false;

    // ── タイプ追加グリッド ──────────────────────────────────────────────────
    if (m_addHost->layout()) {
        QLayoutItem *it;
        while ((it = m_addHost->layout()->takeAt(0)) != nullptr) {
            delete it->widget();
            delete it;
        }
        delete m_addHost->layout();
    }
    auto *grid = new QGridLayout(m_addHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    const unsigned bit = domainBit(d);
    int idx = 0;
    for (const AddType &t : kAddTypes) {
        if (!(t.domains & bit)) continue;
        auto *b = new QPushButton(
            QString::fromUtf8(t.ic) + " " + I18n::tr(QString::fromUtf8(t.name))
            + "\n" + I18n::tr(QString::fromUtf8(t.desc)), m_addHost);
        b->setStyleSheet("text-align:left; padding:4px 8px;");
        b->setMinimumHeight(36);
        tabhelp::markNotImplemented(b);   // モニター追加は未配線
        grid->addWidget(b, idx / 2, idx % 2);
        ++idx;
    }
    m_addSection->setTitle(I18n::tr("mon_add_title") + " "
        + I18n::tr("mon_add_suffix").arg(idx).arg(domainKey(d).toUpper()));

    // ── 設定行の表示切替 ────────────────────────────────────────────────────
    auto setRowVisible = [this](QWidget *field, bool on) {
        field->setVisible(on);
        if (QWidget *lab = m_settings->form()->labelForField(field))
            lab->setVisible(on);
    };
    setRowVisible(m_apodRow, isEmOpt);
    setRowVisible(m_srateRow, isAcoustic);
}
