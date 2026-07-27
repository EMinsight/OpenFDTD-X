// AcousticTab.cpp
#include "AcousticTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
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
    // 受音点 / Mic array 表
    I18n::reg("ac2_col_pos", "位置", "Position");
    I18n::reg("ac2_col_type", "タイプ", "Type");
    I18n::reg("ac2_col_name", "名前", "Name");
    I18n::reg("ac2_mic_p1", "P1 中央", "P1 center");
    I18n::reg("ac2_mic_p2", "P2 左", "P2 left");
    I18n::reg("ac2_mic_p3", "P3 右", "P3 right");
    I18n::reg("ac2_mic_p4", "P4 後方", "P4 rear");
    I18n::reg("ac2_mic_add", "＋ 受音点を追加…", "＋ Add receiver…");
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
    // 材質設定 / Surface materials
    I18n::reg("ac2_mat_section", "材質設定", "Surface materials");
    I18n::reg("ac2_col_face", "面", "Surface");
    I18n::reg("ac2_col_material", "材質", "Material");
    I18n::reg("ac2_col_a125", "α 125Hz", "α 125 Hz");
    I18n::reg("ac2_col_a1k", "α 1kHz", "α 1 kHz");
    I18n::reg("ac2_col_a4k", "α 4kHz", "α 4 kHz");
    I18n::reg("ac2_face_floor", "床", "Floor");
    I18n::reg("ac2_face_wall", "壁", "Wall");
    I18n::reg("ac2_face_ceiling", "天井", "Ceiling");
    I18n::reg("ac2_face_seats", "客席", "Audience");
    I18n::reg("ac2_mat_wood", "木質フローリング", "Wood flooring");
    I18n::reg("ac2_mat_gypsum", "石膏ボード", "Gypsum board");
    I18n::reg("ac2_mat_panel", "音響パネル", "Acoustic panel");
    I18n::reg("ac2_mat_audience", "客 (満席)", "Audience (full)");
    return true;
}();

// mock の CSS クラス相当 (最小限のスタイル)
const char kMono[]  = "font-family:'Consolas','Menlo',monospace;";
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

// 読取専用テーブル (q-table 相当)
QTableWidget *makeStaticTable(QWidget *parent, const QStringList &headers,
                              int rows)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(rows * 26 + 40);
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
    f.setFamily("Menlo");
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
    // mock の Metrics 行にある LF (側方音エネルギー)。Project に該当フィールドが
    // 無いのでローカル状態 (既定 off)。
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
    // mock の 位置(x,y,z) / 向き(θ,φ) 行 (ローカル状態、既定値はモックのまま)。
    // 指向性行の前後に差し込んでモックの並び順にする。
    m_srcPos = new QLineEdit(QStringLiteral("-3.0, 1.6, 5.0"), ss);
    m_srcPos->setStyleSheet(kMono);
    ss->form()->insertRow(0, I18n::tr("ac2_src_pos"), m_srcPos);
    m_srcAim = new QLineEdit(QStringLiteral("90°, 0°"), ss);
    m_srcAim->setStyleSheet(kMono);
    ss->form()->insertRow(2, I18n::tr("ac2_src_aim"), m_srcAim);
    v->addWidget(ss);

    auto *sr = new SectionBox(I18n::tr("ac_mics"), body);
    m_micCount = new QSpinBox(sr);
    m_micCount->setRange(1, 256);
    sr->form()->addRow(I18n::tr("ac_mic_count"), m_micCount);
    // 受音点表 (mock の literal 行)。最終行は「＋ 受音点を追加…」プレースホルダ。
    m_micTable = makeStaticTable(sr, { QString(), QStringLiteral("#"),
                                       I18n::tr("ac2_col_pos"),
                                       I18n::tr("ac2_col_type"),
                                       I18n::tr("ac2_col_name") }, 5);
    struct MicRow { const char *pos; const char *kind; const char *nameKey; };
    static const MicRow kMics[4] = {
        { "0.0, 1.2, 8.0",  "Omni",   "ac2_mic_p1" },
        { "-2.0, 1.2, 8.0", "Omni",   "ac2_mic_p2" },
        { "2.0, 1.2, 8.0",  "Omni",   "ac2_mic_p3" },
        { "0.0, 1.2, 14.0", "Stereo", "ac2_mic_p4" }
    };
    for (int r = 0; r < 4; ++r) {
        m_micTable->setItem(r, 0, checkItem(true));
        m_micTable->setItem(r, 1, numItem(QString::number(r + 1)));
        m_micTable->setItem(r, 2, monoItem(QString::fromUtf8(kMics[r].pos)));
        m_micTable->setItem(r, 3,
            new QTableWidgetItem(QString::fromUtf8(kMics[r].kind)));
        m_micTable->setItem(r, 4,
            new QTableWidgetItem(I18n::tr(kMics[r].nameKey)));
    }
    m_micTable->setItem(4, 0, checkItem(false));
    auto *micAdd = new QTableWidgetItem(I18n::tr("ac2_mic_add"));
    QFont micAddFont = micAdd->font();
    micAddFont.setItalic(true);
    micAdd->setFont(micAddFont);
    micAdd->setForeground(QColor("#888888"));
    m_micTable->setItem(4, 1, micAdd);
    m_micTable->setSpan(4, 1, 1, 4);
    sr->vbox()->addWidget(m_micTable);
    v->addWidget(sr);

    // ── 以下、モック (tabs.jsx AcousticTab) にあって未実装だったセクションを
    //    モックの並び順 (ソルバー → 室内音響 → 周波数帯域 → 可聴化 → 材質) で追加。
    //    Project に対応フィールドが無いのでいずれもローカル状態。

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
    v->addWidget(sv);

    // 室内音響 / Room acoustics — 解析タイプ
    auto *ra = new SectionBox(I18n::tr("ac2_room_section"), body);
    m_analysisType = makeSeg(ra, { I18n::tr("ac_irf"), I18n::tr("ac_rt60"),
                                   I18n::tr("ac2_sti") }, 0);
    ra->form()->addRow(I18n::tr("ac2_analysis_type"), m_analysisType);
    v->addWidget(ra);

    // 周波数帯域 / Band
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
    for (const char *k : { "ac2_play", "ac2_record", "ac2_convolve" })
        auralBtns->addWidget(new QPushButton(I18n::tr(k), au));
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
    v->addWidget(au);

    // 材質設定 / Surface materials — mock の吸音率表 (125Hz / 1kHz / 4kHz)
    auto *ms = new SectionBox(I18n::tr("ac2_mat_section"), body);
    m_surfTable = makeStaticTable(ms, { I18n::tr("ac2_col_face"),
                                        I18n::tr("ac2_col_material"),
                                        I18n::tr("ac2_col_a125"),
                                        I18n::tr("ac2_col_a1k"),
                                        I18n::tr("ac2_col_a4k") }, 4);
    struct SurfRow { const char *faceKey; const char *matKey;
                     const char *a125; const char *a1k; const char *a4k; };
    static const SurfRow kSurf[4] = {
        { "ac2_face_floor",   "ac2_mat_wood",     "0.10", "0.07", "0.07" },
        { "ac2_face_wall",    "ac2_mat_gypsum",   "0.29", "0.05", "0.04" },
        { "ac2_face_ceiling", "ac2_mat_panel",    "0.30", "0.85", "0.90" },
        { "ac2_face_seats",   "ac2_mat_audience", "0.39", "0.80", "0.87" }
    };
    for (int r = 0; r < 4; ++r) {
        m_surfTable->setItem(r, 0, new QTableWidgetItem(I18n::tr(kSurf[r].faceKey)));
        m_surfTable->setItem(r, 1, new QTableWidgetItem(I18n::tr(kSurf[r].matKey)));
        m_surfTable->setItem(r, 2, numItem(QString::fromLatin1(kSurf[r].a125)));
        m_surfTable->setItem(r, 3, numItem(QString::fromLatin1(kSurf[r].a1k)));
        m_surfTable->setItem(r, 4, numItem(QString::fromLatin1(kSurf[r].a4k)));
    }
    ms->vbox()->addWidget(m_surfTable);
    v->addWidget(ms);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    auto applyCb = [this] { apply(); };
    for (auto *c : { m_rt60, m_c80, m_d50, m_sti, m_edt, m_irf, m_aural })
        connect(c, &QCheckBox::toggled, this, applyCb);
    connect(m_sampleRate, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_directivity, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_spl, &QDoubleSpinBox::valueChanged, this, applyCb);
    connect(m_micCount, &QSpinBox::valueChanged, this, applyCb);

    // ソルバー選択はローカル状態 (Project 非永続) → apply() は呼ばない
    connect(m_solver, &QComboBox::currentIndexChanged,
            this, &AcousticTab::updateSolverView);
    updateSolverView();

    connect(project, &Project::loaded, this, &AcousticTab::refresh);
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
    a.micCount = m_micCount->value();
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
    m_micCount->setValue(a.micCount);
    m_updating = false;
}
