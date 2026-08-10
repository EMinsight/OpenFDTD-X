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
    ofd::I18n::reg("mon_add_row_tip",
                   "この行をクリックすると点モニターを 1 行追加します",
                   "Click this row to append a point monitor");
    ofd::I18n::reg("mon_del_row", "− 選択行を削除", "− Delete selected row");
    ofd::I18n::reg("mon_list_note",
                   "一覧はプロジェクトのモニター定義です (.ofdx に保存)。"
                   "名前・位置・周波数の各セルは編集できます。"
                   "モニター駆動の後処理は未実装のため、この定義は記録であり "
                   "カーネル入力 (.ofd) には出力されません。",
                   "The list holds this project's monitor definitions (saved in "
                   ".ofdx). Name / position / frequency cells are editable. "
                   "Monitor-driven post-processing is not implemented, so these "
                   "definitions are a record only and are not written to the "
                   "kernel input (.ofd).");
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

    // 音響/水中ドメイン用の差し替え名称 (E/H 場 → 音圧場)。
    // 電磁の「E/H 場」表記は音響では意味を持たないため rebuildDomain で切替える。
    ofd::I18n::reg("mon_ac_field_time", "時間領域 音圧場",
                   "Time-domain pressure field");
    ofd::I18n::reg("mon_ac_field_freq", "周波数領域 音圧場",
                   "Frequency-domain pressure field");
    ofd::I18n::reg("mon_ac_line", "Line — 音圧 freq", "Line — pressure freq");
    ofd::I18n::reg("mon_ac_volume", "Volume — 音圧 3D", "Volume — pressure 3D");
    ofd::I18n::reg("mon_ac_d_point", "音圧 時間応答 1点",
                   "Pressure time response at one point");
    ofd::I18n::reg("mon_ac_d_plane", "面上 周波数領域 音圧",
                   "Frequency-domain pressure on a plane");
    I18n::reg("mon_uw_sync", "同期・レコーダ・apodization の設定",
              "the synchronisation, recorder and apodization settings");
    I18n::reg("mon_uw_sync_ok", "サンプリング周波数 (音響設定へ反映されます)",
              "the sampling frequency (applied to the acoustic settings)");
    return true;
}();

// ── タイプ追加グリッド (ドメインでフィルタ) ─────────────────────────────────
enum { D_EM = 1, D_OPT = 2, D_AC = 4, D_UW = 8, D_ALL = 15 };
// id      : MonitorRow::type に保存する安定 ID (言語に依らない ASCII 語)
// shortNm : 一覧の「種類」列に出す短縮名 (ID と 1:1)
// acName / acDesc は音響/水中ドメインでの差し替えキー (nullptr = 共通のまま)
struct AddType { const char *id, *ic, *shortNm, *name, *desc; unsigned domains;
                 const char *acName = nullptr, *acDesc = nullptr; };

const AddType kAddTypes[] = {
    { "point",  "⊙",  "Point",    "mn_field_time",       "mon_d_point",    D_ALL,
            "mon_ac_field_time",   "mon_ac_d_point" },
    { "line",   "━",  "Line",     "Line — E/H freq",     "mon_d_line",     D_ALL,
            "mon_ac_line" },
    { "plane",  "▭",  "Plane",    "mn_field_freq",       "mon_d_plane",    D_ALL,
            "mon_ac_field_freq",   "mon_ac_d_plane" },
    { "volume", "▦",  "Volume",   "Volume — E/H 3D",     "mon_d_volume",   D_ALL,
            "mon_ac_volume" },
    { "mode",   "⊛",  "Mode",     "mn_mode_exp",         "mon_d_mode",     D_EM | D_OPT },
    { "movie",  "▶",  "Movie",    "mn_movie",            "mon_d_movie",    D_ALL },
    { "flux",   "≡",  "Flux",     "mn_flux",             "mon_d_flux",     D_EM | D_OPT },
    { "ntff",   "⌖",  "NTFF",     "mn_far_field",        "mon_d_ntff",     D_EM | D_OPT },
    { "index",  "⊚",  "Index",    "mn_index",            "mon_d_index",    D_EM | D_OPT },
    { "q",      "⌬",  "Q",        "mn_qanalysis",        "mon_d_q",        D_EM | D_OPT },
    { "global", "⌛",  "Time",     "Global time monitor", "mon_d_global",   D_ALL },
    // PML は水中音響 (BELLHOP は開境界を自前処理) には存在しないので出さない
    { "pml",    "⌟",  "PML",      "mn_pml",              "mon_d_pml",      D_EM | D_OPT | D_AC },
    // 音響専用
    { "spl",    "♪",  "SPL",      "SPL Meter",           "mon_d_spl",      D_AC | D_UW },
    { "irf",    "⏱",  "IRF",      "IRF (Impulse resp.)", "mon_d_irf",      D_AC | D_UW },
    { "binaural","🎤","Binaural", "Binaural Mic",        "mon_d_binaural", D_AC },
    { "micarray","📐","MicArray", "Mic Array",           "mon_d_micarray", D_AC | D_UW },
    { "tl",     "🌊", "TL",       "TL (Transmission Loss)", "mon_d_tl",    D_UW },
    { "beam",   "⚓", "Beam",     "Beampattern (Sonar)", "mon_d_beam",     D_UW },
    // EM専用
    { "vswr",   "📡", "VSWR",     "VSWR/Impedance",      "mon_d_vswr",     D_EM },
    { "spara",  "📊", "S-param",  "S-parameters",        "mon_d_spara",    D_EM | D_OPT },
    { "radeff", "⊕",  "RadEff",   "Radiation efficiency","mon_d_radeff",   D_EM },
    { "sar",    "⚡", "SAR",      "SAR (Specific AR)",   "mon_d_sar",      D_EM },
};

// タイプ ID → 定義 (手書きの .ofdx や将来の追加で未知 ID が来たら nullptr)
const AddType *findAddType(const QString &id)
{
    for (const AddType &t : kAddTypes)
        if (id == QLatin1String(t.id)) return &t;
    return nullptr;
}

// 一覧の「種類」列に出す表示名 (アイコン + 短縮名)。未知 ID は ID をそのまま。
QString kindLabel(const QString &id)
{
    const AddType *t = findAddType(id);
    if (!t) return id;
    return QString::fromUtf8(t->ic) + " " + QString::fromUtf8(t->shortNm);
}

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
    // 一覧は Project::monitors() のビュー (.ofdx へ保存される実データ)。
    // 後処理へ渡されないことだけは明示しておく (絶対規則 5)。
    auto *listNote = new QLabel(I18n::tr("mon_list_note"), sl);
    listNote->setWordWrap(true);
    listNote->setStyleSheet("color:#7A7A7A; font-size:11px;");
    sl->vbox()->addWidget(listNote);
    auto *listBtns = new QHBoxLayout();
    m_delRow = new QPushButton(I18n::tr("mon_del_row"), sl);
    listBtns->addWidget(m_delRow);
    listBtns->addStretch(1);
    sl->vbox()->addLayout(listBtns);
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
    m_settings->form()->addRow(tabhelp::unwiredNote(m_settings, I18n::tr("mon_uw_sync"), I18n::tr("mon_uw_sync_ok")));

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
    // 一覧の編集 (チェック / 名前 / 位置 / 周波数) → モデルへ書き戻す
    connect(m_list, &QTableWidget::itemChanged, this,
            [this] { applyList(); });
    // 末尾の「＋ モニターを追加…」行のクリックで 1 行追加
    connect(m_list, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row == m_p->monitors().size()) addMonitor(QStringLiteral("point"));
    });
    connect(m_delRow, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        QVector<MonitorRow> &mons = m_p->monitors();
        if (row < 0 || row >= mons.size()) return;
        mons.remove(row);
        m_p->touch();
        rebuildList();
    });
    connect(project, &Project::domainChanged, this,
            [this] { rebuildDomain(); });
    connect(project, &Project::loaded, this, &MonitorsTab::refresh);
    refresh();
}

// モニターを 1 行追加する (タイプ追加グリッド / 一覧末尾の追加行から)
void MonitorsTab::addMonitor(const QString &typeId)
{
    QVector<MonitorRow> &mons = m_p->monitors();
    const AddType *t = findAddType(typeId);
    const QString stem = t ? QString::fromUtf8(t->shortNm).toLower() : typeId;
    // 同名を作らないよう連番を振る
    int n = 1;
    QString name;
    bool dup = true;
    while (dup) {
        name = QStringLiteral("%1_%2").arg(stem).arg(n++);
        dup = false;
        for (const MonitorRow &r : mons)
            if (r.name == name) { dup = true; break; }
    }
    MonitorRow r;
    r.type = typeId;
    r.name = name;
    r.region = QString::fromUtf8("—");   // 位置は利用者が入力する
    r.band = QString::fromUtf8("—");
    mons.push_back(r);
    m_p->touch();
    rebuildList();
    m_list->setCurrentCell(mons.size() - 1, 4);
}

// 一覧 (ウィジェット) → モデル。種類列は読み取り専用なので type は触らない。
void MonitorsTab::applyList()
{
    if (m_updating) return;
    QVector<MonitorRow> &mons = m_p->monitors();
    for (int i = 0; i < mons.size() && i < m_list->rowCount(); ++i) {
        auto cell = [this, i](int c) {
            QTableWidgetItem *it = m_list->item(i, c);
            return it ? it->text() : QString();
        };
        if (QTableWidgetItem *ck = m_list->item(i, 0))
            mons[i].enabled = (ck->checkState() == Qt::Checked);
        mons[i].name   = cell(3);
        mons[i].region = cell(4);
        mons[i].band   = cell(5);
    }
    m_p->touch();
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

// ── 一覧表 (モデル → ウィジェット) ─────────────────────────────────────────
void MonitorsTab::rebuildList()
{
    const Domain d = m_p->activeDomain();
    const bool isAcoustic = (d == Domain::Acoustic || d == Domain::Underwater);
    const QVector<MonitorRow> &mons = m_p->monitors();
    const int n = mons.size();

    m_updating = true;
    m_list->clearSpans();
    m_list->setRowCount(n + 1);
    m_list->horizontalHeaderItem(5)->setText(
        I18n::tr(isAcoustic ? "mon_col_band" : "mon_col_freq"));
    auto readOnly = [](const QString &text) {
        auto *it = new QTableWidgetItem(text);
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        return it;
    };
    for (int i = 0; i < n; ++i) {
        const MonitorRow &r = mons[i];
        auto *ck = new QTableWidgetItem;
        ck->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_list->setItem(i, 0, ck);
        m_list->setItem(i, 1, readOnly(QString::number(i + 1)));
        m_list->setItem(i, 2, readOnly(kindLabel(r.type)));   // 種類は変更不可
        m_list->setItem(i, 3, new QTableWidgetItem(r.name));
        m_list->setItem(i, 4, new QTableWidgetItem(r.region));
        m_list->setItem(i, 5, new QTableWidgetItem(r.band));
    }
    // ＋ モニターを追加… 行 (クリックで 1 行追加)
    auto *addCk = new QTableWidgetItem;
    addCk->setFlags(Qt::ItemIsEnabled);
    m_list->setItem(n, 0, addCk);
    auto *addIt = new QTableWidgetItem(I18n::tr("mon_add_row"));
    addIt->setFlags(addIt->flags() & ~Qt::ItemIsEditable);
    addIt->setToolTip(I18n::tr("mon_add_row_tip"));
    QFont f = addIt->font();
    f.setItalic(true);
    addIt->setFont(f);
    m_list->setItem(n, 1, addIt);
    m_list->setSpan(n, 1, 1, 5);
    m_updating = false;
}

void MonitorsTab::rebuildDomain()
{
    const Domain d = m_p->activeDomain();
    const bool isAcoustic = (d == Domain::Acoustic || d == Domain::Underwater);
    const bool isEmOpt = (d == Domain::EM || d == Domain::Optical);

    // まだ編集されていない (どれかのドメインの既定そのままの) 一覧なら、
    // 新しいドメインの既定へ差し替える。編集済みの一覧はそのまま残す。
    QVector<MonitorRow> &mons = m_p->monitors();
    if (isDefaultMonitorSet(mons) && mons != defaultMonitors(d))
        mons = defaultMonitors(d);
    rebuildList();

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
        // 音響/水中では「E/H 場」の名称・説明を「音圧場」系へ差し替える
        const char *nameKey = (isAcoustic && t.acName) ? t.acName : t.name;
        const char *descKey = (isAcoustic && t.acDesc) ? t.acDesc : t.desc;
        auto *b = new QPushButton(
            QString::fromUtf8(t.ic) + " " + I18n::tr(QString::fromUtf8(nameKey))
            + "\n" + I18n::tr(QString::fromUtf8(descKey)), m_addHost);
        b->setStyleSheet("text-align:left; padding:4px 8px;");
        b->setMinimumHeight(36);
        // クリックでそのタイプのモニターを一覧へ追加する (.ofdx へ保存)
        const QString typeId = QString::fromUtf8(t.id);
        connect(b, &QPushButton::clicked, this,
                [this, typeId] { addMonitor(typeId); });
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
