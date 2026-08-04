// AcousticTab.cpp
#include "AcousticTab.h"
#include "../core/Project.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有の翻訳キー (ac2_) — file-local 登録 (既存 ac_ は I18n.cpp) ───────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // ソルバー / Solver
    I18n::reg("ac2_solver_section", "ソルバー", "Solver");
    I18n::reg("ac2_sv_fdtd", "FDTD (波動)", "FDTD (wave)");
    I18n::reg("ac2_sv_ray", "Raycast (幾何)", "Raycast (geom.)");
    I18n::reg("ac2_sv_ism", "Image-Source", "Image-Source");
    I18n::reg("ac2_sv_hybrid", "ハイブリッド", "Hybrid");
    I18n::reg("ac2_sv_desc_fdtd",
              "波動FDTD — 低域 (<500Hz) の正確な解析、回折・干渉を含む",
              "Wave FDTD — accurate at low frequencies (<500 Hz), "
              "includes diffraction and interference");
    I18n::reg("ac2_sv_desc_ray",
              "幾何音響レイトレース — 中高域、Odeon/CATT-Acoustic相当",
              "Geometrical ray tracing — mid/high bands, "
              "comparable to Odeon/CATT-Acoustic");
    I18n::reg("ac2_sv_desc_ism",
              "鏡像法 (Image-Source) — 直方体ホール、エコー初期反射を高速計算",
              "Image-source method — fast early reflections for shoebox halls");
    I18n::reg("ac2_sv_desc_hybrid", "低域FDTD + 中高域Ray のクロスオーバー",
              "Crossover of low-band FDTD and mid/high-band ray tracing");
    I18n::reg("ac2_num_rays", "レイ数", "# rays");
    I18n::reg("ac2_rays_unit", "本", "rays");
    I18n::reg("ac2_max_bounces", "最大反射回数", "Max bounces");
    I18n::reg("ac2_refl_model", "反射モデル", "Reflection model");
    I18n::reg("ac2_specular", "鏡面反射", "Specular");
    I18n::reg("ac2_diffuse", "拡散反射 (Lambert)", "Diffuse (Lambert)");
    I18n::reg("ac2_band_res", "周波数分解能", "Band resolution");
    I18n::reg("ac2_band_oct", "1オクターブ", "1 octave");
    I18n::reg("ac2_band_third", "1/3オクターブ", "1/3 octave");
    I18n::reg("ac2_crossover", "クロスオーバー", "Crossover");
    I18n::reg("ac2_image_order", "鏡像法次数", "Image-source order");
    I18n::reg("ac2_vis_test", "可視性テスト", "Visibility test");
    I18n::reg("ac2_on", "ON", "ON");
    I18n::reg("ac2_hybrid_split", "FDTD/Ray 境界", "FDTD/ray split");
    // 室内音響 / Room acoustics
    I18n::reg("ac2_room_section", "室内音響", "Room acoustics");
    I18n::reg("ac2_analysis_type", "解析タイプ", "Analysis type");
    // 音源 / Source
    I18n::reg("ac2_src_pos", "位置(x,y,z)", "Position (x,y,z)");
    I18n::reg("ac2_src_aim", "向き(θ,φ)", "Aim (θ,φ)");
    // 受音点 / Mic array 表 (AcousticOpts::receivers の View)
    I18n::reg("ac2_col_pos", "位置", "Position");
    I18n::reg("ac2_col_type", "タイプ", "Type");
    I18n::reg("ac2_col_name", "名前", "Name");
    I18n::reg("ac2_mic_add", "＋ 受音点を追加…", "＋ Add receiver…");
    I18n::reg("ac2_mic_del", "− 選択行を削除", "− Delete selected");
    I18n::reg("ac2_rcv_omni", "Omni", "Omni");
    I18n::reg("ac2_rcv_stereo", "Stereo", "Stereo");
    I18n::reg("ac2_rcv_binaural", "Binaural", "Binaural");
    I18n::reg("ac2_mic_note",
              "▸ 受音点は .ofdx に保存され、行数が「受音点数」と連動します。"
              "位置は「x, y, z」[m] で編集してください "
              "(数値以外を入力した行は保存値に戻ります)。"
              "受音点をソルバーへ渡す処理は未実装です。",
              "▸ Receivers are stored in the .ofdx sidecar and the row count "
              "tracks \"# receivers\". Edit a position as \"x, y, z\" [m] "
              "(a row that fails to parse reverts to the stored value). "
              "Passing receivers to a solver is not implemented yet.");
    // 周波数帯域 / Band
    I18n::reg("ac2_band_section", "周波数帯域", "Band");
    I18n::reg("ac2_third_octave", "1/3オクターブ", "1/3 octave");
    I18n::reg("ac2_band_target", "対象帯域:", "Target bands:");
    I18n::reg("ac2_band_low", "125Hz~", "125 Hz~");
    I18n::reg("ac2_band_mid", "500Hz~2k", "500 Hz~2k");
    I18n::reg("ac2_band_full", "125Hz~16k", "125 Hz~16k");
    // 音響評価指標 / Metrics (既存セクションへの追加分)
    I18n::reg("ac2_lf", "LF (側方音エネルギー)", "LF (lateral energy fraction)");
    // STI はモック (i18n.js ac_sti) が「STI (明瞭度)」。I18n.cpp の共通 ac_sti は
    // 旧表記「STI (音声明瞭度)」のままなので、表示だけモックに合わせる。
    I18n::reg("ac2_sti", "STI (明瞭度)", "STI");
    // 可聴化 / Auralization
    I18n::reg("ac2_aural_section", "可聴化", "Auralization");
    I18n::reg("ac2_play", "📻 再生", "📻 Play");
    I18n::reg("ac2_record", "⏺ 録音", "⏺ Record");
    I18n::reg("ac2_convolve", "⊕ 畳み込み", "⊕ Convolve");
    I18n::reg("ac2_convolve_tip",
              "ドライ音源と実測 RIR を畳み込みます (可聴化タブと同じエンジン)",
              "Convolve the dry source with the measured RIR "
              "(same engine as the Auralization tab)");
    I18n::reg("ac2_convolve_need_input",
              "ドライ音源 WAV と RIR WAV が未設定です。"
              "「%1」タブで指定してから実行してください。",
              "Dry-source WAV and RIR WAV are not set. "
              "Choose them on the \"%1\" tab first.");
    I18n::reg("ac2_convolve_done",
              "畳み込みが完了しました:\n%1\n"
              "ピーク: %2 dBFS / 推奨ゲイン: %3 dB\n"
              "A/B 波形の比較・詳細は「%4」タブで確認できます。",
              "Convolution finished:\n%1\n"
              "Peak: %2 dBFS / suggested gain: %3 dB\n"
              "See the \"%4\" tab for A/B waveforms and details.");
    I18n::reg("ac2_aural_src", "ソース音源", "Source signal");
    I18n::reg("ac2_src_click", "クリック / Click", "Click");
    I18n::reg("ac2_src_speech", "音声サンプル", "Speech sample");
    I18n::reg("ac2_src_music", "音楽 (anechoic)", "Music (anechoic)");
    I18n::reg("ac2_src_custom", "カスタム WAV…", "Custom WAV…");
    I18n::reg("ac2_out_format", "出力形式", "Output format");
    I18n::reg("ac2_out_mono", "モノラル", "Mono");
    I18n::reg("ac2_out_stereo", "ステレオ", "Stereo");
    I18n::reg("ac2_out_binaural", "バイノーラル (HRTF)", "Binaural (HRTF)");
    I18n::reg("ac2_out_ambi", "Ambisonics", "Ambisonics");
    // 材質設定 / Surface materials (AcousticOpts::absorption の View)
    I18n::reg("ac2_mat_section", "材質設定", "Surface materials");
    I18n::reg("ac2_col_face", "面", "Surface");
    I18n::reg("ac2_col_material", "材質", "Material");
    I18n::reg("ac2_col_a125", "α 125Hz", "α 125 Hz");
    I18n::reg("ac2_col_a1k", "α 1kHz", "α 1 kHz");
    I18n::reg("ac2_col_a4k", "α 4kHz", "α 4 kHz");
    // 面の役割 (AbsorptionRow::Role と同順)
    I18n::reg("ac2_role_audience", "客席", "Audience");
    I18n::reg("ac2_role_ceiling", "天井", "Ceiling");
    I18n::reg("ac2_role_sidewall", "側壁", "Side wall");
    I18n::reg("ac2_role_rearwall", "後壁", "Rear wall");
    I18n::reg("ac2_role_floor", "床", "Floor");
    I18n::reg("ac2_role_air", "空気吸収", "Air absorption");
    I18n::reg("ac2_role_other", "その他", "Other");
    I18n::reg("ac2_mat_note",
              "▸ 「室内音響解析」タブの吸音バジェットと同一データです — "
              "ここで α を編集すると残響時間 (Sabine/Eyring/Fitzroy) の"
              "計算にそのまま反映されます。面積・α 250/500/2kHz・"
              "空気吸収力 A・面の役割は同タブで編集してください。",
              "▸ Same data as the absorption budget on the \"Room acoustics\" "
              "tab — editing α here feeds straight into the reverberation "
              "time (Sabine/Eyring/Fitzroy). Area, α at 250/500/2 kHz, the "
              "air absorption A and the surface role are edited on that tab.");
    return true;
}();

// mock の CSS クラス相当 (最小限のスタイル)
;
const char kMuted[] = "color:#888888;";

QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(kMuted);
    l->setWordWrap(true);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// 編集可能テーブル (q-table 相当)。行はモデル (AcousticOpts) から流し込む
// ので初期行数は 0。visibleRows は高さの目安にのみ使う。
QTableWidget *makeTable(QWidget *parent, const QStringList &headers,
                        int visibleRows)
{
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setVisible(false);
    t->setMinimumHeight(visibleRows * 26 + 40);
    return t;
}

// 先頭列のチェックボックスセル (mock の <input type="checkbox">)
QTableWidgetItem *checkItem(bool on)
{
    auto *it = new QTableWidgetItem;
    it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    return it;
}

// 数値セル (右寄せ, className="num" 相当)
QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

// 等幅セル (className="mono" 相当)
QTableWidgetItem *monoItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    QFont f = it->font();
    f.setStyleHint(QFont::Monospace);
    // 実在するファミリのみ指定 (Theme が環境ごとに解決済み)
    if (const QString mf = Theme::monoFontFamily(); !mf.isEmpty())
        f.setFamily(mf);
    it->setFont(f);
    return it;
}

// <Seg> 相当: 少数選択肢の排他選択 → QComboBox
QComboBox *makeSeg(QWidget *parent, const QStringList &items, int current)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
}

QSpinBox *makeSpin(QWidget *parent, int lo, int hi, int value,
                   const QString &suffix = QString())
{
    auto *w = new QSpinBox(parent);
    w->setRange(lo, hi);
    w->setValue(value);
    if (!suffix.isEmpty()) w->setSuffix(suffix);
    w->setMaximumWidth(140);
    return w;
}

// "a, b, c" 形式の数値列をパース ("°" は読み飛ばす)。
// 要素数が n で全要素が数値のときだけ out に書いて true を返す。
bool parseNumList(const QString &text, int n, double *out)
{
    QString s = text;
    s.remove(QStringLiteral("°"));
    const QStringList parts = s.split(',', Qt::SkipEmptyParts);
    if (parts.size() != n) return false;
    for (int i = 0; i < n; ++i) {
        bool ok = false;
        out[i] = parts[i].trimmed().toDouble(&ok);
        if (!ok) return false;
    }
    return true;
}

// 音源位置 / 向きの表示書式 (mock の "−3.0, 1.6, 5.0" / "90°, 0°" 相当)。
// 精度は OfdIO の num() と同じ 'g' 10 桁 (表示で値を丸めない)。
QString fmtPos(double x, double y, double z)
{
    return QStringLiteral("%1, %2, %3").arg(QString::number(x, 'g', 10),
                                            QString::number(y, 'g', 10),
                                            QString::number(z, 'g', 10));
}

QString fmtAim(double theta, double phi)
{
    return QStringLiteral("%1°, %2°").arg(QString::number(theta, 'g', 10),
                                          QString::number(phi, 'g', 10));
}

// AbsorptionRow::Role → 「面」列の表示名 (役割は室内音響解析タブで決まる)
QString roleLabel(int role)
{
    switch (role) {
    case AbsorptionRow::Audience: return I18n::tr("ac2_role_audience");
    case AbsorptionRow::Ceiling:  return I18n::tr("ac2_role_ceiling");
    case AbsorptionRow::SideWall: return I18n::tr("ac2_role_sidewall");
    case AbsorptionRow::RearWall: return I18n::tr("ac2_role_rearwall");
    case AbsorptionRow::Floor:    return I18n::tr("ac2_role_floor");
    case AbsorptionRow::Air:      return I18n::tr("ac2_role_air");
    default:                      return I18n::tr("ac2_role_other");
    }
}

} // namespace

AcousticTab::AcousticTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    auto *hint = new QLabel(I18n::tr("ac_mapping_hint"), body);
    hint->setWordWrap(true);
    v->addWidget(hint);

    auto *sm = new SectionBox(I18n::tr("ac_metrics"), body);
    m_rt60 = new QCheckBox(I18n::tr("ac_rt60"), sm);
    m_c80  = new QCheckBox(I18n::tr("ac_c80"), sm);
    m_d50  = new QCheckBox(I18n::tr("ac_d50"), sm);
    m_sti  = new QCheckBox(I18n::tr("ac2_sti"), sm);
    m_edt  = new QCheckBox(I18n::tr("ac_edt"), sm);
    m_irf  = new QCheckBox(I18n::tr("ac_irf"), sm);
    m_aural = new QCheckBox(I18n::tr("ac_aurora"), sm);
    for (auto *c : { m_rt60, m_c80, m_d50, m_sti, m_edt, m_irf, m_aural })
        sm->vbox()->addWidget(c);
    // mock の Metrics 行にある LF (側方音エネルギー)。AcousticOpts::lf に永続化。
    m_lf = makeCheck(I18n::tr("ac2_lf"), false, sm);
    sm->vbox()->addWidget(m_lf);
    m_sampleRate = new QComboBox(sm);
    m_sampleRate->addItems({ "44100", "48000", "96000" });
    sm->form()->addRow(I18n::tr("ac_sample_rate"), m_sampleRate);
    v->addWidget(sm);

    auto *ss = new SectionBox(I18n::tr("ac_source"), body);
    m_directivity = new QComboBox(ss);
    m_directivity->addItem(I18n::tr("ac_omni"));      // omni
    m_directivity->addItem(I18n::tr("ac_cardioid"));  // cardioid
    m_directivity->addItem(I18n::tr("ac_speaker"));   // speaker
    m_spl = new QDoubleSpinBox(ss);
    m_spl->setRange(0, 200);
    m_spl->setSuffix(" dB");
    ss->form()->addRow(I18n::tr("ac_directivity"), m_directivity);
    ss->form()->addRow(I18n::tr("ac_spl"), m_spl);
    // mock の 位置(x,y,z) [m] / 向き(θ,φ) [deg] 行 — AcousticOpts::src*_m /
    // srcAim*_deg に永続化 (テキストは refresh() が書き込む)。
    // 指向性行の前後に差し込んでモックの並び順にする。
    m_srcPos = new QLineEdit(ss);
    m_srcPos->setStyleSheet(Theme::monoQss());
    ss->form()->insertRow(0, I18n::tr("ac2_src_pos"), m_srcPos);
    m_srcAim = new QLineEdit(ss);
    m_srcAim->setStyleSheet(Theme::monoQss());
    ss->form()->insertRow(2, I18n::tr("ac2_src_aim"), m_srcAim);
    v->addWidget(ss);

    auto *sr = new SectionBox(I18n::tr("ac_mics"), body);
    m_micCount = new QSpinBox(sr);
    m_micCount->setRange(1, 256);
    sr->form()->addRow(I18n::tr("ac_mic_count"), m_micCount);
    // 受音点表 — AcousticOpts::receivers (.ofdx) の View。
    // 有効/位置/タイプ/名前を編集でき、行数は受音点数スピンと双方向に同期する。
    m_micTable = makeTable(sr, { QString(), QStringLiteral("#"),
                                 I18n::tr("ac2_col_pos"),
                                 I18n::tr("ac2_col_type"),
                                 I18n::tr("ac2_col_name") }, 5);
    sr->vbox()->addWidget(m_micTable);
    auto *micBtns = new QHBoxLayout();
    auto *micAdd = new QPushButton(I18n::tr("ac2_mic_add"), sr);
    auto *micDel = new QPushButton(I18n::tr("ac2_mic_del"), sr);
    micBtns->addWidget(micAdd);
    micBtns->addWidget(micDel);
    micBtns->addStretch(1);
    sr->vbox()->addLayout(micBtns);
    sr->vbox()->addWidget(mutedLabel(I18n::tr("ac2_mic_note"), sr));
    v->addWidget(sr);

    connect(m_micTable, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyReceivers();
    });
    connect(micAdd, &QPushButton::clicked, this, [this] {
        AcousticOpts &a = m_p->acoustic();
        if (a.receivers.size() >= m_micCount->maximum()) return;
        // 追加行の初期値は既定リストの次の 1 点 (Project.h)
        a.receivers.push_back(defaultReceivers(a.receivers.size() + 1).last());
        a.micCount = a.receivers.size();
        refreshReceivers();
        m_p->touch();
    });
    connect(micDel, &QPushButton::clicked, this, [this] {
        AcousticOpts &a = m_p->acoustic();
        const int r = m_micTable->currentRow();
        // 受音点は最低 1 点 (受音点数スピンの下限と揃える)
        if (a.receivers.size() <= 1 || r < 0 || r >= a.receivers.size()) return;
        a.receivers.removeAt(r);
        a.micCount = a.receivers.size();
        refreshReceivers();
        m_p->touch();
    });

    // ── 以下、モック (tabs.jsx AcousticTab) にあって未実装だったセクションを
    //    モックの並び順 (ソルバー → 室内音響 → 周波数帯域 → 可聴化 → 材質) で追加。
    //    解析タイプ・周波数帯域は AcousticOpts (.ofdx) に永続化。ソルバー・
    //    可聴化ソース/出力形式・材質表は Project に対応フィールドが無いので
    //    引き続きローカル状態。

    // ソルバー / Solver — 選択に応じた説明文 + 条件付きパラメータ
    auto *sv = new SectionBox(I18n::tr("ac2_solver_section"), body);
    m_solver = makeSeg(sv, { I18n::tr("ac2_sv_fdtd"), I18n::tr("ac2_sv_ray"),
                             I18n::tr("ac2_sv_ism"), I18n::tr("ac2_sv_hybrid") }, 0);
    sv->vbox()->addWidget(m_solver);
    m_solverDesc = mutedLabel(QString(), sv);
    sv->vbox()->addWidget(m_solverDesc);

    m_rayPanel = new QWidget(sv);
    auto *rayForm = new QFormLayout(m_rayPanel);
    rayForm->setContentsMargins(0, 0, 0, 0);
    rayForm->setHorizontalSpacing(8);
    rayForm->setVerticalSpacing(4);
    m_numRays = makeSpin(m_rayPanel, 1, 100000000, 100000,
                         QStringLiteral(" ") + I18n::tr("ac2_rays_unit"));
    rayForm->addRow(I18n::tr("ac2_num_rays"), m_numRays);
    m_maxBounces = makeSpin(m_rayPanel, 1, 1000, 50);
    rayForm->addRow(I18n::tr("ac2_max_bounces"), m_maxBounces);
    auto *reflRow = new QHBoxLayout();
    m_specular = makeCheck(I18n::tr("ac2_specular"), true, m_rayPanel);
    m_diffuse  = makeCheck(I18n::tr("ac2_diffuse"), true, m_rayPanel);
    reflRow->addWidget(m_specular);
    reflRow->addWidget(m_diffuse);
    reflRow->addStretch(1);
    rayForm->addRow(I18n::tr("ac2_refl_model"), reflRow);
    m_rayBandRes = makeSeg(m_rayPanel,
                           { I18n::tr("ac2_band_oct"), I18n::tr("ac2_band_third") }, 0);
    rayForm->addRow(I18n::tr("ac2_band_res"), m_rayBandRes);
    m_rayCrossover = makeSpin(m_rayPanel, 20, 20000, 500, QStringLiteral(" Hz"));
    rayForm->addRow(I18n::tr("ac2_crossover"), m_rayCrossover);
    sv->vbox()->addWidget(m_rayPanel);

    m_ismPanel = new QWidget(sv);
    auto *ismForm = new QFormLayout(m_ismPanel);
    ismForm->setContentsMargins(0, 0, 0, 0);
    ismForm->setHorizontalSpacing(8);
    ismForm->setVerticalSpacing(4);
    m_ismOrder = makeSpin(m_ismPanel, 1, 20, 6);
    ismForm->addRow(I18n::tr("ac2_image_order"), m_ismOrder);
    m_ismVisibility = makeCheck(I18n::tr("ac2_on"), true, m_ismPanel);
    ismForm->addRow(I18n::tr("ac2_vis_test"), m_ismVisibility);
    sv->vbox()->addWidget(m_ismPanel);

    m_hybridPanel = new QWidget(sv);
    auto *hybForm = new QFormLayout(m_hybridPanel);
    hybForm->setContentsMargins(0, 0, 0, 0);
    hybForm->setHorizontalSpacing(8);
    hybForm->setVerticalSpacing(4);
    m_hybridSplit = makeSpin(m_hybridPanel, 20, 20000, 500, QStringLiteral(" Hz"));
    hybForm->addRow(I18n::tr("ac2_hybrid_split"), m_hybridSplit);
    sv->vbox()->addWidget(m_hybridPanel);
    sv->vbox()->addWidget(tabhelp::unwiredNote(sv));
    v->addWidget(sv);

    // 室内音響 / Room acoustics — 解析タイプ (AcousticOpts::analysisType)
    auto *ra = new SectionBox(I18n::tr("ac2_room_section"), body);
    m_analysisType = makeSeg(ra, { I18n::tr("ac_irf"), I18n::tr("ac_rt60"),
                                   I18n::tr("ac2_sti") }, 0);
    ra->form()->addRow(I18n::tr("ac2_analysis_type"), m_analysisType);
    v->addWidget(ra);

    // 周波数帯域 / Band (AcousticOpts::thirdOctave / bandRange)
    auto *fb = new SectionBox(I18n::tr("ac2_band_section"), body);
    m_thirdOctave = makeCheck(I18n::tr("ac2_third_octave"), true, fb);
    fb->vbox()->addWidget(m_thirdOctave);
    auto *bandRow = new QHBoxLayout();
    bandRow->addWidget(mutedLabel(I18n::tr("ac2_band_target"), fb));
    m_bandRange = makeSeg(fb, { I18n::tr("ac2_band_low"), I18n::tr("ac2_band_mid"),
                                I18n::tr("ac2_band_full") }, 2);
    bandRow->addWidget(m_bandRange);
    bandRow->addStretch(1);
    fb->vbox()->addLayout(bandRow);
    v->addWidget(fb);

    // 可聴化 / Auralization
    auto *au = new SectionBox(I18n::tr("ac2_aural_section"), body);
    auto *auralBtns = new QHBoxLayout();
    for (const char *k : { "ac2_play", "ac2_record" }) {
        auto *b = new QPushButton(I18n::tr(k), au);
        tabhelp::markNotImplemented(b);   // 再生/録音は未実装 (QtMultimedia 禁止)
        auralBtns->addWidget(b);
    }
    // 畳み込みは実装済みの可聴化経路 (可聴化タブと同じ
    // QtAcousticAdapter::convolveFiles) へ委譲する
    auto *convolveBtn = new QPushButton(I18n::tr("ac2_convolve"), au);
    convolveBtn->setToolTip(I18n::tr("ac2_convolve_tip"));
    connect(convolveBtn, &QPushButton::clicked,
            this, &AcousticTab::runConvolve);
    auralBtns->addWidget(convolveBtn);
    auralBtns->addStretch(1);
    au->vbox()->addLayout(auralBtns);
    m_auralSource = makeSeg(au, { I18n::tr("ac2_src_click"),
                                  I18n::tr("ac2_src_speech"),
                                  I18n::tr("ac2_src_music"),
                                  I18n::tr("ac2_src_custom") }, 0);
    au->form()->addRow(I18n::tr("ac2_aural_src"), m_auralSource);
    auto *outRow = new QHBoxLayout();
    m_outMono     = makeCheck(I18n::tr("ac2_out_mono"), false, au);
    m_outStereo   = makeCheck(I18n::tr("ac2_out_stereo"), true, au);
    m_outBinaural = makeCheck(I18n::tr("ac2_out_binaural"), false, au);
    m_outAmbi     = makeCheck(I18n::tr("ac2_out_ambi"), false, au);
    for (auto *c : { m_outMono, m_outStereo, m_outBinaural, m_outAmbi })
        outRow->addWidget(c);
    outRow->addStretch(1);
    au->form()->addRow(I18n::tr("ac2_out_format"), outRow);
    au->vbox()->addWidget(tabhelp::unwiredNote(au));
    v->addWidget(au);

    // 材質設定 / Surface materials — 吸音率表 (125Hz / 1kHz / 4kHz)。
    // RoomAcousticsTab の吸音バジェットと同一モデル (AcousticOpts::absorption)
    // をバインドする。同じ α が 2 か所で食い違わないよう、ここでの編集は
    // 直接 absorption[] へ書き戻し、残響計算にそのまま反映される。
    auto *ms = new SectionBox(I18n::tr("ac2_mat_section"), body);
    m_surfTable = makeTable(ms, { I18n::tr("ac2_col_face"),
                                  I18n::tr("ac2_col_material"),
                                  I18n::tr("ac2_col_a125"),
                                  I18n::tr("ac2_col_a1k"),
                                  I18n::tr("ac2_col_a4k") }, 5);
    ms->vbox()->addWidget(m_surfTable);
    ms->vbox()->addWidget(mutedLabel(I18n::tr("ac2_mat_note"), ms));
    v->addWidget(ms);

    connect(m_surfTable, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applySurfaces();
    });

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    auto applyCb = [this] { apply(); };
    for (auto *c : { m_rt60, m_c80, m_d50, m_sti, m_edt, m_irf, m_aural,
                     m_lf, m_thirdOctave })
        connect(c, &QCheckBox::toggled, this, applyCb);
    connect(m_sampleRate, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_directivity, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_spl, &QDoubleSpinBox::valueChanged, this, applyCb);
    // 受音点数は受音点リストの行数そのもの → 専用経路でリストを伸縮させる
    connect(m_micCount, &QSpinBox::valueChanged,
            this, &AcousticTab::applyReceiverCount);
    connect(m_srcPos, &QLineEdit::editingFinished, this, applyCb);
    connect(m_srcAim, &QLineEdit::editingFinished, this, applyCb);
    connect(m_analysisType, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_bandRange, &QComboBox::currentIndexChanged, this, applyCb);

    // ソルバー選択はローカル状態 (Project 非永続) → apply() は呼ばない
    connect(m_solver, &QComboBox::currentIndexChanged,
            this, &AcousticTab::updateSolverView);
    updateSolverView();

    connect(project, &Project::loaded, this, &AcousticTab::refresh);
    // 吸音バジェットは室内音響解析タブと共有するモデルなので、
    // 向こうで編集されたら (changed) こちらの表も追従させる。
    connect(project, &Project::changed, this, &AcousticTab::refresh);
    refresh();
}

void AcousticTab::updateSolverView()
{
    static const char *kDesc[4] = { "ac2_sv_desc_fdtd", "ac2_sv_desc_ray",
                                    "ac2_sv_desc_ism", "ac2_sv_desc_hybrid" };
    const int i = qBound(0, m_solver->currentIndex(), 3);
    m_solverDesc->setText(I18n::tr(kDesc[i]));
    m_rayPanel->setVisible(i == 1);
    m_ismPanel->setVisible(i == 2);
    m_hybridPanel->setVisible(i == 3);
}

void AcousticTab::apply()
{
    if (m_updating) return;
    AcousticOpts &a = m_p->acoustic();
    a.rt60 = m_rt60->isChecked();
    a.c80  = m_c80->isChecked();
    a.d50  = m_d50->isChecked();
    a.sti  = m_sti->isChecked();
    a.edt  = m_edt->isChecked();
    a.impulseResponse = m_irf->isChecked();
    a.auralization = m_aural->isChecked();
    a.sampleRate = m_sampleRate->currentText().toInt();
    static const char *dirs[] = { "omni", "cardioid", "speaker" };
    a.srcDirectivity = dirs[qBound(0, m_directivity->currentIndex(), 2)];
    a.srcSPL_dB = m_spl->value();
    // 受音点数 = 受音点リストの行数 (不変条件)。スピンの値は
    // applyReceiverCount() がリストを伸縮させてから反映される。
    a.micCount = a.receivers.size();
    a.lf = m_lf->isChecked();
    // 音源位置 / 向き: パースできた場合だけモデルへ書き込み、不正入力は
    // 表示をモデル値に戻す (UI とモデルの乖離を作らない — .claude/rules/gui.md)
    double pos[3];
    if (parseNumList(m_srcPos->text(), 3, pos)) {
        a.srcX_m = pos[0]; a.srcY_m = pos[1]; a.srcZ_m = pos[2];
    } else {
        m_srcPos->setText(fmtPos(a.srcX_m, a.srcY_m, a.srcZ_m));
    }
    double aim[2];
    if (parseNumList(m_srcAim->text(), 2, aim)) {
        a.srcAimTheta_deg = aim[0]; a.srcAimPhi_deg = aim[1];
    } else {
        m_srcAim->setText(fmtAim(a.srcAimTheta_deg, a.srcAimPhi_deg));
    }
    a.analysisType = qBound(0, m_analysisType->currentIndex(), 2);
    a.thirdOctave = m_thirdOctave->isChecked();
    a.bandRange = qBound(0, m_bandRange->currentIndex(), 2);
    m_p->touch();
}

void AcousticTab::refresh()
{
    m_updating = true;
    const AcousticOpts &a = m_p->acoustic();
    m_rt60->setChecked(a.rt60);
    m_c80->setChecked(a.c80);
    m_d50->setChecked(a.d50);
    m_sti->setChecked(a.sti);
    m_edt->setChecked(a.edt);
    m_irf->setChecked(a.impulseResponse);
    m_aural->setChecked(a.auralization);
    m_sampleRate->setCurrentText(QString::number(a.sampleRate));
    const int di = (a.srcDirectivity == "cardioid") ? 1
                 : (a.srcDirectivity == "speaker")  ? 2 : 0;
    m_directivity->setCurrentIndex(di);
    m_spl->setValue(a.srcSPL_dB);
    m_lf->setChecked(a.lf);
    m_srcPos->setText(fmtPos(a.srcX_m, a.srcY_m, a.srcZ_m));
    m_srcAim->setText(fmtAim(a.srcAimTheta_deg, a.srcAimPhi_deg));
    m_analysisType->setCurrentIndex(qBound(0, a.analysisType, 2));
    m_thirdOctave->setChecked(a.thirdOctave);
    m_bandRange->setCurrentIndex(qBound(0, a.bandRange, 2));
    m_updating = false;
    refreshReceivers();
    refreshSurfaces();
}

// ── 受音点リスト (AcousticOpts::receivers) ─────────────────────────────────
// model → widgets。受音点数スピンも行数へ合わせる (同一データの 2 表示)。
void AcousticTab::refreshReceivers()
{
    m_updating = true;
    const QVector<ReceiverRow> &rows = m_p->acoustic().receivers;
    m_micCount->setValue(rows.size());
    m_micTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const ReceiverRow &row = rows[r];
        m_micTable->setItem(r, 0, checkItem(row.enabled));
        auto *idx = numItem(QString::number(r + 1));
        idx->setFlags(idx->flags() & ~Qt::ItemIsEditable);   // 行番号は自動
        m_micTable->setItem(r, 1, idx);
        m_micTable->setItem(r, 2, monoItem(fmtPos(row.x, row.y, row.z)));
        // タイプは列挙なのでセル内コンボ (行数が変わらない限り作り直さない)
        auto *type = qobject_cast<QComboBox *>(m_micTable->cellWidget(r, 3));
        if (!type) {
            type = new QComboBox(m_micTable);
            type->addItems({ I18n::tr("ac2_rcv_omni"),
                             I18n::tr("ac2_rcv_stereo"),
                             I18n::tr("ac2_rcv_binaural") });
            m_micTable->setCellWidget(r, 3, type);
            connect(type, &QComboBox::currentIndexChanged, this, [this] {
                if (m_updating) return;
                applyReceivers();
            });
        }
        type->setCurrentIndex(qBound(0, row.type, 2));
        m_micTable->setItem(r, 4, new QTableWidgetItem(row.name));
    }
    m_updating = false;
}

// widgets → model。位置が "x, y, z" として読めない行はモデル値へ戻す
// (UI とモデルの乖離を作らない — .claude/rules/gui.md)。
void AcousticTab::applyReceivers()
{
    AcousticOpts &a = m_p->acoustic();
    for (int r = 0; r < m_micTable->rowCount() && r < a.receivers.size(); ++r) {
        ReceiverRow &row = a.receivers[r];
        if (auto *en = m_micTable->item(r, 0))
            row.enabled = en->checkState() == Qt::Checked;
        if (auto *ps = m_micTable->item(r, 2)) {
            double pos[3];
            if (parseNumList(ps->text(), 3, pos)) {
                row.x = pos[0]; row.y = pos[1]; row.z = pos[2];
            }
            // 読めない入力は無視 → 直後の refresh() が保存値を書き戻す
        }
        if (auto *tp = qobject_cast<QComboBox *>(m_micTable->cellWidget(r, 3)))
            row.type = qBound(0, tp->currentIndex(), 2);
        if (auto *nm = m_micTable->item(r, 4))
            row.name = nm->text();
    }
    a.micCount = a.receivers.size();
    m_p->touch();     // changed → refresh() が表を書き戻す
}

// 受音点数スピン → リストの伸縮 (増分は既定リストの続き、減分は末尾から)
void AcousticTab::applyReceiverCount()
{
    if (m_updating) return;
    AcousticOpts &a = m_p->acoustic();
    const int n = qMax(1, m_micCount->value());
    if (n == a.receivers.size()) return;
    while (a.receivers.size() > n) a.receivers.removeLast();
    while (a.receivers.size() < n)
        a.receivers.push_back(defaultReceivers(a.receivers.size() + 1).last());
    a.micCount = a.receivers.size();
    refreshReceivers();
    m_p->touch();
}

// ── 材質設定 (AcousticOpts::absorption = 吸音バジェットと同一モデル) ───────
void AcousticTab::refreshSurfaces()
{
    m_updating = true;
    const QVector<AbsorptionRow> &rows = m_p->acoustic().absorption;
    m_surfTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const AbsorptionRow &row = rows[r];
        // 空気吸収行は面ではないので α を持たない (吸音力 A を直接指定する)
        const bool air = row.role == AbsorptionRow::Air;
        m_surfTable->setItem(r, 0, tabhelp::roItem(roleLabel(row.role)));
        m_surfTable->setItem(r, 1, new QTableWidgetItem(row.name));
        auto alphaCell = [air](double v) {
            auto *it = numItem(air ? QStringLiteral("—")
                                   : QString::number(v, 'g', 4));
            if (air) it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };
        m_surfTable->setItem(r, 2, alphaCell(row.alpha[0]));   // 125 Hz
        m_surfTable->setItem(r, 3, alphaCell(row.alpha[3]));   // 1 kHz
        m_surfTable->setItem(r, 4, alphaCell(row.alpha[5]));   // 4 kHz
    }
    m_updating = false;
}

// widgets → model。表示している 3 帯域 (125Hz/1kHz/4kHz) と材質名だけを
// 書き戻す — 250/500/2kHz は独立した実測値なので勝手に補間しない
// (室内音響解析タブの吸音バジェットで編集する)。
void AcousticTab::applySurfaces()
{
    AcousticOpts &a = m_p->acoustic();
    for (int r = 0; r < m_surfTable->rowCount() && r < a.absorption.size(); ++r) {
        AbsorptionRow &row = a.absorption[r];
        if (auto *nm = m_surfTable->item(r, 1))
            row.name = nm->text();
        if (row.role == AbsorptionRow::Air) continue;   // α 列は "—"
        auto alpha = [this, r](int col, double cur) {
            auto *it = m_surfTable->item(r, col);
            if (!it) return cur;
            bool ok = false;
            const double v = it->text().toDouble(&ok);
            // 負の吸音率は物理的にあり得ない → 0 でクランプ
            // (refresh() がクランプ後の値を書き戻すので表示と一致する)
            return ok ? qMax(0.0, v) : cur;
        };
        row.alpha[0] = alpha(2, row.alpha[0]);
        row.alpha[3] = alpha(3, row.alpha[3]);
        row.alpha[5] = alpha(4, row.alpha[5]);
    }
    m_p->touch();     // 残響計算 (室内音響解析タブ) も追従する
}

// 「⊕ 畳み込み」— 実装済みの可聴化経路 (可聴化タブと同じ
// QtAcousticAdapter::convolveFiles、同期実行も同タブと同じ契約) へ委譲する。
// 入力は可聴化タブと共通の OperaAcousticSettings (ドライ WAV / RIR WAV /
// 出力先 / ゲインモード)。未設定なら実行せず可聴化タブへ案内する
// (未設定のまま「完了」を装う虚偽表示をしない — CLAUDE.md 絶対規則 5)。
void AcousticTab::runConvolve()
{
    using namespace ofd::acoustics;
    OperaAcousticSettings &s = m_p->operaAcoustic();
    if (s.auralizationDryFile.trimmed().isEmpty() ||
        s.rirPath.trimmed().isEmpty()) {
        QMessageBox::information(this, I18n::tr("ac2_convolve"),
            I18n::tr("ac2_convolve_need_input")
                .arg(I18n::tr("t_auralization")));
        return;
    }
    // 出力先が未指定なら実行時に選択させる (可聴化タブと同じ流儀)。
    // 選択結果はモデルへ書き戻し、可聴化タブとも共有する。
    QString outPath = s.auralizationOutputFile;
    if (outPath.trimmed().isEmpty()) {
        outPath = QFileDialog::getSaveFileName(
            this, I18n::tr("ac2_convolve"), QStringLiteral("auralized.wav"),
            I18n::tr("rir_wav_filter"));
        if (outPath.isEmpty()) return;
        s.auralizationOutputFile = outPath;
        m_p->touch();
    }
    const AcousticResult<ConvolutionInfo> res =
        QtAcousticAdapter::convolveFiles(s.auralizationDryFile, s.rirPath,
                                         outPath, s.auralizationGainMode);
    if (!res.success()) {
        // fs 不一致はリサンプリングしない旨も明示 (可聴化タブと同じ文言)
        QString msg = I18n::tr("aur_status_error")
                          .arg(QString::fromUtf8(
                                   acousticErrorCodeName(res.errorCode())),
                               QString::fromStdString(res.message()));
        if (res.errorCode() == kSampleRateMismatch)
            msg += QStringLiteral("\n") + I18n::tr("aur_no_resample_note");
        QMessageBox::warning(this, I18n::tr("ac2_convolve"), msg);
        return;
    }
    // 結果は書き出した WAV のサンプルで測った実測値 (アダプター契約)。
    const ConvolutionInfo &info = res.value();
    QMessageBox::information(this, I18n::tr("ac2_convolve"),
        I18n::tr("ac2_convolve_done")
            .arg(outPath,
                 QString::number(info.outputPeakDbfs, 'f', 1),
                 QString::number(info.suggestedGainDb, 'f', 1),
                 I18n::tr("t_auralization")));
}
